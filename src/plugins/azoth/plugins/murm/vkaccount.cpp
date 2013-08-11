/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "vkaccount.h"
#include <QUuid>
#include <QtDebug>
#include "vkprotocol.h"
#include "vkconnection.h"
#include "vkentry.h"
#include "vkmessage.h"
#include "photostorage.h"
#include "georesolver.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Murm
{
	VkAccount::VkAccount (const QString& name, VkProtocol *proto,
			ICoreProxy_ptr proxy, const QByteArray& id, const QByteArray& cookies)
	: QObject (proto)
	, CoreProxy_ (proxy)
	, Proto_ (proto)
	, ID_ (id.isEmpty () ? QUuid::createUuid ().toByteArray () : id)
	, PhotoStorage_ (new PhotoStorage (proxy->GetNetworkAccessManager (), ID_))
	, Name_ (name)
	, Conn_ (new VkConnection (cookies, proxy))
	, GeoResolver_ (new GeoResolver (Conn_, this))
	{
		connect (Conn_,
				SIGNAL (cookiesChanged ()),
				this,
				SLOT (emitUpdateAcc ()));
		connect (Conn_,
				SIGNAL (stoppedPolling ()),
				this,
				SLOT (finishOffline ()));
		connect (Conn_,
				SIGNAL (gotLists (QList<ListInfo>)),
				this,
				SLOT (handleLists (QList<ListInfo>)));
		connect (Conn_,
				SIGNAL (gotUsers (QList<UserInfo>)),
				this,
				SLOT (handleUsers (QList<UserInfo>)));
		connect (Conn_,
				SIGNAL (userStateChanged (qulonglong, bool)),
				this,
				SLOT (handleUserState (qulonglong, bool)));
		connect (Conn_,
				SIGNAL (gotMessage (MessageInfo)),
				this,
				SLOT (handleMessage (MessageInfo)));
		connect (Conn_,
				SIGNAL (gotTypingNotification (qulonglong)),
				this,
				SLOT (handleTypingNotification (qulonglong)));
		connect (Conn_,
				SIGNAL (statusChanged (EntryStatus)),
				this,
				SIGNAL (statusChanged (EntryStatus)));
	}

	QByteArray VkAccount::Serialize () const
	{
		QByteArray result;
		QDataStream out (&result, QIODevice::WriteOnly);

		out << static_cast<quint8> (1)
				<< ID_
				<< Name_
				<< Conn_->GetCookies ();

		return result;
	}

	VkAccount* VkAccount::Deserialize (const QByteArray& data, VkProtocol *proto, ICoreProxy_ptr proxy)
	{
		QDataStream in (data);

		quint8 version = 0;
		in >> version;
		if (version != 1)
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown version"
					<< version;
			return nullptr;
		}

		QByteArray id;
		QString name;
		QByteArray cookies;

		in >> id
				>> name
				>> cookies;

		return new VkAccount (name, proto, proxy, id, cookies);
	}

	ListInfo VkAccount::GetListInfo (qulonglong id) const
	{
		return ID2ListInfo_ [id];
	}

	void VkAccount::Send (VkEntry *entry, VkMessage *msg)
	{
		Conn_->SendMessage (entry->GetInfo ().ID_,
				msg->GetBody (),
				[msg] (qulonglong id) { msg->SetID (id); });
	}

	ICoreProxy_ptr VkAccount::GetCoreProxy () const
	{
		return CoreProxy_;
	}

	VkConnection* VkAccount::GetConnection () const
	{
		return Conn_;
	}

	PhotoStorage* VkAccount::GetPhotoStorage () const
	{
		return PhotoStorage_;
	}

	GeoResolver* VkAccount::GetGeoResolver () const
	{
		return GeoResolver_;
	}

	QObject* VkAccount::GetQObject ()
	{
		return this;
	}

	QObject* VkAccount::GetParentProtocol () const
	{
		return Proto_;
	}

	IAccount::AccountFeatures VkAccount::GetAccountFeatures () const
	{
		return AccountFeature::FRenamable;
	}

	QList<QObject*> VkAccount::GetCLEntries ()
	{
		QList<QObject*> result;
		result.reserve (Entries_.size ());
		std::copy (Entries_.begin (), Entries_.end (), std::back_inserter (result));
		return result;
	}

	QString VkAccount::GetAccountName () const
	{
		return Name_;
	}

	QString VkAccount::GetOurNick () const
	{
		return tr ("me");
	}

	void VkAccount::RenameAccount (const QString& name)
	{
		Name_ = name;
		emit accountRenamed (name);
		emit accountChanged (this);
	}

	QByteArray VkAccount::GetAccountID () const
	{
		return ID_;
	}

	QList<QAction*> VkAccount::GetActions () const
	{
		return {};
	}

	void VkAccount::QueryInfo (const QString&)
	{
	}

	void VkAccount::OpenConfigurationDialog ()
	{
	}

	EntryStatus VkAccount::GetState () const
	{
		return Conn_->GetStatus ();
	}

	void VkAccount::ChangeState (const EntryStatus& status)
	{
		Conn_->SetStatus (status);
	}

	void VkAccount::Authorize (QObject*)
	{
	}

	void VkAccount::DenyAuth (QObject*)
	{
	}

	void VkAccount::RequestAuth (const QString&, const QString&, const QString&, const QStringList&)
	{
	}

	void VkAccount::RemoveEntry (QObject*)
	{
	}

	QObject* VkAccount::GetTransferManager () const
	{
		return nullptr;
	}

	void VkAccount::PublishTune (const QMap<QString, QVariant>& tuneData)
	{
		if (!XmlSettingsManager::Instance ().property ("PublishTune").toBool ())
			return;

		QStringList fields
		{
			tuneData ["artist"].toString (),
			tuneData ["source"].toString (),
			tuneData ["title"].toString ()
		};
		fields.removeAll ({});

		const auto& toPublish = fields.join (QString::fromUtf8 (" — "));
		Conn_->SetStatus (toPublish);
	}

	void VkAccount::handleLists (const QList<ListInfo>& lists)
	{
		ID2ListInfo_.clear ();
		for (const auto& list : lists)
			ID2ListInfo_ [list.ID_] = list;
	}

	void VkAccount::handleUsers (const QList<UserInfo>& infos)
	{
		QList<QObject*> newEntries;
		QSet<int> newCountries;
		for (const auto& info : infos)
		{
			if (Entries_.contains (info.ID_))
			{
				Entries_ [info.ID_]->UpdateInfo (info);
				continue;
			}

			auto entry = new VkEntry (info, this);
			Entries_ [info.ID_] = entry;
			newEntries << entry;

			newCountries << info.Country_;
		}

		GeoResolver_->CacheCountries (newCountries.toList ());

		if (!newEntries.isEmpty ())
			emit gotCLItems (newEntries);
	}

	void VkAccount::handleUserState (qulonglong id, bool isOnline)
	{
		if (!Entries_.contains (id))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown user"
					<< id;
			Conn_->RerequestFriends ();
			return;
		}

		auto entry = Entries_.value (id);
		auto info = entry->GetInfo ();
		info.IsOnline_ = isOnline;
		entry->UpdateInfo (info);
	}

	void VkAccount::handleMessage (const MessageInfo& info)
	{
		const auto from = info.From_;
		if (!Entries_.contains (from))
		{
			qWarning () << Q_FUNC_INFO
					<< "message from unknown user"
					<< from;
			return;
		}

		const auto entry = Entries_.value (from);
		entry->HandleMessage (info);
	}

	void VkAccount::handleTypingNotification (qulonglong uid)
	{
		if (!Entries_.contains (uid))
		{
			qWarning () << Q_FUNC_INFO
					<< "message from unknown user"
					<< uid;
			Conn_->RerequestFriends ();
			return;
		}

		const auto entry = Entries_.value (uid);
		entry->HandleTypingNotification ();
	}

	void VkAccount::finishOffline ()
	{
		emit removedCLItems (GetCLEntries ());
		qDeleteAll (Entries_);
		Entries_.clear ();
	}

	void VkAccount::emitUpdateAcc ()
	{
		emit accountChanged (this);
	}
}
}
}
