/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2014  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include "task.h"
#include <algorithm>
#include <stdexcept>
#include <QUrl>
#include <QFileInfo>
#include <QDataStream>
#include <QDir>
#include <QTimer>
#include <QtDebug>
#include <util/xpc/util.h>
#include <util/sll/qtutil.h>
#include <util/sll/prelude.h>
#include <util/sll/either.h>
#include <util/sll/qstringwrappers.h>
#include <util/threads/futures.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ientitymanager.h>
#include "core.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace CSTP
{
	namespace
	{
		void LateDelete (QNetworkReply *rep)
		{
			if (rep)
			{
				Core::Instance ().RemoveFinishedReply (rep);
				rep->deleteLater ();
			}
		}

		QVariantMap Augment (QVariantMap map, const QList<QPair<QString, QVariant>>& pairs)
		{
			if (pairs.isEmpty ())
				return map;

			const auto& keys = Util::Map (Util::Stlize (map),
					[] (const std::pair<QString, QVariant>& p) { return std::move (p.first).trimmed (); });
			for (const auto& pair : pairs)
				if (!keys.contains (pair.first, Qt::CaseInsensitive))
					map [pair.first] = pair.second;

			return map;
		}
	}

	Task::Task (const QUrl& url, const QVariantMap& params)
	: Reply_ (nullptr, &LateDelete)
	, URL_ (url)
	, Timer_ (new QTimer (this))
	, Referer_ (params ["Referer"].toUrl ())
	, Operation_ (static_cast<QNetworkAccessManager::Operation> (params
				.value ("Operation", QNetworkAccessManager::GetOperation).toInt ()))
	, Headers_ (Augment (params.value ("HttpHeaders").toMap (),
			{
				{ "Content-Type", "application/x-www-form-urlencoded" }
			}))
	, UploadData_ (params.value ("UploadData").toByteArray ())
	{
		StartTime_.start ();

		connect (Timer_,
				SIGNAL (timeout ()),
				this,
				SIGNAL (updateInterface ()));
	}

	Task::Task (QNetworkReply *reply)
	: Reply_ (reply, &LateDelete)
	, Timer_ (new QTimer (this))
	, Operation_ (reply->operation ())
	, Headers_
	{
		{
			"Content-Type",
			reply->request ().header (QNetworkRequest::ContentTypeHeader).toByteArray ()
		}
	}
	{
		StartTime_.start ();

		connect (Timer_,
				SIGNAL (timeout ()),
				this,
				SIGNAL (updateInterface ()));

		Promise_.reportStarted ();
	}

	void Task::Start (const std::shared_ptr<QFile>& tof)
	{
		FileSizeAtStart_ = tof->size ();
		To_ = tof;

		if (!Reply_)
		{
			if (URL_.scheme () == "file")
			{
				QTimer::singleShot (100,
						this,
						SLOT (handleLocalTransfer ()));
				return;
			}

			auto ua = XmlSettingsManager::Instance ().property ("UserUserAgent").toString ();
			if (ua.isEmpty ())
				ua = XmlSettingsManager::Instance ().property ("PredefinedUserAgent").toString ();

			if (ua == "%leechcraft%")
				ua = "LeechCraft.CSTP/" + Core::Instance ().GetCoreProxy ()->GetVersion ();

			QNetworkRequest req { URL_ };
			if (tof->size ())
				req.setRawHeader ("Range", QString ("bytes=%1-").arg (tof->size ()).toLatin1 ());
			req.setRawHeader ("User-Agent", ua.toLatin1 ());

			if (Referer_.isEmpty ())
				req.setRawHeader ("Referer", QString (QString ("http://") + URL_.host ()).toLatin1 ());
			else
				req.setRawHeader ("Referer", Referer_.toEncoded ());

			req.setRawHeader ("Host", URL_.host ().toLatin1 ());
			req.setRawHeader ("Origin", URL_.scheme ().toLatin1 () + "://" + URL_.host ().toLatin1 ());
			req.setRawHeader ("Accept", "*/*");

			StartTime_.restart ();

			for (const auto& pair : Util::Stlize (Headers_))
				req.setRawHeader (pair.first.toLatin1 (), pair.second.toByteArray ());

			auto nam = Core::Instance ().GetNetworkAccessManager ();
			switch (Operation_)
			{
			case QNetworkAccessManager::GetOperation:
				Reply_.reset (nam->get (req));
				break;
			case QNetworkAccessManager::PostOperation:
				Reply_.reset (nam->post (req, UploadData_));
				break;
			default:
				qWarning () << Q_FUNC_INFO
						<< "unsupported operation";
				handleError ();
				return;
			}
		}
		else
		{
			handleMetaDataChanged ();

			qint64 contentLength = Reply_->header (QNetworkRequest::ContentLengthHeader).toInt ();
			if (contentLength &&
					Reply_->bytesAvailable () == contentLength)
			{
				handleReadyRead ();
				handleFinished ();
				return;
			}
			else if (!Reply_->isOpen ())
			{
				handleError ();
				return;
			}
			else if (handleReadyRead ())
				return;
		}

		if (!Timer_->isActive ())
			Timer_->start (3000);

		Reply_->setParent (nullptr);
		connect (Reply_.get (),
				SIGNAL (downloadProgress (qint64, qint64)),
				this,
				SLOT (handleDataTransferProgress (qint64, qint64)));
		connect (Reply_.get (),
				SIGNAL (finished ()),
				this,
				SLOT (handleFinished ()));
		connect (Reply_.get (),
				SIGNAL (error (QNetworkReply::NetworkError)),
				this,
				SLOT (handleError ()));
		connect (Reply_.get (),
				SIGNAL (metaDataChanged ()),
				this,
				SLOT (handleMetaDataChanged ()));
		connect (Reply_.get (),
				SIGNAL (readyRead ()),
				this,
				SLOT (handleReadyRead ()));
	}

	void Task::Stop ()
	{
		if (Reply_)
			Reply_->abort ();
	}

	void Task::ForbidNameChanges ()
	{
		CanChangeName_ = false;
	}

	QByteArray Task::Serialize () const
	{
		QByteArray result;
		{
			QDataStream out (&result, QIODevice::WriteOnly);
			out << 2
				<< URL_
				<< StartTime_
				<< Done_
				<< Total_
				<< Speed_
				<< CanChangeName_;
		}
		return result;
	}

	void Task::Deserialize (QByteArray& data)
	{
		QDataStream in (&data, QIODevice::ReadOnly);
		int version = 0;
		in >> version;
		if (version < 1 || version > 2)
			throw std::runtime_error ("Unknown version");

		in >> URL_
			>> StartTime_
			>> Done_
			>> Total_
			>> Speed_;

		if (version >= 2)
			in >> CanChangeName_;
	}

	double Task::GetSpeed () const
	{
		return Speed_;
	}

	qint64 Task::GetDone () const
	{
		return Done_;
	}

	qint64 Task::GetTotal () const
	{
		return Total_;
	}

	QString Task::GetState () const
	{
		if (!Reply_)
			return tr ("Stopped");
		else if (Done_ == Total_)
			return tr ("Finished");
		else
			return tr ("Running");
	}

	QString Task::GetURL () const
	{
		return Reply_ ? Reply_->url ().toString () : URL_.toString ();
	}

	int Task::GetTimeFromStart () const
	{
		return StartTime_.elapsed ();
	}

	bool Task::IsRunning () const
	{
		return Reply_ && !URL_.isEmpty ();
	}

	QString Task::GetErrorString () const
	{
		return Reply_ ? Reply_->errorString () : tr ("Task isn't initialized properly");
	}

	QFuture<IDownload::Result> Task::GetFuture ()
	{
		return Promise_.future ();
	}

	void Task::Reset ()
	{
		RedirectHistory_.clear ();
		Done_ = -1;
		Total_ = 0;
		Speed_ = 0;
		FileSizeAtStart_ = -1;
		Reply_.reset ();
	}

	void Task::RecalculateSpeed ()
	{
		Speed_ = static_cast<double> (Done_ * 1000) / static_cast<double> (StartTime_.elapsed ());
	}

	void Task::HandleMetadataRedirection ()
	{
		const auto& newUrl = Reply_->rawHeader ("Location");
		if (!newUrl.size ())
			return;

		const auto code = Reply_->attribute (QNetworkRequest::HttpStatusCodeAttribute).toInt ();
		if (code > 399 || code < 300)
		{
			qDebug () << Q_FUNC_INFO
					<< "there is a redirection URL, but the status code is not 3xx:"
					<< newUrl
					<< code
					<< Reply_->attribute (QNetworkRequest::HttpReasonPhraseAttribute);
			return;
		}

		if (!QUrl { newUrl }.isValid ())
		{
			qWarning () << Q_FUNC_INFO
				<< "invalid redirect URL"
				<< newUrl
				<< "for"
				<< Reply_->url ();
		}
		else if (RedirectHistory_.contains (newUrl))
		{
			qWarning () << Q_FUNC_INFO
				<< "redir loop detected"
				<< newUrl
				<< "for"
				<< Reply_->url ();
			handleError ();
		}
		else
		{
			RedirectHistory_ << newUrl;

			disconnect (Reply_.get (),
					0,
					this,
					0);

			QMetaObject::invokeMethod (this,
					"redirectedConstruction",
					Qt::QueuedConnection,
					Q_ARG (QByteArray, newUrl));
		}
	}

	namespace
	{
		QString GetFilenameAscii (const QString& contdis)
		{
			const QByteArray start { "filename=" };
			int startPos = contdis.indexOf (start) + start.size ();
			bool ignoreNextQuote = false;
			QString result;
			while (startPos < contdis.size ())
			{
				const auto cur = contdis.at (startPos++);
				if (cur == '\\')
				{
					ignoreNextQuote = true;
					continue;
				}

				result += cur;

				if (cur == '"' &&
						!ignoreNextQuote &&
						result.size () != 1)
					break;

				ignoreNextQuote = false;
			}
			return result;
		}

		QString GetFilenameUtf8 (const QString& contdis)
		{
			const QByteArray start = "filename*=UTF-8''";
			const auto markerPos = contdis.indexOf (start);
			if (markerPos == -1)
				return {};

			const auto startPos = markerPos + start.size ();
			auto endPos = contdis.indexOf (';', startPos);
			if (endPos == -1)
				endPos = contdis.size ();

			const auto& encoded = contdis.mid (startPos, endPos - startPos);
			const auto& utf8 = QByteArray::fromPercentEncoding (encoded.toLatin1 ());

			const auto& result = QString::fromUtf8 (utf8);
			return result;
		}

		QString GetFilename (const QString& contdis)
		{
			auto result = GetFilenameUtf8 (contdis);
			if (result.isEmpty ())
				result = GetFilenameAscii (contdis);

			if (result.startsWith ('"') && result.endsWith ('"'))
				result = result.mid (1, result.size () - 2);

			return result;
		}
	}

	void Task::HandleMetadataFilename ()
	{
		if (!CanChangeName_)
			return;

		const auto& contdis = Reply_->rawHeader ("Content-Disposition");
		qDebug () << Q_FUNC_INFO << contdis;
		if (!contdis.contains ("filename="))
			return;

		const auto& result = GetFilename (contdis);

		if (result.isEmpty ())
			return;

		auto path = To_->fileName ();
		auto oldPath = path;
		auto fname = QFileInfo (path).fileName ();
		path.replace (path.lastIndexOf (fname),
				fname.size (), result);

		if (path == oldPath)
		{
			qDebug () << Q_FUNC_INFO
					<< "new name equals to the old name, skipping renaming";
			return;
		}

		const auto openMode = To_->openMode ();
		To_->close ();

		if (!To_->rename (path))
		{
			qWarning () << Q_FUNC_INFO
				<< "failed to rename to"
				<< path
				<< To_->errorString ();
		}
		if (!To_->open (openMode))
		{
			qWarning () << Q_FUNC_INFO
				<< "failed to re-open the renamed file"
				<< path;
			To_->rename (oldPath);
			To_->open (openMode);
		}
	}

	void Task::handleDataTransferProgress (qint64 done, qint64 total)
	{
		Done_ = done;
		Total_ = total;

		RecalculateSpeed ();

		if (done == total)
			emit updateInterface ();
	}

	void Task::redirectedConstruction (const QByteArray& newUrl)
	{
		if (To_ && FileSizeAtStart_ >= 0)
		{
			To_->close ();
			To_->resize (FileSizeAtStart_);
			if (!To_->open (QIODevice::ReadWrite))
				qWarning () << Q_FUNC_INFO
						<< "failed to re-open the file after resizing"
						<< To_->fileName ()
						<< "to"
						<< FileSizeAtStart_
						<< ", error:"
						<< To_->errorString ();
		}

		Reply_.reset ();

		Referer_ = URL_;
		URL_ = QUrl::fromEncoded (newUrl);
		Start (To_);
	}

	void Task::handleMetaDataChanged ()
	{
		HandleMetadataRedirection ();
		HandleMetadataFilename ();
	}

	void Task::handleLocalTransfer ()
	{
		const auto& localFile = URL_.toLocalFile ();
		qDebug () << "LOCAL FILE" << localFile << To_->fileName ();
		QFileInfo fi { localFile };
		if (!fi.isFile ())
		{
			qWarning () << Q_FUNC_INFO
					<< URL_
					<< "is not a file";
			QTimer::singleShot (0,
					this,
					SLOT (handleError ()));
			return;
		}

		const auto& destination = To_->fileName ();
		QFile file { localFile };
		To_->close ();
		if (!To_->remove () ||
				!file.copy (destination))
		{
			if (!To_->open (QIODevice::WriteOnly))
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to open destfile"
						<< To_->fileName ()
						<< "for writing";
				QTimer::singleShot (0,
						this,
						SLOT (handleError ()));
				return;
			}

			if (!file.open (QIODevice::ReadOnly))
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to open sourcefile"
						<< file.fileName ()
						<< "for reading";
				QTimer::singleShot (0,
						this,
						SLOT (handleError ()));
				return;
			}

			const int chunkSize = 10 * 1024 * 1024;
			auto chunk = file.read (chunkSize);
			while (chunk.size ())
			{
				To_->write (chunk);
				chunk = file.read (chunkSize);
			}
		}

		QTimer::singleShot (0,
				this,
				SLOT (handleFinished ()));
	}

	bool Task::handleReadyRead ()
	{
		if (Reply_)
		{
			quint64 avail = Reply_->bytesAvailable ();
			quint64 res = To_->write (Reply_->readAll ());
			if (static_cast<quint64> (-1) == res ||
					res != avail)
			{
				qWarning () << Q_FUNC_INFO
						<< "Error writing to file:"
						<< To_->fileName ()
						<< To_->errorString ();

				const auto& errString = tr ("Error writing to file %1: %2")
						.arg (To_->fileName ())
						.arg (To_->errorString ());
				const auto& e = Util::MakeNotification ("LeechCraft CSTP",
						errString,
						Priority::Critical);
				Core::Instance ().GetCoreProxy ()->GetEntityManager ()->HandleEntity (e);
				handleError ();
			}
		}
		if (URL_.isEmpty () &&
				Core::Instance ().HasFinishedReply (Reply_.get ()))
		{
			handleFinished ();
			return true;
		}
		return false;
	}

	void Task::handleFinished ()
	{
		Util::ReportFutureResult (Promise_, IDownload::Result::Right ({}));

		emit done (false);
	}

	void Task::handleError ()
	{
		// TODO don't emit this when the file is already fully downloaded
		Util::ReportFutureResult (Promise_, IDownload::Result::Left ({ IDownload::Error::Type::Unknown, {} }));

		emit done (true);
	}
}
}
