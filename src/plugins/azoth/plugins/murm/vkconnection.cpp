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

#include "vkconnection.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtDebug>
#include <qjson/parser.h>
#include <util/svcauth/vkauthmanager.h>
#include <util/queuemanager.h>

namespace LeechCraft
{
namespace Azoth
{
namespace Murm
{
	VkConnection::VkConnection (const QByteArray& cookies, ICoreProxy_ptr proxy)
	: AuthMgr_ (new Util::SvcAuth::VkAuthManager ("3778319",
			{ "messages", "notifications", "friends" }, cookies, proxy, this))
	, Proxy_ (proxy)
	, CallQueue_ (new Util::QueueManager (400))
	{
		connect (AuthMgr_,
				SIGNAL (cookiesChanged (QByteArray)),
				this,
				SLOT (saveCookies (QByteArray)));
		connect (AuthMgr_,
				SIGNAL (gotAuthKey (QString)),
				this,
				SLOT (callWithKey (QString)));

		Dispatcher_ [1] = [this] (const QVariantList&) {};
		Dispatcher_ [2] = [this] (const QVariantList&) {};
		Dispatcher_ [3] = [this] (const QVariantList&) {};

		Dispatcher_ [4] = [this] (const QVariantList& items)
		{
			emit gotMessage ({
					items.value (1).toULongLong (),
					items.value (3).toULongLong (),
					items.value (7).toString (),
					MessageFlags (items.value (2).toInt ()),
					QDateTime::fromTime_t (items.value (4).toULongLong ())
				});
		};
		Dispatcher_ [8] = [this] (const QVariantList& items)
			{ emit userStateChanged (items.value (1).toLongLong () * -1, true); };
		Dispatcher_ [9] = [this] (const QVariantList& items)
			{ emit userStateChanged (items.value (1).toLongLong () * -1, false); };

		Dispatcher_ [101] = [this] (const QVariantList&) {};	// unknown stuff
	}

	const QByteArray& VkConnection::GetCookies () const
	{
		return LastCookies_;
	}

	void VkConnection::RerequestFriends ()
	{
		PushFriendsRequest ();
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::SendMessage (qulonglong to,
			const QString& body, std::function<void (qulonglong)> idSetter)
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([=] (const QString& key) -> QNetworkReply*
			{
				QUrl url ("https://api.vk.com/method/messages.send");
				url.addQueryItem ("access_token", key);
				url.addQueryItem ("uid", QString::number (to));
				url.addQueryItem ("message", body);
				url.addQueryItem ("type", "1");

				auto reply = nam->get (QNetworkRequest (url));
				MsgReply2Setter_ [reply] = idSetter;
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleMessageSent ()));
				return reply;
			});
		AuthMgr_->GetAuthKey ();
	}

	void VkConnection::SetStatus (const EntryStatus& status)
	{
		Status_ = status;
		if (Status_.State_ == SOffline)
			return;

		if (!LPKey_.isEmpty ())
			return;

		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([this, nam] (const QString& key) -> QNetworkReply*
			{
				QUrl lpUrl ("https://api.vk.com/method/friends.getLists");
				lpUrl.addQueryItem ("access_token", key);
				auto reply = nam->get (QNetworkRequest (lpUrl));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleGotFriendLists ()));
				return reply;
			});
		PushFriendsRequest ();
		PushLPFetchCall ();

		AuthMgr_->GetAuthKey ();
	}

	const EntryStatus& VkConnection::GetStatus () const
	{
		return Status_;
	}

	void VkConnection::PushFriendsRequest ()
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([this, nam] (const QString& key) -> QNetworkReply*
			{
				QUrl friendsUrl ("https://api.vk.com/method/friends.get");
				friendsUrl.addQueryItem ("access_token", key);
				friendsUrl.addQueryItem ("fields", "first_name,last_name,nickname,photo");
				auto reply = nam->get (QNetworkRequest (friendsUrl));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleGotFriends ()));
				return reply;
			});
	}

	void VkConnection::PushLPFetchCall ()
	{
		auto nam = Proxy_->GetNetworkAccessManager ();
		PreparedCalls_.push_back ([this, nam] (const QString& key) -> QNetworkReply*
			{
				QUrl lpUrl ("https://api.vk.com/method/messages.getLongPollServer");
				lpUrl.addQueryItem ("access_token", key);
				auto reply = nam->get (QNetworkRequest (lpUrl));
				connect (reply,
						SIGNAL (finished ()),
						this,
						SLOT (handleGotLPServer ()));
				return reply;
			});
	}

	void VkConnection::Poll ()
	{
		QUrl url = LPURLTemplate_;
		url.addQueryItem ("ts", QString::number (LPTS_));
		connect (Proxy_->GetNetworkAccessManager ()->get (QNetworkRequest (url)),
				SIGNAL (finished ()),
				this,
				SLOT (handlePollFinished ()));
	}

	void VkConnection::handlePollFinished ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		const auto& data = QJson::Parser ().parse (reply);
		const auto& rootMap = data.toMap ();
		if (rootMap.contains ("failed"))
		{
			PushLPFetchCall ();
			AuthMgr_->GetAuthKey ();
			return;
		}

		for (const auto& update : rootMap ["updates"].toList ())
		{
			const auto& parmList = update.toList ();
			const auto code = parmList.value (0).toInt ();

			if (!Dispatcher_.contains (code))
				qWarning () << Q_FUNC_INFO
						<< "unknown code"
						<< code
						<< parmList;
			else
				Dispatcher_ [code] (parmList);
		}

		LPTS_ = rootMap ["ts"].toULongLong ();
		Poll ();
	}

	void VkConnection::callWithKey (const QString& key)
	{
		while (!PreparedCalls_.isEmpty ())
		{
			auto f = PreparedCalls_.takeFirst ();
			CallQueue_->Schedule ([f, key] { f (key); });
		}
	}

	void VkConnection::handleGotFriendLists ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		QList<ListInfo> lists;

		const auto& data = QJson::Parser ().parse (reply);
		for (const auto& item : data.toMap () ["response"].toList ())
		{
			const auto& map = item.toMap ();
			lists.append ({ map ["lid"].toULongLong (), map ["name"].toString () });
		}

		emit gotLists (lists);
	}

	void VkConnection::handleGotFriends ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		QList<UserInfo> users;

		const auto& data = QJson::Parser ().parse (reply);
		for (const auto& item : data.toMap () ["response"].toList ())
		{
			const auto& userMap = item.toMap ();
			if (userMap.contains ("deactivated"))
				continue;

			QList<qulonglong> lists;
			for (const auto& item : userMap ["lists"].toList ())
				lists << item.toULongLong ();

			const UserInfo ui
			{
				userMap ["uid"].toULongLong (),

				userMap ["first_name"].toString (),
				userMap ["last_name"].toString (),
				userMap ["nickname"].toString (),

				QUrl (userMap ["photo"].toString ()),

				static_cast<bool> (userMap ["online"].toULongLong ()),

				lists
			};
			users << ui;
		}

		emit gotUsers (users);
	}

	void VkConnection::handleGotLPServer ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		const auto& data = QJson::Parser ().parse (reply);
		const auto& map = data.toMap () ["response"].toMap ();

		LPKey_ = map ["key"].toString ();
		LPServer_ = map ["server"].toString ();
		LPTS_ = map ["ts"].toULongLong ();

		LPURLTemplate_ = QUrl ("http://" + LPServer_);
		LPURLTemplate_.addQueryItem ("act", "a_check");
		LPURLTemplate_.addQueryItem ("key", LPKey_);
		LPURLTemplate_.addQueryItem ("wait", "25");
		LPURLTemplate_.addQueryItem ("mode", "2");

		Poll ();
	}

	void VkConnection::handleMessageSent ()
	{
		auto reply = qobject_cast<QNetworkReply*> (sender ());
		reply->deleteLater ();

		const auto& setter = MsgReply2Setter_.take (reply);
		if (!setter)
			return;

		const auto& data = QJson::Parser ().parse (reply);
		const auto code = data.toMap ().value ("response", -1).toULongLong ();
		setter (code);
	}

	void VkConnection::saveCookies (const QByteArray& cookies)
	{
		LastCookies_ = cookies;
		emit cookiesChanged ();
	}
}
}
}
