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

#include "toxcontact.h"
#include <QImage>
#include <interfaces/azoth/azothutil.h>
#include "toxaccount.h"
#include "chatmessage.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Sarin
{
	ToxContact::ToxContact (const QByteArray& pubkey, ToxAccount *account)
	: QObject { account }
	, Pubkey_ { pubkey }
	, Acc_ { account }
	{
	}

	const QByteArray& ToxContact::GetPubKey () const
	{
		return Pubkey_;
	}

	QObject* ToxContact::GetQObject ()
	{
		return this;
	}

	IAccount* ToxContact::GetParentAccount () const
	{
		return Acc_;
	}

	ICLEntry::Features ToxContact::GetEntryFeatures () const
	{
		return FPermanentEntry;
	}

	ICLEntry::EntryType ToxContact::GetEntryType () const
	{
		return EntryType::Chat;
	}

	QString ToxContact::GetEntryName () const
	{
		return PublicName_.isEmpty () ? Pubkey_ : PublicName_;
	}

	void ToxContact::SetEntryName (const QString& name)
	{
		if (name == PublicName_)
			return;

		PublicName_ = name;
		emit nameChanged (GetEntryName ());
	}

	QString ToxContact::GetEntryID () const
	{
		return Acc_->GetAccountID () + '/' + Pubkey_;
	}

	QString ToxContact::GetHumanReadableID () const
	{
		return Pubkey_;
	}

	QStringList ToxContact::Groups () const
	{
		return {};
	}

	void ToxContact::SetGroups (const QStringList&)
	{
	}

	QStringList ToxContact::Variants () const
	{
		return { "" };
	}

	IMessage* ToxContact::CreateMessage (IMessage::Type type, const QString&, const QString& body)
	{
		if (type != IMessage::Type::ChatMessage)
		{
			qWarning () << Q_FUNC_INFO
					<< "unsupported message type"
					<< static_cast<int> (type);
			return nullptr;
		}

		return new ChatMessage { body, IMessage::Direction::Out, this };
	}

	QList<IMessage*> ToxContact::GetAllMessages () const
	{
		QList<IMessage*> result;
		std::copy (AllMessages_.begin (), AllMessages_.end (), std::back_inserter (result));
		return result;
	}

	void ToxContact::PurgeMessages (const QDateTime& before)
	{
		AzothUtil::StandardPurgeMessages (AllMessages_, before);
	}

	void ToxContact::SetChatPartState (ChatPartState state, const QString&)
	{
		Acc_->SetTypingState (Pubkey_, state == CPSComposing);
	}

	EntryStatus ToxContact::GetStatus (const QString&) const
	{
		return Status_;
	}

	void ToxContact::ShowInfo ()
	{
	}

	QList<QAction*> ToxContact::GetActions () const
	{
		return {};
	}

	QMap<QString, QVariant> ToxContact::GetClientInfo (const QString&) const
	{
		return {};
	}

	void ToxContact::MarkMsgsRead ()
	{
	}

	void ToxContact::ChatTabClosed ()
	{
	}

	void ToxContact::SetStatus (const EntryStatus& status)
	{
		if (status == Status_)
			return;

		Status_ = status;
		emit statusChanged (Status_, Variants ().front ());
	}

	void ToxContact::SetTyping (bool isTyping)
	{
		emit chatPartStateChanged (isTyping ? ChatPartState::CPSComposing : ChatPartState::CPSNone, {});
	}

	void ToxContact::HandleMessage (ChatMessage *msg)
	{
		AllMessages_ << msg;
		emit gotMessage (msg);
	}

	void ToxContact::SendMessage (ChatMessage *msg)
	{
		Acc_->SendMessage (Pubkey_, msg);
	}
}
}
}
