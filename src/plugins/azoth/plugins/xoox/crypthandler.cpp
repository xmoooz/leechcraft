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

#include "crypthandler.h"
#include <QtDebug>

#ifdef ENABLE_CRYPT
#include "pgpmanager.h"
#endif

#include "clientconnection.h"
#include "glooxmessage.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	CryptHandler::CryptHandler (ClientConnection *conn)
	: QObject (conn)
	, Conn_ (conn)
#ifdef ENABLE_CRYPT
	, PGPManager_ (0)
#endif
	{
	}

	void CryptHandler::Init ()
	{
#ifdef ENABLE_CRYPT
		PGPManager_ = new PgpManager ();

		Conn_->GetClient ()->addExtension (PGPManager_);
		connect (PGPManager_,
				SIGNAL (encryptedMessageReceived (QString, QString)),
				this,
				SLOT (handleEncryptedMessageReceived (QString, QString)));
		connect (PGPManager_,
				SIGNAL (signedMessageReceived (const QString&)),
				this,
				SLOT (handleSignedMessageReceived (const QString&)));
		connect (PGPManager_,
				SIGNAL (signedPresenceReceived (const QString&)),
				this,
				SLOT (handleSignedPresenceReceived (const QString&)));
		connect (PGPManager_,
				SIGNAL (invalidSignatureReceived (const QString&)),
				this,
				SLOT (handleInvalidSignatureReceived (const QString&)));
#endif
	}

	void CryptHandler::HandlePresence (const QXmppPresence&, const QString& jid, const QString&)
	{
		if (SignedPresences_.remove (jid))
		{
			qDebug () << "got signed presence" << jid;
		}
	}

	void CryptHandler::ProcessOutgoing (QXmppMessage& msg, GlooxMessage *msgObj)
	{
#ifdef ENABLE_CRYPT
		EntryBase *entry = qobject_cast<EntryBase*> (msgObj->OtherPart ());
		if (!entry || !Entries2Crypt_.contains (entry->GetJID ()))
			return;

		const QCA::PGPKey& key = PGPManager_->PublicKey (entry->GetJID ());

		if (!key.isNull ())
		{
			const QString& body = msg.body ();
			msg.setBody (tr ("This message is encrypted. Please decrypt "
							"it to view the original contents."));

			QXmppElement crypt;
			crypt.setTagName ("x");
			crypt.setAttribute ("xmlns", "jabber:x:encrypted");
			crypt.setValue (PGPManager_->EncryptBody (key, body.toUtf8 ()));

			msg.setExtensions (msg.extensions () << crypt);
		}
#endif
	}

	void CryptHandler::ProcessIncoming (QXmppMessage& msg)
	{
		if (EncryptedMessages_.contains (msg.from ()))
			msg.setBody (EncryptedMessages_.take (msg.from ()));
	}

#ifdef ENABLE_CRYPT
	PgpManager* CryptHandler::GetPGPManager () const
	{
		return PGPManager_;
	}

	bool CryptHandler::SetEncryptionEnabled (const QString& jid, bool enabled)
	{
		if (enabled)
			Entries2Crypt_ << jid;
		else
			Entries2Crypt_.remove (jid);

		return true;
	}
#endif

	void CryptHandler::handleEncryptedMessageReceived (const QString& id,
			const QString& decrypted)
	{
		EncryptedMessages_ [id] = decrypted;
	}

	void CryptHandler::handleSignedMessageReceived (const QString&)
	{
	}

	void CryptHandler::handleSignedPresenceReceived (const QString&)
	{
	}

	void CryptHandler::handleInvalidSignatureReceived (const QString& id)
	{
		qDebug () << Q_FUNC_INFO << id;
	}
}
}
}
