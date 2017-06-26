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

#include "pendingtagsfetch.h"
#include <functional>
#include <QtConcurrentRun>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDomDocument>
#include <util/sll/queuemanager.h>
#include <util/sll/urloperator.h>
#include <util/threads/futures.h>
#include "chroma.h"

namespace LeechCraft
{
namespace MusicZombie
{
	const QString ApiKey = "rBE9CXpr";

	PendingTagsFetch::PendingTagsFetch (Util::QueueManager *queue,
			QNetworkAccessManager *nam, const QString& filename)
	: Queue_ (queue)
	, NAM_ (nam)
	, Filename_ (filename)
	{
		auto worker = [filename]
		{
			Chroma chroma;
			try
			{
				return chroma (filename);
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< e.what ();
				return Chroma::Result ();
			}
		};

		Util::Sequence (this, QtConcurrent::run (worker)) >>
				[this] (const Chroma::Result& result)
				{
					const auto& fp = result.FP_;
					if (fp.isEmpty ())
					{
						emit ready (Filename_, {});
						deleteLater ();
					}

					Queue_->Schedule ([this, result] { Request (result.FP_, result.Duration_); }, this);
				};
	}

	QObject* PendingTagsFetch::GetQObject ()
	{
		return this;
	}

	Media::AudioInfo PendingTagsFetch::GetResult () const
	{
		return Info_;
	}

	void PendingTagsFetch::Request (const QByteArray& fp, int duration)
	{
		QUrl url ("http://api.acoustid.org/v2/lookup");
		Util::UrlOperator { url }
				("client", ApiKey)
				("duration", QString::number (duration))
				("fingerprint", QString::fromLatin1 (fp))
				("meta", "recordings+releasegroups+releases+tracks+sources+usermeta")
				("format", "xml");

		auto reply = NAM_->get (QNetworkRequest (url));
		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (handleReplyFinished ()));
	}

	void PendingTagsFetch::handleReplyFinished ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();
		deleteLater ();

		const auto& data = reply->readAll ();
		QDomDocument doc;
		if (!doc.setContent (data))
		{
			qWarning () << Q_FUNC_INFO
					<< "error parsing"
					<< data;
			emit ready (Filename_, Media::AudioInfo ());
			return;
		}

		const auto& result = doc.documentElement ()
				.firstChildElement ("results")
				.firstChildElement ("result");
		if (result.isNull ())
		{
			qWarning () << Q_FUNC_INFO
					<< "no results for"
					<< Filename_;
			emit ready (Filename_, Media::AudioInfo ());
			deleteLater ();
			return;
		}

		Media::AudioInfo info;

		const auto& recordingElem = result
				.firstChildElement ("recordings")
				.firstChildElement ("recording");

		info.Title_ = recordingElem.firstChildElement ("title").text ();

		const auto& releaseGroupElem = recordingElem
				.firstChildElement ("releasegroups")
				.firstChildElement ("releasegroup");
		info.Album_ = releaseGroupElem
				.firstChildElement ("title").text ();

		const auto& releaseElem = releaseGroupElem
				.firstChildElement ("releases")
				.firstChildElement ("release");

		const auto& dateElem = releaseElem.firstChildElement ("date");
		info.Year_ = dateElem.firstChildElement ("year").text ().toInt ();

		auto trackElem = releaseElem
				.firstChildElement ("mediums")
				.firstChildElement ("medium")
				.firstChildElement ("tracks")
				.firstChildElement ("track");
		while (!trackElem.isNull ())
		{
			info.TrackNumber_ = trackElem.firstChildElement ("position").text ().toInt ();
			if (trackElem.firstChildElement ("title").text () == info.Title_)
				break;

			trackElem = trackElem.nextSiblingElement ("track");
		}

		QStringList artists;
		auto artistElem = recordingElem
				.firstChildElement ("artists")
				.firstChildElement ("artist");
		while (!artistElem.isNull ())
		{
			artists << artistElem.firstChildElement ("name").text ();
			artistElem = artistElem.nextSiblingElement ("artist");
		}
		info.Artist_ = artists.join (" feat ");

		emit ready (Filename_, info);
		deleteLater ();
	}
}
}
