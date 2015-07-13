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

#include "messagesmanager.h"
#include <QFutureWatcher>
#include <tox/tox.h>
#include <util/sll/slotclosure.h>
#include <util/sll/delayedexecutor.h>
#include "toxaccount.h"
#include "toxthread.h"
#include "chatmessage.h"
#include "util.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Sarin
{
	MessagesManager::MessagesManager (ToxAccount *acc)
	: QObject { acc }
	{
		connect (acc,
				SIGNAL (threadChanged (std::shared_ptr<ToxThread>)),
				this,
				SLOT (setThread (std::shared_ptr<ToxThread>)));
	}

	void MessagesManager::SendMessage (const QByteArray& privkey, ChatMessage *msg)
	{
		const auto thread = Thread_.lock ();
		if (!thread)
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot send messages in offline";
			return;
		}

		if (!msg)
		{
			qWarning () << Q_FUNC_INFO
					<< "empty message";
			return;
		}

		const auto watcher = new QFutureWatcher<MessageSendResult>;
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[watcher, this]
			{
				watcher->deleteLater ();

				const auto& result = watcher->result ();
				if (!result.Result_)
				{
					qWarning () << Q_FUNC_INFO
							<< "message was not sent, resending in 5 seconds";

					new Util::DelayedExecutor
					{
						[result, this] { SendMessage (result.Privkey_, result.Msg_); },
						5000
					};
				}
				else
					MsgId2Msg_ [result.Result_] = result.Msg_;
			},
			watcher,
			SIGNAL (finished ()),
			watcher
		};

		const auto& body = msg->GetBody ();
		watcher->setFuture (thread->ScheduleFunction ([=] (Tox *tox) -> MessageSendResult
				{
					const auto friendNum = GetFriendId (tox, privkey);
					if (friendNum < 0)
					{
						qWarning () << Q_FUNC_INFO
								<< "unknown friend"
								<< privkey;
						return { 0, privkey, msg };
					}

					const auto& msgUtf8 = body.toUtf8 ();
					TOX_ERR_FRIEND_SEND_MESSAGE error {};
					const auto id = tox_friend_send_message (tox,
							friendNum,
							TOX_MESSAGE_TYPE_NORMAL,
							reinterpret_cast<const uint8_t*> (msgUtf8.constData ()),
							msgUtf8.size (),
							&error);

					if (error != TOX_ERR_FRIEND_SEND_MESSAGE_OK)
					{
						qWarning () << Q_FUNC_INFO
								<< "unable to send message"
								<< error;
						throw MakeCommandCodeException ("tox_friend_send_message", error);
					}

					return { id, privkey, msg };
				}));
	}

	void MessagesManager::handleReadReceipt (quint32 msgId)
	{
		if (!MsgId2Msg_.contains (msgId))
		{
			qWarning () << Q_FUNC_INFO
					<< "unknown message ID"
					<< msgId
					<< MsgId2Msg_.keys ();
			return;
		}

		const auto& val = MsgId2Msg_.take (msgId);
		if (!val)
		{
			qWarning () << Q_FUNC_INFO
					<< "too late for roses, the message for ID"
					<< msgId
					<< "is dead";
			return;
		}

		val->SetDelivered ();
	}

	void MessagesManager::handleInMessage (qint32 friendId, const QString& msg)
	{
		const auto thread = Thread_.lock ();
		if (!thread)
		{
			qWarning () << Q_FUNC_INFO
					<< "got message in offline, that's kinda strange";
			return;
		}

		const auto watcher = new QFutureWatcher<QByteArray>;
		new Util::SlotClosure<Util::DeleteLaterPolicy>
		{
			[msg, watcher, this]
			{
				watcher->deleteLater ();
				const auto& pubkey = watcher->result ();
				if (pubkey.isEmpty ())
				{
					qWarning () << Q_FUNC_INFO
							<< "cannot get pubkey for message"
							<< msg;
					return;
				}

				emit gotMessage (pubkey, msg);
			},
			watcher,
			SIGNAL (finished ()),
			watcher
		};

		watcher->setFuture (thread->GetFriendPubkey (friendId));
	}

	void MessagesManager::setThread (const std::shared_ptr<ToxThread>& thread)
	{
		Thread_ = thread;

		if (thread)
			connect (thread.get (),
					SIGNAL (toxCreated (Tox*)),
					this,
					SLOT (handleToxCreated (Tox*)),
					Qt::BlockingQueuedConnection);
	}

	void MessagesManager::handleToxCreated (Tox *tox)
	{
		tox_callback_friend_message (tox,
				[] (Tox*, uint32_t friendId, TOX_MESSAGE_TYPE, const uint8_t *msgData, size_t, void *udata)
				{
					const auto& msg = QString::fromUtf8 (reinterpret_cast<const char*> (msgData));

					QMetaObject::invokeMethod (static_cast<MessagesManager*> (udata),
							"handleInMessage",
							Q_ARG (qint32, friendId),
							Q_ARG (QString, msg));
				},
				this);
		tox_callback_friend_read_receipt (tox,
				[] (Tox*, uint32_t, uint32_t msgId, void *udata)
				{
					QMetaObject::invokeMethod (static_cast<MessagesManager*> (udata),
							"handleReadReceipt",
							Q_ARG (quint32, msgId));
				},
				this);
	}
}
}
}
