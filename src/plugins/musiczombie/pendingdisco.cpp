/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2012  Georg Rudoy
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

#include "pendingdisco.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QDomDocument>
#include <QFuture>
#include <QPointer>
#include <QtDebug>
#include <util/sll/queuemanager.h>
#include <util/util.h>
#include <util/sll/util.h>
#include <util/sll/prelude.h>
#include <util/sll/domchildrenrange.h>
#include <util/threads/futures.h>
#include "artistlookup.h"
#include "util.h"

namespace LeechCraft
{
namespace MusicZombie
{
	namespace
	{
		QString NormalizeRelease (QString title)
		{
			return title
					.remove (' ')
					.remove ('.')
					.toLower ();
		}
	}

	PendingDisco::PendingDisco (Util::QueueManager *queue, const QString& artist, const QString& release,
			const QStringList& hints, QNetworkAccessManager *nam, QObject *parent)
	: QObject (parent)
	, ReleaseName_ (release.toLower ())
	, Hints_ (hints)
	, Queue_ (queue)
	, NAM_ (nam)
	{
		Promise_.reportStarted ();
		Queue_->Schedule ([this, artist, nam]
			{
				auto idLookup = new ArtistLookup (artist, nam, this);
				connect (idLookup,
						SIGNAL(gotID (QString)),
						this,
						SLOT (handleGotID (QString)));
				connect (idLookup,
						SIGNAL (replyError ()),
						this,
						SLOT (handleIDError ()));
				connect (idLookup,
						SIGNAL (networkError ()),
						this,
						SLOT (handleIDError ()));
			}, this);
	}

	QFuture<Media::IDiscographyProvider::QueryResult_t> PendingDisco::GetFuture ()
	{
		return Promise_.future ();
	}

	void PendingDisco::handleGotID (const QString& id)
	{
		static const QString pref { "http://musicbrainz.org/ws/2/release?limit=100&inc=recordings+release-groups&status=official&artist=" };
		const QUrl url { pref + id };

		Queue_->Schedule ([this, url]
			{
				auto reply = NAM_->get (SetupRequest (QNetworkRequest { url }));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleLookupFinished ()));
				connect (reply,
						SIGNAL (error (QNetworkReply::NetworkError)),
						this,
						SLOT (handleLookupError ()));
			}, this);
	}

	void PendingDisco::handleIDError ()
	{
		qWarning () << Q_FUNC_INFO
				<< "error getting MBID";

		Util::ReportFutureResult (Promise_, QueryResult_t::Left (tr ("Error getting artist MBID.")));
		deleteLater ();
	}

	namespace
	{
		auto ParseMediumList (const QDomElement& mediumList)
		{
			return Util::Map (Util::MakeDomChildrenRange (mediumList, "medium"),
					[] (const auto& medium)
					{
						return Util::Map (Util::MakeDomChildrenRange (medium.firstChildElement ("track-list"), "track"),
								[] (const auto& trackElem)
								{
									const int num = trackElem.firstChildElement ("number").text ().toInt ();

									const auto& recElem = trackElem.firstChildElement ("recording");
									const auto& title = recElem.firstChildElement ("title").text ();
									const int length = recElem.firstChildElement ("length").text ().toInt () / 1000;
									return Media::ReleaseTrackInfo { num, title, length };
								});
					});
		}

		Media::ReleaseInfo::Type GetReleaseType (const QDomElement& releaseElem)
		{
			const auto& elem = releaseElem.firstChildElement ("release-group");
			if (elem.isNull ())
			{
				qWarning () << Q_FUNC_INFO
						<< "null element";
				return Media::ReleaseInfo::Type::Other;
			}

			const auto& type = elem.attribute ("type");

			static const QMap<QString, Media::ReleaseInfo::Type> map
			{
				{ "Album", Media::ReleaseInfo::Type::Standard },
				{ "EP", Media::ReleaseInfo::Type::EP },
				{ "Single", Media::ReleaseInfo::Type::Single },
				{ "Compilation", Media::ReleaseInfo::Type::Compilation },
				{ "Live", Media::ReleaseInfo::Type::Live },
				{ "Soundtrack", Media::ReleaseInfo::Type::Soundtrack },
				{ "Other", Media::ReleaseInfo::Type::Other }
			};

			if (map.contains (type))
				return map.value (type);

			qWarning () << Q_FUNC_INFO
					<< "unknown type:"
					<< type;
			return Media::ReleaseInfo::Type::Other;
		}
	}

	void PendingDisco::handleLookupFinished ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();
		deleteLater ();

		const auto& data = reply->readAll ();
		QDomDocument doc;
		if (!doc.setContent (data))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to parse"
					<< data;
			Util::ReportFutureResult (Promise_, QueryResult_t::Left (tr ("Unable to parse MusicBrainz reply.")));
			return;
		}

		QMap<QString, QMap<QString, Media::ReleaseInfo>> infos;

		const auto& releaseList = doc.documentElement ().firstChildElement ("release-list");
		for (const auto& releaseElem : Util::MakeDomChildrenRange (releaseList, "release"))
		{
			auto elemText = [&releaseElem] (const QString& sub)
			{
				return releaseElem.firstChildElement (sub).text ();
			};

			if (elemText ("status") != "Official")
				continue;

			const auto& dateStr = elemText ("date");
			const int dashPos = dateStr.indexOf ('-');
			const int date = (dashPos > 0 ? dateStr.left (dashPos) : dateStr).toInt ();
			if (date < 1000)
				continue;

			const auto& title = elemText ("title");
			if (!ReleaseName_.isEmpty () && title.toLower () != ReleaseName_)
				continue;

			Media::ReleaseInfo info
			{
				releaseElem.attribute ("id"),
				title,
				date,
				GetReleaseType (releaseElem),
				ParseMediumList (releaseElem.firstChildElement ("medium-list"))
			};

			infos [title] [elemText ("country")] = info;
		}

		QList<Media::ReleaseInfo> releases;
		for (const auto& countries : infos)
		{
			Media::ReleaseInfo release;
			if (countries.contains ("US"))
				release = countries ["US"];
			else if (!countries.isEmpty ())
				release = countries.begin ().value ();

			releases << release;
		}

		std::sort (releases.begin (), releases.end (),
				Util::ComparingBy (&Media::ReleaseInfo::Year_));
		Util::ReportFutureResult (Promise_, QueryResult_t::Right (releases));
	}

	void PendingDisco::handleLookupError ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		qWarning () << Q_FUNC_INFO
				<< "error looking stuff up"
				<< reply->errorString ();
		Util::ReportFutureResult (Promise_,
				QueryResult_t::Left (tr ("Error performing artist lookup: %1.")
						.arg (reply->errorString ())));
		deleteLater ();
	}
}
}
