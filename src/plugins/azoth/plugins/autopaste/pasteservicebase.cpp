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

#include "pasteservicebase.h"
#include <QApplication>
#include <QClipboard>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <util/xpc/util.h>
#include <interfaces/azoth/iclentry.h>
#include <interfaces/azoth/imessage.h>
#include <interfaces/core/ientitymanager.h>

namespace LeechCraft
{
namespace Azoth
{
namespace Autopaste
{
	PasteServiceBase::PasteServiceBase (QObject *entry, const ICoreProxy_ptr& proxy, QObject *parent)
	: QObject (parent)
	, Proxy_ (proxy)
	, Entry_ (entry)
	{
	}

	void PasteServiceBase::InitReply (QNetworkReply *reply)
	{
		connect (reply,
				SIGNAL (finished ()),
				this,
				SLOT (handleFinished ()));
		connect (reply,
				SIGNAL (metaDataChanged ()),
				this,
				SLOT (handleMetadata ()));
		connect (reply,
				SIGNAL (error (QNetworkReply::NetworkError)),
				this,
				SLOT (handleError ()));
	}

	void PasteServiceBase::FeedURL (const QString& pasteUrl)
	{
		if (!Entry_)
		{
			QApplication::clipboard ()->setText (pasteUrl, QClipboard::Clipboard);
			QApplication::clipboard ()->setText (pasteUrl, QClipboard::Selection);

			const Entity& e = Util::MakeNotification (tr ("Text pasted"),
					tr ("Your text has been pasted: %1. The URL has "
						"been copied to the clipboard."),
					PInfo_);
			Proxy_->GetEntityManager ()->HandleEntity (e);

			return;
		}

		auto entry = qobject_cast<ICLEntry*> (Entry_);
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to cast"
					<< Entry_
					<< "to ICLEntry";
			return;
		}

		auto type = entry->GetEntryType () == ICLEntry::EntryType::MUC ?
				IMessage::Type::MUCMessage :
				IMessage::Type::ChatMessage;
		const auto msg = entry->CreateMessage (type, QString (), pasteUrl);
		if (!msg)
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to create message for"
					<< entry->GetEntryID ();
			return;
		}
		msg->Send ();
	}

	void PasteServiceBase::handleError ()
	{
		sender ()->deleteLater ();
		deleteLater ();
	}

	void PasteServiceBase::handleFinished ()
	{
		sender ()->deleteLater ();
		deleteLater ();
	}

	void PasteServiceBase::handleMetadata ()
	{
		QNetworkReply *reply = qobject_cast<QNetworkReply*> (sender ());
		if (!reply)
		{
			qWarning () << Q_FUNC_INFO
					<< "sender is not a QNetworkReply:"
					<< sender ();
			return;
		}

		const auto& location = reply->header (QNetworkRequest::LocationHeader).toString ();
		if (!location.isEmpty ())
			FeedURL (location);
	}
}
}
}
