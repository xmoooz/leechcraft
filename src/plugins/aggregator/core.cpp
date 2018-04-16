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

#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <QtDebug>
#include <QImage>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QTextCodec>
#include <QXmlStreamWriter>
#include <QNetworkReply>
#include <interfaces/iwebbrowser.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/itagsmanager.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/ientitymanager.h>
#include <util/models/mergemodel.h>
#include <util/xpc/util.h>
#include <util/sys/fileremoveguard.h>
#include <util/sys/paths.h>
#include <util/xpc/defaulthookproxy.h>
#include <util/shortcuts/shortcutmanager.h>
#include <util/sll/prelude.h>
#include <util/sll/qtutil.h>
#include "core.h"
#include "xmlsettingsmanager.h"
#include "parserfactory.h"
#include "rss20parser.h"
#include "rss10parser.h"
#include "rss091parser.h"
#include "atom10parser.h"
#include "atom03parser.h"
#include "channelsmodel.h"
#include "opmlparser.h"
#include "opmlwriter.h"
#include "sqlstoragebackend.h"
#include "jobholderrepresentation.h"
#include "channelsfiltermodel.h"
#include "importopml.h"
#include "addfeed.h"
#include "itemswidget.h"
#include "pluginmanager.h"
#include "dbupdatethread.h"
#include "dbupdatethreadworker.h"
#include "dumbstorage.h"
#include "storagebackendmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
	Core::Core ()
	{
		qRegisterMetaType<IDType_t> ("IDType_t");
		qRegisterMetaType<QList<IDType_t>> ("QList<IDType_t>");
		qRegisterMetaType<QSet<IDType_t>> ("QSet<IDType_t>");
		qRegisterMetaType<QItemSelection> ("QItemSelection");
		qRegisterMetaType<Item_ptr> ("Item_ptr");
		qRegisterMetaType<Item_cptr> ("Item_cptr");
		qRegisterMetaType<ChannelShort> ("ChannelShort");
		qRegisterMetaType<Channel_ptr> ("Channel_ptr");
		qRegisterMetaType<channels_container_t> ("channels_container_t");
		qRegisterMetaTypeStreamOperators<Feed> ("LeechCraft::Plugins::Aggregator::Feed");
	}

	Core& Core::Instance ()
	{
		static Core core;
		return core;
	}

	void Core::Release ()
	{
		DBUpThread_.reset ();

		delete JobHolderRepresentation_;
		delete ChannelsFilterModel_;
		delete ChannelsModel_;

		StorageBackend_.reset ();

		XmlSettingsManager::Instance ()->Release ();
	}

	void Core::SetProxy (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;
	}

	ICoreProxy_ptr Core::GetProxy () const
	{
		return Proxy_;
	}

	void Core::AddPlugin (QObject *plugin)
	{
		PluginManager_->AddPlugin (plugin);
	}

	Util::IDPool<IDType_t>& Core::GetPool (PoolType type)
	{
		return Pools_ [type];
	}

	bool Core::CouldHandle (const Entity& e)
	{
		if (!e.Entity_.canConvert<QUrl> () ||
				!Initialized_)
			return false;

		const auto& url = e.Entity_.toUrl ();

		if (e.Mime_ == "text/x-opml")
		{
			if (url.scheme () != "file" &&
					url.scheme () != "http" &&
					url.scheme () != "https" &&
					url.scheme () != "itpc")
				return false;
		}
		else if (e.Mime_ == "text/xml")
		{
			if (url.scheme () != "http" &&
					url.scheme () != "https")
				return false;

			const auto& pageData = e.Additional_ ["URLData"].toString ();
			QXmlStreamReader xmlReader (pageData);
			if (!xmlReader.readNextStartElement ())
				return false;
			return xmlReader.name () == "rss" || xmlReader.name () == "atom";
		}
		else
		{
			if (url.scheme () == "feed")
				return true;
			if (url.scheme () == "itpc")
				return true;
			if (url.scheme () != "http" &&
					url.scheme () != "https" &&
					url.scheme () != "itpc")
				return false;

			if (e.Mime_ != "application/atom+xml" &&
					e.Mime_ != "application/rss+xml")
				return false;

			const auto& linkRel = e.Additional_ ["LinkRel"].toString ();
			if (!linkRel.isEmpty () &&
					linkRel != "alternate")
				return false;
		}

		return true;
	}

	void Core::Handle (Entity e)
	{
		QUrl url = e.Entity_.toUrl ();
		if (e.Mime_ == "text/x-opml")
		{
			if (url.scheme () == "file")
				StartAddingOPML (url.toLocalFile ());
			else
			{
				const auto& name = Util::GetTemporaryName ();

				const auto& dlEntity = Util::MakeEntity (url,
						name,
						Internal |
							DoNotNotifyUser |
							DoNotSaveInHistory |
							NotPersistent |
							DoNotAnnounceEntity);

				const auto& handleResult = Proxy_->GetEntityManager ()->DelegateEntity (dlEntity);
				if (!handleResult)
				{
					ErrorNotification (tr ("Import error"),
							tr ("Could not find plugin to download OPML %1.")
								.arg (url.toString ()));
					return;
				}

				HandleProvider (handleResult.Handler_, handleResult.ID_);
				PendingOPMLs_ [handleResult.ID_] = PendingOPML { name };
			}

			const auto& s = e.Additional_;
			if (s.contains ("UpdateOnStartup"))
				XmlSettingsManager::Instance ()->setProperty ("UpdateOnStartup",
						s.value ("UpdateOnStartup").toBool ());
			if (s.contains ("UpdateTimeout"))
				XmlSettingsManager::Instance ()->setProperty ("UpdateInterval",
						s.value ("UpdateTimeout").toInt ());
			if (s.contains ("MaxArticles"))
				XmlSettingsManager::Instance ()->setProperty ("ItemsPerChannel",
						s.value ("MaxArticles").toInt ());
			if (s.contains ("MaxAge"))
				XmlSettingsManager::Instance ()->setProperty ("ItemsMaxAge",
						s.value ("MaxAge").toInt ());
		}
		else
		{
			QString str = url.toString ();
			if (str.startsWith ("feed://"))
				str.replace (0, 4, "http");
			else if (str.startsWith ("feed:"))
				str.remove  (0, 5);
			else if (str.startsWith ("itpc://"))
				str.replace (0, 4, "http");

			class AddFeed af { str };
			if (af.exec () == QDialog::Accepted)
				AddFeed (af.GetURL (),
						af.GetTags ());
		}
	}

	void Core::StartAddingOPML (const QString& file)
	{
		ImportOPML importDialog (file);
		if (importDialog.exec () == QDialog::Rejected)
			return;

		AddFromOPML (importDialog.GetFilename (),
				importDialog.GetTags (),
				importDialog.GetMask ());
	}

	void Core::SetAppWideActions (const AppWideActions& aw)
	{
		AppWideActions_ = aw;
	}

	const AppWideActions& Core::GetAppWideActions () const
	{
		return AppWideActions_;
	}

	bool Core::DoDelayedInit ()
	{
		bool result = true;
		ShortcutMgr_ = new Util::ShortcutManager (Proxy_, this);

		QDir dir = QDir::home ();
		if (!dir.cd (".leechcraft/aggregator") &&
				!dir.mkpath (".leechcraft/aggregator"))
		{
			qCritical () << Q_FUNC_INFO << "could not create necessary "
				"directories for Aggregator";
			result = false;
		}

		ChannelsModel_ = new ChannelsModel ();

		if (!ReinitStorage ())
			result = false;

		ChannelsFilterModel_ = new ChannelsFilterModel ();
		ChannelsFilterModel_->setSourceModel (ChannelsModel_);
		ChannelsFilterModel_->setFilterKeyColumn (0);

		JobHolderRepresentation_ = new JobHolderRepresentation ();

		DBUpThread_ = std::make_shared<DBUpdateThread> (Proxy_);
		DBUpThread_->SetAutoQuit (true);
		DBUpThread_->start (QThread::LowestPriority);
		DBUpThread_->ScheduleImpl (&DBUpdateThreadWorker::WithWorker,
				[this] (DBUpdateThreadWorker *worker)
				{
					connect (worker,
							SIGNAL (gotNewChannel (ChannelShort)),
							this,
							SLOT (handleDBUpGotNewChannel (ChannelShort)),
							Qt::QueuedConnection);
					connect (worker,
							SIGNAL (hookGotNewItems (LeechCraft::IHookProxy_ptr, QList<Item_cptr>)),
							this,
							SIGNAL (hookGotNewItems (LeechCraft::IHookProxy_ptr, QList<Item_cptr>)));
				});

		connect (&StorageBackendManager::Instance (),
				SIGNAL (channelDataUpdated (Channel_ptr)),
				this,
				SLOT (handleChannelDataUpdated (Channel_ptr)),
				Qt::QueuedConnection);

		ParserFactory::Instance ().Register (&RSS20Parser::Instance ());
		ParserFactory::Instance ().Register (&Atom10Parser::Instance ());
		ParserFactory::Instance ().Register (&RSS091Parser::Instance ());
		ParserFactory::Instance ().Register (&Atom03Parser::Instance ());
		ParserFactory::Instance ().Register (&RSS10Parser::Instance ());

		ReprWidget_ = new ItemsWidget ();
		ReprWidget_->SetChannelsFilter (JobHolderRepresentation_);
		ReprWidget_->RegisterShortcuts ();
		ChannelsModel_->SetWidgets (ReprWidget_->GetToolBar (), ReprWidget_);

		JobHolderRepresentation_->setSourceModel (ChannelsModel_);

		CustomUpdateTimer_ = new QTimer (this);
		CustomUpdateTimer_->start (60 * 1000);
		connect (CustomUpdateTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (handleCustomUpdates ()));

		UpdateTimer_ = new QTimer (this);
		UpdateTimer_->setSingleShot (true);
		QDateTime currentDateTime = QDateTime::currentDateTime ();
		QDateTime lastUpdated = XmlSettingsManager::Instance ()->
			Property ("LastUpdateDateTime", currentDateTime).toDateTime ();
		connect (UpdateTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (updateFeeds ()));

		int updateDiff = lastUpdated.secsTo (currentDateTime);
		int interval = XmlSettingsManager::Instance ()->
			property ("UpdateInterval").toInt ();
		if (interval)
		{
			if ((XmlSettingsManager::Instance ()->
						property ("UpdateOnStartup").toBool ()) ||
					(updateDiff > interval * 60))
				QTimer::singleShot (7000,
						this,
						SLOT (updateFeeds ()));
			else
				UpdateTimer_->start (updateDiff * 1000);
		}

		XmlSettingsManager::Instance ()->
			RegisterObject ("UpdateInterval", this, "updateIntervalChanged");
		Initialized_ = true;

		PluginManager_ = new PluginManager ();
		PluginManager_->RegisterHookable (this);

		PluginManager_->RegisterHookable (StorageBackend_.get ());

		return result;
	}

	bool Core::ReinitStorage ()
	{
		Pools_.clear ();
		ChannelsModel_->Clear ();

		StorageBackend_.reset (new DumbStorage);

		const QString& strType = XmlSettingsManager::Instance ()->
				property ("StorageType").toString ();
		try
		{
			StorageBackend_ = StorageBackend::Create (strType);
		}
		catch (const std::runtime_error& s)
		{
			ErrorNotification (tr ("Storage error"),
					QTextCodec::codecForName ("UTF-8")->
					toUnicode (s.what ()));
			return false;
		}
		catch (...)
		{
			ErrorNotification (tr ("Storage error"),
					tr ("Aggregator: general storage initialization error."));
			return false;
		}

		emit storageChanged ();

		const int feedsTable = 1;
		const int channelsTable = 2;
		const int itemsTable = 6;

		if (StorageBackend_->UpdateFeedsStorage (XmlSettingsManager::Instance ()->
				Property (strType + "FeedsTableVersion", feedsTable).toInt (),
				feedsTable))
			XmlSettingsManager::Instance ()->setProperty (qPrintable (strType + "FeedsTableVersion"),
					feedsTable);
		else
			return false;

		if (StorageBackend_->UpdateChannelsStorage (XmlSettingsManager::Instance ()->
				Property (strType + "ChannelsTableVersion", channelsTable).toInt (),
				channelsTable))
			XmlSettingsManager::Instance ()->setProperty (qPrintable (strType + "ChannelsTableVersion"),
					channelsTable);
		else
			return false;

		if (StorageBackend_->UpdateItemsStorage (XmlSettingsManager::Instance ()->
				Property (strType + "ItemsTableVersion", itemsTable).toInt (),
				itemsTable))
			XmlSettingsManager::Instance ()->setProperty (qPrintable (strType + "ItemsTableVersion"),
					itemsTable);
		else
			return false;

		StorageBackend_->Prepare ();

		ids_t feeds;
		StorageBackend_->GetFeedsIDs (feeds);
		for (const auto feedId : feeds)
		{
			channels_shorts_t channels;
			StorageBackend_->GetChannels (channels, feedId);
			for (const auto& chan : channels)
				ChannelsModel_->AddChannel (chan);
		}

		for (int type = 0; type < PTMAX; ++type)
		{
			Util::IDPool<IDType_t> pool;
			pool.SetID (StorageBackend_->GetHighestID (static_cast<PoolType> (type)) + 1);
			Pools_ [static_cast<PoolType> (type)] = pool;
		}

		return true;
	}

	void Core::AddFeed (const QString& url, const QString& tagString)
	{
		AddFeed (url, Proxy_->GetTagsManager ()->Split (tagString));
	}

	void Core::AddFeed (QString url, const QStringList& tags,
			std::shared_ptr<Feed::FeedSettings> fs)
	{
		const auto& fixedUrl = QUrl::fromUserInput (url);
		url = fixedUrl.toString ();
		if (StorageBackend_->FindFeed (url) != static_cast<IDType_t> (-1))
		{
			ErrorNotification (tr ("Feed addition error"),
					tr ("The feed %1 is already added")
					.arg (url));
			return;
		}

		const auto& name = Util::GetTemporaryName ();
		const auto& e = Util::MakeEntity (fixedUrl,
				name,
				Internal |
					DoNotNotifyUser |
					DoNotSaveInHistory |
					NotPersistent |
					DoNotAnnounceEntity);

		const auto& tagIds = Proxy_->GetTagsManager ()->GetIDs (tags);
		PendingJob pj =
		{
			PendingJob::RFeedAdded,
			url,
			name,
			tagIds,
			fs
		};

		const auto& delegateResult = Proxy_->GetEntityManager ()->DelegateEntity (e);
		if (!delegateResult)
		{
			ErrorNotification (tr ("Plugin error"),
					tr ("Could not find plugin to download feed %1.")
						.arg (url),
					false);
			return;
		}

		HandleProvider (delegateResult.Handler_, delegateResult.ID_);
		PendingJobs_ [delegateResult.ID_] = pj;
	}

	void Core::RemoveFeed (const QModelIndex& index)
	{
		if (!index.isValid ())
			return;

		ChannelShort channel;
		try
		{
			channel = ChannelsModel_->GetChannelForIndex (index);
		}
		catch (const std::exception& e)
		{
			ErrorNotification (tr ("Feed removal error"),
					tr ("Could not remove the feed: %1")
					.arg (e.what ()));
			return;
		}

		StorageBackend_->RemoveFeed (channel.FeedID_);
	}

	void Core::RenameFeed (const QModelIndex& index, const QString& newName)
	{
		if (!index.isValid ())
			return;

		ChannelShort channel;
		try
		{
			channel = ChannelsModel_->GetChannelForIndex (index);
		}
		catch (const std::exception& e)
		{
			ErrorNotification (tr ("Feed rename error"),
					tr ("Could not rename the feed: %1")
					.arg (e.what ()));
			return;
		}

		channel.DisplayTitle_ = newName;
		try
		{
			StorageBackend_->UpdateChannel (channel);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}

	void Core::RemoveChannel (const QModelIndex& index)
	{
		if (!index.isValid ())
			return;

		ChannelShort channel;
		try
		{
			channel = ChannelsModel_->GetChannelForIndex (index);
		}
		catch (const std::exception& e)
		{
			ErrorNotification (tr ("Channel removal error"),
					tr ("Could not remove the channel: %1")
					.arg (e.what ()));
			return;
		}

		StorageBackend_->RemoveChannel (channel.ChannelID_);
	}

	ItemsWidget* Core::GetReprWidget () const
	{
		return ReprWidget_;
	}

	Util::ShortcutManager* Core::GetShortcutManager () const
	{
		return ShortcutMgr_;
	}

	ChannelsModel* Core::GetRawChannelsModel () const
	{
		return ChannelsModel_;
	}

	QSortFilterProxyModel* Core::GetChannelsModel () const
	{
		return ChannelsFilterModel_;
	}

	IWebBrowser* Core::GetWebBrowser () const
	{
		Proxy_->GetPluginsManager ()->
				GetAllCastableRoots<IWebBrowser*> ();
		return Proxy_->GetPluginsManager ()->
				GetAllCastableTo<IWebBrowser*> ().value (0, 0);
	}

	void Core::MarkChannelAsRead (const QModelIndex& i)
	{
		MarkChannel (i, false);
	}

	void Core::MarkChannelAsUnread (const QModelIndex& i)
	{
		MarkChannel (i, true);
	}

	QStringList Core::GetTagsForIndex (int i) const
	{
		try
		{
			const auto& ids = ChannelsModel_->GetChannelForIndex (ChannelsModel_->index (i, 0)).Tags_;
			return Proxy_->GetTagsManager ()->GetTags (ids);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< "caught"
				<< e.what ();
			return QStringList ();
		}
	}

	Core::ChannelInfo Core::GetChannelInfo (const QModelIndex& i) const
	{
		ChannelShort channel;
		try
		{
			channel = ChannelsModel_->GetChannelForIndex (i);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
			return ChannelInfo ();
		}
		ChannelInfo ci;
		ci.FeedID_ = channel.FeedID_;
		ci.ChannelID_ = channel.ChannelID_;
		ci.Link_ = channel.Link_;

		Channel_ptr rc = StorageBackend_->
				GetChannel (channel.ChannelID_, channel.FeedID_);
		ci.Description_ = rc->Description_;
		ci.Author_ = rc->Author_;

		Feed_ptr feed = StorageBackend_->GetFeed (channel.FeedID_);
		ci.URL_ = feed->URL_;

		items_shorts_t items;
		StorageBackend_->GetItems (items, channel.ChannelID_);
		ci.NumItems_ = items.size ();

		return ci;
	}

	QPixmap Core::GetChannelPixmap (const QModelIndex& i) const
	{
		try
		{
			ChannelShort channel = ChannelsModel_->GetChannelForIndex (i);
			Channel_ptr rc = StorageBackend_->
					GetChannel (channel.ChannelID_, channel.FeedID_);
			return QPixmap::fromImage (rc->Pixmap_);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
			return QPixmap ();
		}
	}

	void Core::SetTagsForIndex (const QString& tags, const QModelIndex& index)
	{
		try
		{
			auto channel = ChannelsModel_->GetChannelForIndex (index);
			channel.Tags_ = Proxy_->GetTagsManager ()->GetIDs (Proxy_->GetTagsManager ()->Split (tags));
			StorageBackend_->UpdateChannel (channel);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
		}
	}

	void Core::UpdateFavicon (const QModelIndex& index)
	{
		try
		{
			ChannelShort channel = ChannelsModel_->GetChannelForIndex (index);
			FetchFavicon (StorageBackend_->
					GetChannel (channel.ChannelID_, channel.FeedID_));
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
		}
	}

	QStringList Core::GetCategories (const QModelIndex& index) const
	{
		ChannelShort cs;
		try
		{
			cs = ChannelsModel_->GetChannelForIndex (index);
		}
		catch (const std::exception& e)
		{
			return QStringList ();
		}

		items_shorts_t items;
		StorageBackend_->GetItems (items, cs.ChannelID_);
		return GetCategories (items);
	}

	QStringList Core::GetCategories (const items_shorts_t& items) const
	{
		QSet<QString> unique;
		for (const auto& item : items)
			for (const auto& category : item.Categories_)
				unique << category;

		auto result = unique.toList ();
		std::sort (result.begin (), result.end ());
		return result;
	}

	Feed::FeedSettings Core::GetFeedSettings (const QModelIndex& index) const
	{
		try
		{
			return StorageBackend_->GetFeedSettings (ChannelsModel_->
					GetChannelForIndex (index).FeedID_);
		}
		catch (const std::exception& e)
		{
			ErrorNotification (tr ("Aggregator error"),
					tr ("Could not get feed settings: %1")
					.arg (e.what ()));
			throw std::runtime_error (QString ("Could not get feed settings, "
					"inner exception: %1").arg (e.what ()).toStdString ());
		}
	}

	void Core::SetFeedSettings (const Feed::FeedSettings& settings)
	{
		try
		{
			StorageBackend_->SetFeedSettings (settings);
		}
		catch (const std::exception& e)
		{
			ErrorNotification (tr ("Aggregator error"),
					tr ("Could not update feed settings: %1")
					.arg (e.what ()));
		}
	}

	void Core::UpdateFeed (const QModelIndex& si)
	{
		QModelIndex index = si;

		ChannelShort channel;
		try
		{
			channel = ChannelsModel_->GetChannelForIndex (index);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ()
				<< si
				<< index;
			ErrorNotification (tr ("Feed update error"),
					tr ("Could not update feed"),
					false);
			return;
		}
		UpdateFeed (channel.FeedID_);
	}

	void Core::AddFromOPML (const QString& filename,
			const QString& tags,
			const std::vector<bool>& mask)
	{
		QFile file (filename);
		if (!file.open (QIODevice::ReadOnly))
		{
			ErrorNotification (tr ("OPML import error"),
					tr ("Could not open file %1 for reading.")
						.arg (filename));
			return;
		}

		QByteArray data = file.readAll ();
		file.close ();

		QString errorMsg;
		int errorLine, errorColumn;
		QDomDocument document;
		if (!document.setContent (data,
					true,
					&errorMsg,
					&errorLine,
					&errorColumn))
		{
			ErrorNotification (tr ("OPML import error"),
					tr ("XML error, file %1, line %2, column %3, error:<br />%4")
						.arg (filename)
						.arg (errorLine)
						.arg (errorColumn)
						.arg (errorMsg));
			return;
		}

		OPMLParser parser (document);
		if (!parser.IsValid ())
		{
			ErrorNotification (tr ("OPML import error"),
					tr ("OPML from file %1 is not valid.")
						.arg (filename));
			return;
		}

		OPMLParser::items_container_t items = parser.Parse ();
		for (std::vector<bool>::const_iterator begin = mask.begin (),
				i = mask.end () - 1; i >= begin; --i)
			if (!*i)
			{
				size_t distance = std::distance (mask.begin (), i);
				OPMLParser::items_container_t::iterator eraser = items.begin ();
				std::advance (eraser, distance);
				items.erase (eraser);
			}

		QStringList tagsList = Proxy_->GetTagsManager ()->Split (tags);
		for (OPMLParser::items_container_t::const_iterator i = items.begin (),
				end = items.end (); i != end; ++i)
		{
			int interval = 0;
			if (i->CustomFetchInterval_)
				interval = i->FetchInterval_;
			FeedSettings_ptr s (new Feed::FeedSettings (-1, -1,
					interval, i->MaxArticleNumber_, i->MaxArticleAge_, false));

			AddFeed (i->URL_, tagsList + i->Categories_, s);
		}
	}

	void Core::ExportToOPML (const QString& where,
			const QString& title,
			const QString& owner,
			const QString& ownerEmail,
			const std::vector<bool>& mask) const
	{
		channels_shorts_t channels;
		GetChannels (channels);

		for (std::vector<bool>::const_iterator begin = mask.begin (),
				i = mask.end () - 1; i >= begin; --i)
			if (!*i)
			{
				size_t distance = std::distance (mask.begin (), i);
				channels_shorts_t::iterator eraser = channels.begin ();
				std::advance (eraser, distance);
				channels.erase (eraser);
			}

		OPMLWriter writer;
		QString data = writer.Write (channels, title, owner, ownerEmail);

		QFile f (where);
		if (!f.open (QIODevice::WriteOnly))
		{
			ErrorNotification (tr ("OPML export error"),
					tr ("Could not open file %1 for write.").arg (where));
			return;
		}

		f.write (data.toUtf8 ());
		f.close ();
	}

	void Core::ExportToBinary (const QString& where,
			const QString& title,
			const QString& owner,
			const QString& ownerEmail,
			const std::vector<bool>& mask) const
	{
		channels_shorts_t channels;
		GetChannels (channels);

		for (std::vector<bool>::const_iterator begin = mask.begin (),
				i = mask.end () - 1; i >= begin; --i)
			if (!*i)
			{
				size_t distance = std::distance (mask.begin (), i);
				channels_shorts_t::iterator eraser = channels.begin ();
				std::advance (eraser, distance);
				channels.erase (eraser);
			}

		QFile f (where);
		if (!f.open (QIODevice::WriteOnly))
		{
			ErrorNotification (tr ("Binary export error"),
					tr ("Could not open file %1 for write.").arg (where));
			return;
		}

		QByteArray buffer;
		QDataStream data (&buffer, QIODevice::WriteOnly);

		int version = 1;
		int magic = 0xd34df00d;
		data << magic
				<< version
				<< title
				<< owner
				<< ownerEmail;

		for (channels_shorts_t::const_iterator i = channels.begin (),
				end = channels.end (); i != end; ++i)
		{
			Channel_ptr channel = StorageBackend_->
					GetChannel (i->ChannelID_, i->FeedID_);
			items_shorts_t items;
			StorageBackend_->GetItems (items, channel->ChannelID_);

			for (items_shorts_t::const_iterator j = items.begin (),
					endJ = items.end (); j != endJ; ++j)
				channel->Items_.push_back (StorageBackend_->
						GetItem (j->ItemID_));

			data << (*channel);
		}

		f.write (qCompress (buffer, 9));
	}

	JobHolderRepresentation* Core::GetJobHolderRepresentation () const
	{
		return JobHolderRepresentation_;
	}

	StorageBackend_ptr Core::MakeStorageBackendForThread () const
	{
		if (QThread::currentThread () == qApp->thread ())
			return StorageBackend_;

		const auto& strType = XmlSettingsManager::Instance ()->
				property ("StorageType").toString ();
		try
		{
			auto mgr = StorageBackend::Create (strType, "_AuxThread");
			mgr->Prepare ();
			return mgr;
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot create storage for auxiliary thread";
			return {};
		}
	}

	void Core::GetChannels (channels_shorts_t& channels) const
	{
		ids_t ids;
		StorageBackend_->GetFeedsIDs (ids);
		for (const auto id : ids)
			StorageBackend_->GetChannels (channels, id);
	}

	void Core::AddFeeds (const feeds_container_t& feeds,
			const QString& tagsString)
	{
		auto tags = Proxy_->GetTagsManager ()->Split (tagsString);
		tags.removeDuplicates ();

		for (const auto feed : feeds)
		{
			for (const auto channel : feed->Channels_)
			{
				channel->Tags_ += tags;
				channel->Tags_.removeDuplicates ();

				ChannelsModel_->AddChannel (channel->ToShort ());
			}

			StorageBackend_->AddFeed (feed);
		}
	}

	void Core::SetContextMenu (QMenu *menu)
	{
		ChannelsModel_->SetMenu (menu);
	}

	void Core::openLink (const QString& url)
	{
		IWebBrowser *browser = GetWebBrowser ();
		if (!browser ||
				XmlSettingsManager::Instance ()->
					property ("AlwaysUseExternalBrowser").toBool ())
		{
			QDesktopServices::openUrl (QUrl (url));
			return;
		}

		browser->Open (url);
	}

	void Core::handleJobFinished (int id)
	{
		if (!PendingJobs_.contains (id))
		{
			if (PendingOPMLs_.contains (id))
			{
				StartAddingOPML (PendingOPMLs_ [id].Filename_);
				PendingOPMLs_.remove (id);
			}
			return;
		}
		PendingJob pj = PendingJobs_ [id];
		PendingJobs_.remove (id);
		ID2Downloader_.remove (id);

		Util::FileRemoveGuard file (pj.Filename_);
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO << "could not open file for pj " << pj.Filename_;
			return;
		}
		if (!file.size ())
		{
			if (pj.Role_ != PendingJob::RFeedExternalData)
				ErrorNotification (tr ("Feed error"),
						tr ("Downloaded file from url %1 has null size.").arg (pj.URL_));
			return;
		}

		channels_container_t channels;
		if (pj.Role_ != PendingJob::RFeedExternalData)
		{
			QByteArray data = file.readAll ();
			QDomDocument doc;
			QString errorMsg;
			int errorLine, errorColumn;
			if (!doc.setContent (data, true, &errorMsg, &errorLine, &errorColumn))
			{
				file.copy (QDir::tempPath () + "/failedFile.xml");
				ErrorNotification (tr ("Feed error"),
						tr ("XML file parse error: %1, line %2, column %3, filename %4, from %5")
						.arg (errorMsg)
						.arg (errorLine)
						.arg (errorColumn)
						.arg (pj.Filename_)
						.arg (pj.URL_));
				return;
			}

			Parser *parser = ParserFactory::Instance ().Return (doc);
			if (!parser)
			{
				file.copy (QDir::tempPath () + "/failedFile.xml");
				ErrorNotification (tr ("Feed error"),
						tr ("Could not find parser to parse file %1 from %2")
						.arg (pj.Filename_)
						.arg (pj.URL_));
				return;
			}

			IDType_t feedId = IDNotFound;
			if (pj.Role_ == PendingJob::RFeedAdded)
			{
				const auto& feed = std::make_shared<Feed> ();
				feed->URL_ = pj.URL_;
				StorageBackend_->AddFeed (feed);
				feedId = feed->FeedID_;
			}
			else
				feedId = StorageBackend_->FindFeed (pj.URL_);

			if (feedId == IDNotFound)
			{
				ErrorNotification (tr ("Feed error"),
						tr ("Feed with url %1 not found.").arg (pj.URL_));
				return;
			}

			channels = parser->ParseFeed (doc, feedId);
		}

		if (pj.Role_ == PendingJob::RFeedAdded)
			HandleFeedAdded (channels, pj);
		else if (pj.Role_ == PendingJob::RFeedUpdated)
			HandleFeedUpdated (channels, pj);
		else if (pj.Role_ == PendingJob::RFeedExternalData)
			HandleExternalData (pj.URL_, file);
	}

	void Core::handleJobRemoved (int id)
	{
		if (PendingJobs_.contains (id))
		{
			PendingJobs_.remove (id);
			ID2Downloader_.remove (id);
		}
		if (PendingOPMLs_.contains (id))
			PendingOPMLs_.remove (id);
	}

	void Core::handleJobError (int id, IDownload::Error ie)
	{
		if (!PendingJobs_.contains (id))
		{
			if (PendingOPMLs_.contains (id))
				ErrorNotification (tr ("OPML import error"),
						tr ("Unable to download the OPML file."));
			return;
		}

		PendingJob pj = PendingJobs_ [id];
		Util::FileRemoveGuard file (pj.Filename_);

		if ((!XmlSettingsManager::Instance ()->property ("BeSilent").toBool () &&
					pj.Role_ == PendingJob::RFeedUpdated) ||
				pj.Role_ == PendingJob::RFeedAdded)
		{
			QString msg;
			switch (ie)
			{
				case IDownload::ENotFound:
					msg = tr ("Address not found:<br />%1");
					break;
				case IDownload::EAccessDenied:
					msg = tr ("Access denied:<br />%1");
					break;
				case IDownload::ELocalError:
					msg = tr ("Local error for:<br />%1");
					break;
				default:
					msg = tr ("Unknown error for:<br />%1");
					break;
			}
			ErrorNotification (tr ("Download error"),
					msg.arg (pj.URL_));
		}
		PendingJobs_.remove (id);
		ID2Downloader_.remove (id);
	}

	void Core::updateFeeds ()
	{
		ids_t ids;
		StorageBackend_->GetFeedsIDs (ids);
		for (const auto id : ids)
		{
			try
			{
				// It's handled by custom timer.
				if (StorageBackend_->GetFeedSettings (id).UpdateTimeout_)
					continue;
			}
			catch (const StorageBackend::FeedSettingsNotFoundError&)
			{
				// That's ok, we have no settings so we update as always.
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "error obtaining settings for feed"
						<< id
						<< e.what ();
			}

			UpdateFeed (id);
		}
		XmlSettingsManager::Instance ()->
			setProperty ("LastUpdateDateTime", QDateTime::currentDateTime ());
		int interval = XmlSettingsManager::Instance ()->
			property ("UpdateInterval").toInt ();
		if (interval)
			UpdateTimer_->start (interval * 60 * 1000);
	}

	void Core::fetchExternalFile (const QString& url, const QString& where)
	{
		const auto& e = Util::MakeEntity (QUrl (url),
				where,
				Internal |
					DoNotNotifyUser |
					DoNotSaveInHistory |
					NotPersistent |
					DoNotAnnounceEntity);

		PendingJob pj =
		{
			PendingJob::RFeedExternalData,
			url,
			where,
			QStringList (),
			std::shared_ptr<Feed::FeedSettings> ()
		};

		const auto& delegateResult = Proxy_->GetEntityManager ()->DelegateEntity (e);
		if (!delegateResult)
		{
			ErrorNotification (tr ("Feed error"),
					tr ("Could not find plugin to download external file %1.").arg (url));
			return;
		}

		HandleProvider (delegateResult.Handler_, delegateResult.ID_);
		PendingJobs_ [delegateResult.ID_] = pj;
	}

	void Core::handleChannelDataUpdated (Channel_ptr channel)
	{
		ChannelShort cs = channel->ToShort ();

		cs.Unread_ = StorageBackend_->GetUnreadItems (cs.ChannelID_);
		ChannelsModel_->UpdateChannelData (cs);
	}

	void Core::updateIntervalChanged ()
	{
		int min = XmlSettingsManager::Instance ()->
			property ("UpdateInterval").toInt ();
		if (min)
		{
			if (UpdateTimer_->isActive ())
				UpdateTimer_->setInterval (min * 60 * 1000);
			else
				UpdateTimer_->start (min * 60 * 1000);
		}
		else
			UpdateTimer_->stop ();
	}

	void Core::handleSslError (QNetworkReply *reply)
	{
		reply->ignoreSslErrors ();
	}

	void Core::handleCustomUpdates ()
	{
		ids_t ids;
		StorageBackend_->GetFeedsIDs (ids);
		QDateTime current = QDateTime::currentDateTime ();
		for (const auto id : ids)
		{
			int ut = 0;
			try
			{
				ut = StorageBackend_->GetFeedSettings (id).UpdateTimeout_;
			}
			catch (const StorageBackend::FeedSettingsNotFoundError&)
			{
				continue;
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "could not get feed settings for"
						<< id
						<< e.what ();
				continue;
			}

			// It's handled by normal timer.
			if (!ut)
				continue;

			if (!Updates_.contains (id) ||
					(Updates_ [id].isValid () &&
						Updates_ [id].secsTo (current) / 60 > ut))
				UpdateFeed (id);
		}
	}

	void Core::rotateUpdatesQueue ()
	{
		if (UpdatesQueue_.isEmpty ())
			return;

		const IDType_t id = UpdatesQueue_.takeFirst ();

		if (!UpdatesQueue_.isEmpty ())
			QTimer::singleShot (2000,
					this,
					SLOT (rotateUpdatesQueue ()));

		QString url = StorageBackend_->GetFeed (id)->URL_;
		for (const auto& pair : Util::Stlize (PendingJobs_))
			if (pair.second.URL_ == url)
			{
				const auto id = pair.first;
				QObject *provider = ID2Downloader_ [id];
				IDownload *downloader = qobject_cast<IDownload*> (provider);
				if (downloader)
				{
					qWarning () << Q_FUNC_INFO
						<< "stalled task detected from"
						<< downloader
						<< "trying to kill...";

					downloader->KillTask (id);
					ID2Downloader_.remove (id);
					qWarning () << Q_FUNC_INFO
						<< "killed!";
				}
				else
					qWarning () << Q_FUNC_INFO
						<< "provider is not a downloader:"
						<< provider
						<< "; cannot kill the task";
			}
		PendingJobs_.clear ();

		QString filename = Util::GetTemporaryName ();

		Entity e = Util::MakeEntity (QUrl (url),
				filename,
				Internal |
					DoNotNotifyUser |
					DoNotSaveInHistory |
					NotPersistent |
					DoNotAnnounceEntity);

		PendingJob pj =
		{
			PendingJob::RFeedUpdated,
			url,
			filename,
			QStringList (),
			std::shared_ptr<Feed::FeedSettings> ()
		};

		const auto& delegateResult = Proxy_->GetEntityManager ()->DelegateEntity (e);
		if (!delegateResult)
		{
			ErrorNotification ("Aggregator",
					tr ("Could not find plugin for feed with URL %1")
						.arg (url));
			return;
		}

		HandleProvider (delegateResult.Handler_, delegateResult.ID_);
		PendingJobs_ [delegateResult.ID_] = pj;
		Updates_ [id] = QDateTime::currentDateTime ();
	}

	void Core::handleDBUpGotNewChannel (const ChannelShort& chSh)
	{
		ChannelsModel_->AddChannel (chSh);
	}

	void Core::FetchPixmap (const Channel_ptr& channel)
	{
		if (QUrl (channel->PixmapURL_).isValid () &&
				!QUrl (channel->PixmapURL_).isRelative ())
		{
			ExternalData data;
			data.Type_ = ExternalData::TImage;
			data.RelatedChannel_ = channel;
			QString exFName = LeechCraft::Util::GetTemporaryName ();
			try
			{
				fetchExternalFile (channel->PixmapURL_, exFName);
			}
			catch (const std::runtime_error& e)
			{
				qWarning () << Q_FUNC_INFO << e.what ();
				return;
			}
			PendingJob2ExternalData_ [channel->PixmapURL_] = data;
		}
	}

	void Core::FetchFavicon (const Channel_ptr& channel)
	{
		QUrl oldUrl (channel->Link_);
		oldUrl.setPath ("/favicon.ico");
		QString iconUrl = oldUrl.toString ();

		ExternalData iconData;
		iconData.Type_ = ExternalData::TIcon;
		iconData.RelatedChannel_ = channel;
		QString exFName = LeechCraft::Util::GetTemporaryName ();
		try
		{
			fetchExternalFile (iconUrl, exFName);
		}
		catch (const std::runtime_error& e)
		{
			qWarning () << Q_FUNC_INFO << e.what ();
			return;
		}
		PendingJob2ExternalData_ [iconUrl] = iconData;
	}

	void Core::HandleExternalData (const QString& url, const QFile& file)
	{
		ExternalData data = PendingJob2ExternalData_.take (url);
		if (!data.RelatedChannel_)
		{
			qWarning () << Q_FUNC_INFO
					<< "no related channel for external data"
					<< url;
			return;
		}

		const QImage image { file.fileName () };
		switch (data.Type_)
		{
		case ExternalData::TImage:
			data.RelatedChannel_->Pixmap_ = image;
			break;
		case ExternalData::TIcon:
			data.RelatedChannel_->Favicon_ = image.scaled (16, 16);
			break;
		}

		try
		{
			StorageBackend_->UpdateChannel (data.RelatedChannel_);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
		}
	}

	void Core::HandleFeedAdded (const channels_container_t& channels,
			const Core::PendingJob& pj)
	{
		for (const auto& channel : channels)
		{
			for (const auto& item : channel->Items_)
				item->FixDate ();

			channel->Tags_ = pj.Tags_;
			ChannelsModel_->AddChannel (channel->ToShort ());
			StorageBackend_->AddChannel (channel);

			emit hookGotNewItems (std::make_shared<Util::DefaultHookProxy> (),
					Util::Map (channel->Items_,
							[] (const Item_ptr& item) { return Item_cptr { item }; }));

			FetchPixmap (channel);
			FetchFavicon (channel);
		}

		if (pj.FeedSettings_)
		{
			IDType_t feedId = StorageBackend_->FindFeed (pj.URL_);
			Feed::FeedSettings fs (feedId,
					pj.FeedSettings_->UpdateTimeout_,
					pj.FeedSettings_->NumItems_,
					pj.FeedSettings_->ItemAge_,
					pj.FeedSettings_->AutoDownloadEnclosures_);
			try
			{
				StorageBackend_->SetFeedSettings (fs);
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to set settings for just added feed"
						<< pj.URL_
						<< e.what ();
			}
		}
	}

	void Core::HandleFeedUpdated (const channels_container_t& channels,
			const Core::PendingJob& pj)
	{
		DBUpThread_->ScheduleImpl (&DBUpdateThreadWorker::updateFeed,
				channels,
				pj.URL_);
	}

	void Core::MarkChannel (const QModelIndex& i, bool state)
	{
		try
		{
			ChannelShort cs = ChannelsModel_->GetChannelForIndex (i);
			DBUpThread_->ScheduleImpl (&DBUpdateThreadWorker::toggleChannelUnread,
					cs.ChannelID_,
					state);
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
				<< e.what ();
			ErrorNotification (tr ("Aggregator error"),
					tr ("Could not mark channel"));
		}
	}

	void Core::UpdateFeed (const IDType_t& id)
	{
		if (UpdatesQueue_.isEmpty ())
			QTimer::singleShot (500,
					this,
					SLOT (rotateUpdatesQueue ()));

		UpdatesQueue_ << id;
	}

	void Core::HandleProvider (QObject *provider, int id)
	{
		ID2Downloader_ [id] = provider;

		if (Downloaders_.contains (provider))
			return;

		Downloaders_ << provider;
		connect (provider,
				SIGNAL (jobFinished (int)),
				this,
				SLOT (handleJobFinished (int)));
		connect (provider,
				SIGNAL (jobRemoved (int)),
				this,
				SLOT (handleJobRemoved (int)));
		connect (provider,
				SIGNAL (jobError (int, IDownload::Error)),
				this,
				SLOT (handleJobError (int, IDownload::Error)));
	}

	void Core::ErrorNotification (const QString& h, const QString& body, bool wait) const
	{
		auto e = Util::MakeNotification (h, body, Priority::Critical);
		e.Additional_ ["UntilUserSees"] = wait;
		Proxy_->GetEntityManager ()->HandleEntity (e);
	}
}
}
