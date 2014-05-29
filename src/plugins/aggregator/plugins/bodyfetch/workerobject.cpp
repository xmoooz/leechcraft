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

#include "workerobject.h"
#include <QUrl>
#include <QFile>
#include <QWebPage>
#include <QWebFrame>
#include <QWebElementCollection>
#include <QTextCodec>
#include <QTimer>
#include <QApplication>
#include <QtDebug>
#include <interfaces/iscriptloader.h>
#include <util/util.h>
#include <util/sys/paths.h>

uint qHash (IScript_ptr script)
{
	return qHash (script.get ());
}

namespace LeechCraft
{
namespace Aggregator
{
namespace BodyFetch
{
	const int CacheValidity = 20;

	WorkerObject::WorkerObject (QObject *parent)
	: QObject (parent)
	, Inst_ (0)
	, IsProcessing_ (false)
	, RecheckScheduled_ (false)
	, StorageDir_ (Util::CreateIfNotExists ("aggregator/bodyfetcher/storage"))
	{
		QTimer *timer = new QTimer;
		connect (timer,
				SIGNAL (timeout ()),
				this,
				SLOT (clearCaches ()));
		timer->start (10000);
	}

	void WorkerObject::SetLoaderInstance (IScriptLoaderInstance *inst)
	{
		Inst_ = inst;
	}

	bool WorkerObject::IsOk () const
	{
		return Inst_;
	}

	void WorkerObject::AppendItems (const QVariantList& items)
	{
		Items_ << items;

		QTimer::singleShot (500,
				this,
				SLOT (process ()));
	}

	void WorkerObject::ProcessItems (const QVariantList& items)
	{
		if (!Inst_)
		{
			qWarning () << Q_FUNC_INFO
					<< "null instance loader, aborting";
			return;
		}

		if (EnumeratedCache_.isEmpty ())
			EnumeratedCache_ = Inst_->EnumerateScripts ();

		QHash<QString, IScript_ptr> channel2script;

		Q_FOREACH (const QVariant& item, items)
		{
			const QVariantMap& map = item.toMap ();

			const QString& channelLinkStr = map ["ChannelLink"].toString ();

			IScript_ptr script = channel2script.value (channelLinkStr);
			if (!script)
			{
				script = GetScriptForChannel (channelLinkStr);
				if (!script)
					continue;
				channel2script [channelLinkStr] = script;
			}

			QVariantList args;
			args << map ["ItemLink"];
			args << map ["ItemCommentsPageLink"];
			args << map ["ItemDescription"];
			QString fetchStr = script->InvokeMethod ("GetFullURL", args).toString ();
			if (fetchStr.isEmpty ())
				fetchStr = map ["ItemLink"].toString ();

			qDebug () << Q_FUNC_INFO << fetchStr << "using" << ChannelLink2ScriptID_ [channelLinkStr];

			const QUrl& url = QUrl::fromEncoded (fetchStr.toUtf8 ());
			URL2Script_ [url] = script;
			URL2ItemID_ [url] = map ["ItemID"].value<quint64> ();
			emit downloadRequested (url);
		}
	}

	IScript_ptr WorkerObject::GetScriptForChannel (const QString& channel)
	{
		if (CachedScripts_.contains (channel))
			return CachedScripts_ [channel];

		IScript_ptr script;
		if (ChannelLink2ScriptID_.contains (channel))
		{
			script = Inst_->LoadScript (ChannelLink2ScriptID_ [channel]);
			if (!script ||
					!script->InvokeMethod ("CanHandle", QVariantList () << channel).toBool ())
			{
				ChannelLink2ScriptID_.remove (channel);
				script.reset ();
			}
		}

		if (!ChannelLink2ScriptID_.contains (channel))
		{
			const QString& scriptId = FindScriptForChannel (channel);
			if (scriptId.isEmpty ())
			{
				CachedScripts_ [channel] = IScript_ptr ();
				return IScript_ptr ();
			}

			ChannelLink2ScriptID_ [channel] = scriptId;
		}

		if (ChannelLink2ScriptID_ [channel].isEmpty ())
		{
			ChannelLink2ScriptID_.remove (channel);
			CachedScripts_ [channel] = IScript_ptr ();
			return IScript_ptr ();
		}

		if (!script)
			script = Inst_->LoadScript (ChannelLink2ScriptID_ [channel]);

		CachedScripts_ [channel] = script;

		return script;
	}

	QString WorkerObject::FindScriptForChannel (const QString& link)
	{
		Q_FOREACH (const QString& id, EnumeratedCache_)
		{
			IScript_ptr script (Inst_->LoadScript (id));
			if (script->InvokeMethod ("CanHandle", QVariantList () << link).toBool ())
				return id;
		}

		return QString ();
	}

	namespace
	{
		QStringList GetReplacements (IScript_ptr script, const QString& method)
		{
			const QVariant& var = script->InvokeMethod (method, QVariantList ());

			QStringList result;
			Q_FOREACH (const QVariant& varItem, var.toList ())
				result << varItem.toString ();

			result.removeAll (QString ());
			result.removeDuplicates ();

			return result;
		}

		template<typename Func>
		QString ParseWithSelectors (QWebFrame *frame,
				const QStringList& selectors,
				int amount,
				Func func)
		{
			QString result;

			Q_FOREACH (const QString& sel, selectors)
			{
				QWebElementCollection col = frame->findAllElements (sel);
				for (int i = 0, size = std::min (amount, col.count ());
						i < size; ++i)
					result += func (col.at (i)).simplified ();

				qApp->processEvents ();
			}

			return result;
		}
	}

	QString WorkerObject::Parse (const QString& contents, IScript_ptr script)
	{
		const QStringList& firstTagOut = GetReplacements (script, "KeepFirstTag");
		const QStringList& allTagsOut = GetReplacements (script, "KeepAllTags");
		const QStringList& firstTagIn = GetReplacements (script, "KeepFirstTagInnerXml");

		qApp->processEvents ();

		if (firstTagOut.isEmpty () &&
				allTagsOut.isEmpty () &&
				firstTagIn.isEmpty ())
			return script->InvokeMethod ("Strip", QVariantList () << contents).toString ();

		QWebPage page;
		page.settings ()->setAttribute (QWebSettings::DeveloperExtrasEnabled, false);
		page.settings ()->setAttribute (QWebSettings::JavascriptEnabled, false);
		page.settings ()->setAttribute (QWebSettings::AutoLoadImages, false);
		page.settings ()->setAttribute (QWebSettings::PluginsEnabled, false);
		page.mainFrame ()->setHtml (contents);

		qApp->processEvents ();

		QString result;
		result += ParseWithSelectors (page.mainFrame (),
				firstTagOut, 1, [] (const QWebElement& e) { return e.toOuterXml (); });
		result += ParseWithSelectors (page.mainFrame (),
				allTagsOut, 1000, [] (const QWebElement& e) { return e.toOuterXml (); });
		result += ParseWithSelectors (page.mainFrame (),
				firstTagIn, 1, [] (const QWebElement& e) { return e.toInnerXml (); });

		result.remove ("</br>");

		return result;
	}

	void WorkerObject::WriteFile (const QString& contents, quint64 itemId) const
	{
		QDir dir = StorageDir_;
		dir.cd (QString::number (itemId % 10));

		QFile file (dir.filePath (QString ("%1.html").arg (itemId)));
		if (!file.open (QIODevice::WriteOnly | QIODevice::Truncate))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open file"
					<< file.fileName ()
					<< file.errorString ();
			return;
		}

		file.write (contents.toUtf8 ());
	}

	QString WorkerObject::Recode (const QByteArray& rawContents) const
	{
		const QByteArray stupidCharset ("meta charset=");
		const int stupidPos = rawContents.indexOf (stupidCharset);

		if (stupidPos >= 0)
		{
			const int begin = stupidPos + stupidCharset.size ();
			const char sep = rawContents.at (begin);
			if (sep == '\'' || sep == '"')
			{
				const int end = rawContents.indexOf (sep, begin + 1);

				const QByteArray& enca = rawContents.mid (begin + 1, end - begin - 1);
				qDebug () << "detected encoding" << enca;
				QTextCodec *codec = QTextCodec::codecForName (enca);
				if (codec)
					return codec->toUnicode (rawContents);
				else
					qWarning () << Q_FUNC_INFO
							<< "unable to get codec for"
							<< enca;
			}
		}

		QTextCodec *codec = QTextCodec::codecForHtml (rawContents, 0);
		return codec ?
				codec->toUnicode (rawContents) :
				QString::fromUtf8 (rawContents);
	}

	namespace
	{
		struct ProcessingGuard
		{
			bool *P_;

			ProcessingGuard (bool *p)
			: P_ (p)
			{
				*P_ = true;
			}

			~ProcessingGuard ()
			{
				*P_ = false;
			}
		};
	}

	void WorkerObject::ScheduleRechecking ()
	{
		if (RecheckScheduled_)
			return;

		QTimer::singleShot (1000,
				this,
				SLOT (recheckFinished ()));

		RecheckScheduled_ = true;
	}

	void WorkerObject::handleDownloadFinished (QUrl url, QString filename)
	{
		if (IsProcessing_)
		{
			FetchedQueue_ << qMakePair (url, filename);
			ScheduleRechecking ();
			return;
		}

		ProcessingGuard pg (&IsProcessing_);

		IScript_ptr script = URL2Script_.take (url);
		if (!script)
		{
			qWarning () << Q_FUNC_INFO
					<< "null script for"
					<< url;
			return;
		}

		QFile file (filename);
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open file";
			file.remove ();
			return;
		}

		const QByteArray& rawContents = file.readAll ();
		qApp->processEvents ();
		const QString& contents = Recode (rawContents);
		file.close ();
		file.remove ();
		const QString& result = Parse (contents, script);

		if (result.isEmpty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "empty result for"
					<< url;
			return;
		}

		const quint64 id = URL2ItemID_.take (url);
		qApp->processEvents ();
		WriteFile (result, id);
		qApp->processEvents ();
		emit newBodyFetched (id);
	}

	void WorkerObject::recheckFinished ()
	{
		RecheckScheduled_ = false;

		if (FetchedQueue_.isEmpty ())
			return;

		if (IsProcessing_)
			ScheduleRechecking ();

		const QPair<QUrl, QString>& item = FetchedQueue_.takeFirst ();
		handleDownloadFinished (item.first, item.second);
	}

	void WorkerObject::process ()
	{
		if (Items_.isEmpty ())
			return;

		if (!IsProcessing_)
			ProcessItems (QVariantList () << Items_.takeFirst ());

		if (!Items_.isEmpty ())
			QTimer::singleShot (400,
					this,
					SLOT (process ()));
	}

	void WorkerObject::clearCaches ()
	{
		if (IsProcessing_)
			return;

		EnumeratedCache_.clear ();
		CachedScripts_.clear ();
	}
}
}
}
