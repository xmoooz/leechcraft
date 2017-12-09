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

#include "bioviewmanager.h"
#include <numeric>
#include <QQuickWidget>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QtConcurrentRun>
#include <QFutureWatcher>
#include <QStandardItemModel>
#include <util/util.h>
#include <util/qml/colorthemeproxy.h>
#include <util/qml/themeimageprovider.h>
#include <util/sys/paths.h>
#include <util/models/rolenamesmixin.h>
#include <util/sll/slotclosure.h>
#include <util/sll/prelude.h>
#include <util/sll/visitor.h>
#include <util/threads/futures.h>
#include <interfaces/media/idiscographyprovider.h>
#include <interfaces/media/ialbumartprovider.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/iiconthememanager.h>
#include "biopropproxy.h"
#include "core.h"
#include "util.h"
#include "previewhandler.h"
#include "stdartistactionsmanager.h"
#include "localcollection.h"

namespace LeechCraft
{
namespace LMP
{
	namespace
	{
		class DiscoModel : public Util::RoleNamesMixin<QStandardItemModel>
		{
		public:
			enum Roles
			{
				AlbumName = Qt::UserRole + 1,
				AlbumYear,
				AlbumImage,
				AlbumTrackListTooltip
			};

			DiscoModel (QObject *parent)
			: RoleNamesMixin<QStandardItemModel> (parent)
			{
				QHash<int, QByteArray> roleNames;
				roleNames [Roles::AlbumName] = "albumName";
				roleNames [Roles::AlbumYear] = "albumYear";
				roleNames [Roles::AlbumImage] = "albumImage";
				roleNames [Roles::AlbumTrackListTooltip] = "albumTrackListTooltip";
				setRoleNames (roleNames);
			}
		};

		const int AASize = 170;
	}

	BioViewManager::BioViewManager (const ICoreProxy_ptr& proxy,
			QQuickWidget *view, QObject *parent)
	: QObject (parent)
	, View_ (view)
	, BioPropProxy_ (new BioPropProxy (this))
	, DiscoModel_ (new DiscoModel (this))
	, Proxy_ (proxy)
	{
		View_->rootContext ()->setContextObject (BioPropProxy_);
		View_->rootContext ()->setContextProperty ("artistDiscoModel", DiscoModel_);
		View_->rootContext ()->setContextProperty ("colorProxy",
				new Util::ColorThemeProxy (proxy->GetColorThemeManager (), this));
		View_->engine ()->addImageProvider ("ThemeIcons", new Util::ThemeImageProvider (proxy));

		for (const auto& cand : Util::GetPathCandidates (Util::SysPath::QML, ""))
			View_->engine ()->addImportPath (cand);
	}

	void BioViewManager::InitWithSource ()
	{
		connect (View_->rootObject (),
				SIGNAL (albumPreviewRequested (int)),
				this,
				SLOT (handleAlbumPreviewRequested (int)));

		new StdArtistActionsManager (Proxy_, View_, this);
	}

	void BioViewManager::Request (Media::IArtistBioFetcher *fetcher, const QString& artist, const QStringList& releases)
	{
		DiscoModel_->clear ();
		BioPropProxy_->SetBio ({});

		CurrentArtist_ = artist;

		auto pending = fetcher->RequestArtistBio (CurrentArtist_);
		connect (pending->GetQObject (),
				SIGNAL (ready ()),
				this,
				SLOT (handleBioReady ()));

		auto pm = Core::Instance ().GetProxy ()->GetPluginsManager ();
		for (auto prov : pm->GetAllCastableTo<Media::IDiscographyProvider*> ())
			Util::Sequence (this, prov->GetDiscography (CurrentArtist_, releases)) >>
					Util::Visitor
					{
						[artist] (const QString&) { qWarning () << Q_FUNC_INFO << "error for" << artist; },
						[this] (const auto& releases) { HandleDiscographyReady (releases); }
					};
	}

	QStandardItem* BioViewManager::FindAlbumItem (const QString& albumName) const
	{
		for (int i = 0, rc = DiscoModel_->rowCount (); i < rc; ++i)
		{
			auto item = DiscoModel_->item (i);
			const auto& itemData = item->data (DiscoModel::Roles::AlbumName);
			if (itemData.toString () == albumName)
				return item;
		}
		return 0;
	}

	bool BioViewManager::QueryReleaseImageLocal (const Media::AlbumInfo& info) const
	{
		const auto coll = Core::Instance ().GetLocalCollection ();
		const auto albumId = coll->FindAlbum (info.Artist_, info.Album_);
		if (albumId == -1)
			return false;

		const auto& album = coll->GetAlbum (albumId);
		if (!album)
			return false;

		const auto& path = album->CoverPath_;
		if (path.isEmpty () || !QFile::exists (path))
			return false;

		SetAlbumImage (info.Album_, QUrl::fromLocalFile (path));
		return true;
	}

	void BioViewManager::QueryReleaseImage (Media::IAlbumArtProvider *aaProv, const Media::AlbumInfo& info)
	{
		if (QueryReleaseImageLocal (info))
			return;

		const auto proxy = aaProv->RequestAlbumArt (info);
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[this, proxy]
			{
				const auto proxyObj = proxy->GetQObject ();
				proxyObj->deleteLater ();

				const auto& info = proxy->GetAlbumInfo ();
				const auto& images = proxy->GetImageUrls ();
				if (info.Artist_ != CurrentArtist_ || images.isEmpty ())
					return;

				SetAlbumImage (info.Album_, images.first ());
			},
			proxy->GetQObject (),
			SIGNAL (urlsReady (Media::AlbumInfo, QList<QUrl>)),
			this
		};
	}

	void BioViewManager::SetAlbumImage (const QString& album, const QUrl& img) const
	{
		auto item = FindAlbumItem (album);
		if (!item)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown item for"
					<< album;
			return;
		}

		item->setData (img, DiscoModel::Roles::AlbumImage);
	}

	void BioViewManager::handleBioReady ()
	{
		auto pending = qobject_cast<Media::IPendingArtistBio*> (sender ());
		const auto& bio = pending->GetArtistBio ();
		BioPropProxy_->SetBio (bio);

		emit gotArtistImage (bio.BasicInfo_.Name_, bio.BasicInfo_.LargeImage_);
	}

	void BioViewManager::HandleDiscographyReady (QList<Media::ReleaseInfo> releases)
	{
		const auto pm = Core::Instance ().GetProxy ()->GetPluginsManager ();
		const auto aaProvObj = pm->GetAllCastableRoots<Media::IAlbumArtProvider*> ().value (0);
		const auto aaProv = qobject_cast<Media::IAlbumArtProvider*> (aaProvObj);

		const auto& icon = Core::Instance ().GetProxy ()->GetIconThemeManager ()->
				GetIcon ("media-optical").pixmap (AASize * 2, AASize * 2);

		std::sort (releases.rbegin (), releases.rend (),
				Util::ComparingBy (&Media::ReleaseInfo::Year_));
		for (const auto& release : releases)
		{
			if (FindAlbumItem (release.Name_))
				continue;

			auto item = new QStandardItem;
			item->setData (release.Name_, DiscoModel::Roles::AlbumName);
			item->setData (QString::number (release.Year_), DiscoModel::Roles::AlbumYear);
			item->setData (Util::GetAsBase64Src (icon.toImage ()), DiscoModel::Roles::AlbumImage);

			item->setData (MakeTrackListTooltip (release.TrackInfos_),
					DiscoModel::Roles::AlbumTrackListTooltip);

			Album2Tracks_ << Util::Concat (release.TrackInfos_);

			DiscoModel_->appendRow (item);

			QueryReleaseImage (aaProv, { CurrentArtist_, release.Name_ });
		}
	}

	void BioViewManager::handleAlbumPreviewRequested (int index)
	{
		QList<QPair<QString, int>> tracks;
		for (const auto& track : Album2Tracks_.at (index))
			tracks.push_back ({ track.Name_, track.Length_ });

		const auto& album = DiscoModel_->item (index)->data (DiscoModel::Roles::AlbumName).toString ();

		auto ph = Core::Instance ().GetPreviewHandler ();
		ph->previewAlbum (CurrentArtist_, album, tracks);
	}
}
}
