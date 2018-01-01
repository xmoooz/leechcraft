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

#include "albumartfetcher.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QDomDocument>
#include <QStringList>
#include "util.h"

namespace LeechCraft
{
namespace Lastfmscrobble
{
	AlbumArtFetcher::AlbumArtFetcher (const Media::AlbumInfo& albumInfo, ICoreProxy_ptr proxy, QObject *parent)
	: QObject (parent)
	, Proxy_ (proxy)
	, Info_ (albumInfo)
	{
		const QMap<QString, QString> params
		{
			{ "artist", albumInfo.Artist_ },
			{ "album", albumInfo.Album_ },
			{ "autocorrect", "1" }
		};
		auto reply = Request ("album.getInfo", proxy->GetNetworkAccessManager (), params);
		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (handleReplyFinished ()));
	}

	QObject* AlbumArtFetcher::GetQObject ()
	{
		return this;
	}

	Media::AlbumInfo AlbumArtFetcher::GetAlbumInfo () const
	{
		return Info_;
	}

	QList<QUrl> AlbumArtFetcher::GetImageUrls () const
	{
		return { ImageUrl_ };
	}

	QList<QImage> AlbumArtFetcher::GetImages () const
	{
		return { Image_ };
	}

	void AlbumArtFetcher::handleReplyFinished ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		QDomDocument doc;
		if (!doc.setContent (reply->readAll ()))
		{
			emit ready (Info_, {});
			deleteLater ();
			return;
		}

		const auto& elems = doc.elementsByTagName ("image");
		QStringList sizes;
		sizes << "mega"
				<< "extralarge"
				<< "large"
				<< "medium"
				<< "small"
				<< "";
		while (!sizes.isEmpty ())
		{
			const auto& size = sizes.takeFirst ();
			for (int i = 0; i < elems.size (); ++i)
			{
				const auto& elem = elems.at (i).toElement ();
				if (elem.attribute ("size") != size)
					continue;

				const auto& text = elem.text ();
				if (text.isEmpty ())
					continue;

				ImageUrl_ = QUrl (elem.text ());
				emit urlsReady (Info_, { ImageUrl_ });

				QNetworkRequest req (ImageUrl_);
				req.setPriority (QNetworkRequest::LowPriority);
				auto imageReply = Proxy_->GetNetworkAccessManager ()->get (req);
				imageReply->setProperty ("AlbumInfo", reply->property ("AlbumInfo"));
				connect (imageReply,
						SIGNAL (finished ()),
						this,
						SLOT (handleImageReplyFinished ()));
				connect (this,
						SIGNAL (destroyed ()),
						imageReply,
						SLOT (deleteLater ()));
				return;
			}
		}

		emit ready (Info_, {});
		deleteLater ();
	}

	void AlbumArtFetcher::handleImageReplyFinished ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();
		deleteLater ();

		Image_.loadFromData (reply->readAll ());
		emit ready (Info_, { Image_ });
	}
}
}
