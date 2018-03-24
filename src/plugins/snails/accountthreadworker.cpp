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

#include "accountthreadworker.h"
#include <algorithm>
#include <QMutexLocker>
#include <QUrl>
#include <QFile>
#include <QtDebug>
#include <QTimer>
#include <QEventLoop>
#include <QCryptographicHash>
#include <vmime/security/defaultAuthenticator.hpp>
#include <vmime/net/transport.hpp>
#include <vmime/net/store.hpp>
#include <vmime/net/message.hpp>
#include <vmime/utility/datetimeUtils.hpp>
#include <vmime/dateTime.hpp>
#include <vmime/messageParser.hpp>
#include <vmime/messageBuilder.hpp>
#include <vmime/htmlTextPart.hpp>
#include <vmime/stringContentHandler.hpp>
#include <vmime/fileAttachment.hpp>
#include <vmime/messageIdSequence.hpp>
#include <util/util.h>
#include <util/xpc/util.h>
#include <util/sll/prelude.h>
#include <util/sll/unreachable.h>
#include "message.h"
#include "account.h"
#include "core.h"
#include "progresslistener.h"
#include "storage.h"
#include "vmimeconversions.h"
#include "outputiodevadapter.h"
#include "common.h"
#include "messagechangelistener.h"
#include "folder.h"
#include "tracerfactory.h"
#include "accountthreadnotifier.h"
#include "certificateverifier.h"
#include "tracebytecounter.h"

namespace LeechCraft
{
namespace Snails
{
	namespace
	{
		template<typename T>
		auto WaitForFuture (const QFuture<T>& future)
		{
			QFutureWatcher<QString> watcher;
			QEventLoop loop;

			QObject::connect (&watcher,
					&QFutureWatcher<QString>::finished,
					&loop,
					&QEventLoop::quit);

			watcher.setFuture (future);

			loop.exec ();

			return future.result ();
		}

		class VMimeAuth : public vmime::security::defaultAuthenticator
		{
			Account::Direction Dir_;
			Account *Acc_;
		public:
			VMimeAuth (Account::Direction, Account*);

			const vmime::string getUsername () const;
			const vmime::string getPassword () const;
		private:
			QByteArray GetID () const
			{
				QByteArray id = "org.LeechCraft.Snails.PassForAccount/" + Acc_->GetID ();
				id += Dir_ == Account::Direction::Out ? "/Out" : "/In";
				return id;
			}
		};

		VMimeAuth::VMimeAuth (Account::Direction dir, Account *acc)
		: Dir_ (dir)
		, Acc_ (acc)
		{
		}

		const vmime::string VMimeAuth::getUsername () const
		{
			switch (Dir_)
			{
			case Account::Direction::Out:
				return Acc_->GetOutUsername ().toStdString ();
			case Account::Direction::In:
				return Acc_->GetInUsername ().toStdString ();
			}

			Util::Unreachable ();
		}

		const vmime::string VMimeAuth::getPassword () const
		{
			return WaitForFuture (Acc_->GetPassword (Account::Direction::In)).toStdString ();
		}
	}

	const char* FolderNotFound::what () const noexcept
	{
		return "folder not found";
	}

	const char* MessageNotFound::what () const noexcept
	{
		return "message not found";
	}

	AccountThreadWorker::AccountThreadWorker (bool isListening,
			const QString& threadName, Account *parent, Storage *st)
	: A_ (parent)
	, Storage_ (st)
	, NoopTimer_ (new QTimer (this))
	, IsListening_ (isListening)
	, ThreadName_ (threadName)
	, ChangeListener_ (new MessageChangeListener (this))
	, Session_ (vmime::net::session::create ())
	, CachedFolders_ (2)
	, CertVerifier_ (vmime::make_shared<CertificateVerifier> ())
	, InAuth_ (vmime::make_shared<VMimeAuth> (Account::Direction::In, A_))
	{

		if (IsListening_)
			connect (ChangeListener_,
					SIGNAL (messagesChanged (QStringList, QList<size_t>)),
					this,
					SLOT (handleMessagesChanged (QStringList, QList<size_t>)));

		connect (NoopTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (sendNoop ()));
		NoopTimer_->start (60 * 1000);
	}

	vmime::shared_ptr<vmime::net::store> AccountThreadWorker::MakeStore ()
	{
		if (CachedStore_)
			return CachedStore_;

		auto url = WaitForFuture (A_->BuildInURL ());

		auto st = Session_->getStore (vmime::utility::url (url.toUtf8 ().constData ()));

		TracerFactory_ = vmime::make_shared<TracerFactory> (ThreadName_, A_->GetLogger ());
		st->setTracerFactory (TracerFactory_);

		st->setCertificateVerifier (CertVerifier_);
		st->setAuthenticator (InAuth_);

		if (A_->UseTLS_)
		{
			st->setProperty ("connection.tls", A_->UseTLS_);
			st->setProperty ("connection.tls.required", A_->InSecurityRequired_);
		}
		st->setProperty ("options.sasl", A_->UseSASL_);
		st->setProperty ("options.sasl.fallback", A_->SASLRequired_);
		st->setProperty ("server.port", A_->InPort_);

		st->connect ();

		if (IsListening_)
			if (const auto defFolder = st->getDefaultFolder ())
			{
				defFolder->addMessageChangedListener (ChangeListener_);
				CachedFolders_ [GetFolderPath (defFolder)] = defFolder;
			}

		CachedStore_ = st;

		return st;
	}

	vmime::shared_ptr<vmime::net::transport> AccountThreadWorker::MakeTransport ()
	{
		auto url = WaitForFuture (A_->BuildOutURL ());

		QString username;
		QString password;
		bool setAuth = false;
		if (A_->SMTPNeedsAuth_ &&
				A_->OutType_ == Account::OutType::SMTP)
		{
			setAuth = true;

			QUrl parsed = QUrl::fromEncoded (url.toUtf8 ());
			username = parsed.userName ();
			password = parsed.password ();

			parsed.setUserName (QString ());
			parsed.setPassword (QString ());
			url = QString::fromUtf8 (parsed.toEncoded ());
			qDebug () << Q_FUNC_INFO << url << username << password;
		}

		auto trp = Session_->getTransport (vmime::utility::url (url.toUtf8 ().constData ()));
		trp->setCertificateVerifier (CertVerifier_);

		if (setAuth)
		{
			trp->setProperty ("options.need-authentication", true);
			trp->setProperty ("auth.username", username.toUtf8 ().constData ());
			trp->setProperty ("auth.password", password.toUtf8 ().constData ());
		}
		trp->setProperty ("server.port", A_->OutPort_);

		if (A_->OutSecurity_ == Account::SecurityType::TLS)
		{
			trp->setProperty ("connection.tls", true);
			trp->setProperty ("connection.tls.required", A_->OutSecurityRequired_);
		}

		return trp;
	}

	namespace
	{
		vmime::net::folder::path Folder2Path (const QStringList& folder)
		{
			if (folder.isEmpty ())
				return vmime::net::folder::path ("INBOX");

			vmime::net::folder::path path;
			for (const auto& comp : folder)
				path.appendComponent ({ comp.toUtf8 ().constData (), vmime::charsets::UTF_8 });
			return path;
		}
	}

	VmimeFolder_ptr AccountThreadWorker::GetFolder (const QStringList& path, FolderMode mode)
	{
		if (path.size () == 1 && path.at (0) == "[Gmail]")
			return {};

		if (!CachedFolders_.contains (path))
		{
			auto store = MakeStore ();
			CachedFolders_ [path] = store->getFolder (Folder2Path (path));
		}

		auto folder = CachedFolders_ [path];

		if (mode == FolderMode::NoChange)
			return folder;

		const auto requestedMode = mode == FolderMode::ReadOnly ?
				vmime::net::folder::MODE_READ_ONLY :
				vmime::net::folder::MODE_READ_WRITE;

		const auto isOpen = folder->isOpen ();
		if (isOpen && folder->getMode () == requestedMode)
			return folder;
		else if (isOpen)
			folder->close (false);

		folder->open (requestedMode);

		return folder;
	}

	namespace
	{
		class MessageStructHandler
		{
			const Message_ptr Msg_;
			const vmime::shared_ptr<vmime::net::message> VmimeMessage_;
		public:
			MessageStructHandler (const Message_ptr&, const vmime::shared_ptr<vmime::net::message>&);

			void operator() ();
		private:
			void HandlePart (const vmime::shared_ptr<vmime::net::messagePart>&);

			template<typename T>
			void EnumParts (const T&);
		};

		MessageStructHandler::MessageStructHandler (const Message_ptr& msg,
				const vmime::shared_ptr<vmime::net::message>& message)
		: Msg_ { msg }
		, VmimeMessage_ { message }
		{
		}

		void MessageStructHandler::operator() ()
		{
			const auto& structure = VmimeMessage_->getStructure ();
			if (!structure)
				return;

			EnumParts (structure);
		}

		void MessageStructHandler::HandlePart (const vmime::shared_ptr<vmime::net::messagePart>& part)
		{
			const auto& type = part->getType ();

			if (type.getType () == "text")
				return;

			if (type.getType () == "multipart")
			{
				EnumParts (part);
				return;
			}

			Msg_->AddAttachment ({
						{},
						{},
						type.getType ().c_str (),
						type.getSubType ().c_str (),
						static_cast<qlonglong> (part->getSize ())
					});
		}

		template<typename T>
		void MessageStructHandler::EnumParts (const T& enumable)
		{
			const auto pc = enumable->getPartCount ();
			for (vmime::size_t i = 0; i < pc; ++i)
				HandlePart (enumable->getPartAt (i));
		}
	}

	MessageWHeaders_t AccountThreadWorker::FromHeaders (const vmime::shared_ptr<vmime::net::message>& message) const
	{
		const auto& utf8cs = vmime::charset { vmime::charsets::UTF_8 };

		auto msg = std::make_shared<Message> ();
		msg->SetFolderID (static_cast<vmime::string> (message->getUID ()).c_str ());
		msg->SetSize (message->getSize ());

		if (message->getFlags () & vmime::net::message::FLAG_SEEN)
			msg->SetRead (true);

		auto header = message->getHeader ();

		if (const auto& from = header->From ())
		{
			const auto& mboxVal = from->getValue ();
			const auto& mbox = vmime::dynamicCast<const vmime::mailbox> (mboxVal);
			msg->AddAddress (Message::Address::From, Mailbox2Strings (mbox));
		}
		else
			qWarning () << "no 'from' data";

		if (const auto& replyToHeader = header->ReplyTo ())
		{
			const auto& replyToVal = replyToHeader->getValue ();
			const auto& replyTo = vmime::dynamicCast<const vmime::mailbox> (replyToVal);
			msg->AddAddress (Message::Address::ReplyTo, Mailbox2Strings (replyTo));
		}

		if (const auto& msgIdField = header->MessageId ())
		{
			const auto& msgIdVal = msgIdField->getValue ();
			const auto& msgId = vmime::dynamicCast<const vmime::messageId> (msgIdVal);
			msg->SetMessageID (msgId->getId ().c_str ());
		}

		if (const auto& refHeader = header->References ())
		{
			const auto& seqVal = refHeader->getValue ();
			const auto& seq = vmime::dynamicCast<const vmime::messageIdSequence> (seqVal);

			for (const auto& id : seq->getMessageIdList ())
				msg->AddReferences (id->getId ().c_str ());
		}

		if (const auto& irt = header->InReplyTo ())
		{
			const auto& seqVal = irt->getValue ();
			const auto& seq = vmime::dynamicCast<const vmime::messageIdSequence> (seqVal);

			for (const auto& id : seq->getMessageIdList ())
				msg->AddInReplyTo (id->getId ().c_str ());
		}

		auto setAddresses = [&msg] (Message::Address type, const vmime::shared_ptr<const vmime::headerField>& field)
		{
			if (!field)
				return;

			if (const auto& alist = vmime::dynamicCast<const vmime::addressList> (field->getValue ()))
			{
				auto addrs = Util::Map (alist->toMailboxList ()->getMailboxList (), &Mailbox2Strings);
				msg->SetAddresses (type, addrs);
			}
			else
			{
				const auto fieldPtr = field.get ();
				qWarning () << "no"
						<< static_cast<int> (type)
						<< "data: cannot cast to mailbox list"
						<< typeid (*fieldPtr).name ();
			}
		};

		setAddresses (Message::Address::To, header->To ());
		setAddresses (Message::Address::Cc, header->Cc ());
		setAddresses (Message::Address::Bcc, header->Bcc ());
		if (const auto dateHeader = header->Date ())
		{
			const auto& origDateVal = dateHeader->getValue ();
			const auto& origDate = vmime::dynamicCast<const vmime::datetime> (origDateVal);
			const auto& date = vmime::utility::datetimeUtils::toUniversalTime (*origDate);
			QDate qdate (date.getYear (), date.getMonth (), date.getDay ());
			QTime time (date.getHour (), date.getMinute (), date.getSecond ());
			msg->SetDate (QDateTime (qdate, time, Qt::UTC));
		}
		else
			qWarning () << "no 'date' data";

		if (const auto subjectHeader = header->Subject ())
		{
			const auto& strVal = subjectHeader->getValue ();
			const auto& str = vmime::dynamicCast<const vmime::text> (strVal);
			msg->SetSubject (QString::fromUtf8 (str->getConvertedText (utf8cs).c_str ()));
		}
		else
			qWarning () << "no 'subject' data";

		MessageStructHandler { msg, message } ();

		return { msg, header };
	}

	namespace
	{
		vmime::shared_ptr<vmime::message> FromNetMessage (vmime::shared_ptr<vmime::net::message> msg)
		{
			return msg->getParsedMessage ();
		}
	}

	auto AccountThreadWorker::FetchMessagesIMAP (const QList<QStringList>& origFolders,
			const QByteArray& last) -> Folder2Messages_t
	{
		Folder2Messages_t result;

		const auto pl = A_->MakeProgressListener (tr ("Synchronizing messages..."));
		pl->start (origFolders.size ());

		for (const auto& folder : origFolders)
		{
			if (const auto& netFolder = GetFolder (folder, FolderMode::ReadOnly))
				result [folder] = FetchMessagesInFolder (folder, netFolder, last);

			pl->Increment ();
		}

		pl->stop (origFolders.size ());

		return result;
	}

	namespace
	{
		MessageVector_t GetMessagesInFolder (const VmimeFolder_ptr& folder, const QByteArray& lastId)
		{
			const int desiredFlags = vmime::net::fetchAttributes::FLAGS |
						vmime::net::fetchAttributes::SIZE |
						vmime::net::fetchAttributes::UID |
						vmime::net::fetchAttributes::FULL_HEADER |
						vmime::net::fetchAttributes::STRUCTURE |
						vmime::net::fetchAttributes::ENVELOPE;

			if (lastId.isEmpty ())
			{
				const auto count = folder->getMessageCount ();

				MessageVector_t messages;
				messages.reserve (count);

				const auto chunkSize = 100;
				for (vmime::size_t i = 0; i < count; i += chunkSize)
				{
					const auto endVal = i + chunkSize;
					const auto& set = vmime::net::messageSet::byNumber (i + 1, std::min (count, endVal));
					try
					{
						auto theseMessages = folder->getAndFetchMessages (set, desiredFlags);
						std::move (theseMessages.begin (), theseMessages.end (), std::back_inserter (messages));
					}
					catch (const std::exception& e)
					{
						qWarning () << Q_FUNC_INFO
								<< "cannot get messages from"
								<< i + 1
								<< "to"
								<< endVal
								<< "because:"
								<< e.what ();
						return {};
					}
				}

				return messages;
			}
			else
			{
				const auto& set = vmime::net::messageSet::byUID (lastId.constData (), "*");
				try
				{
					return folder->getAndFetchMessages (set, desiredFlags);
				}
				catch (const std::exception& e)
				{
					qWarning () << Q_FUNC_INFO
							<< "cannot get messages from"
							<< lastId
							<< "because:"
							<< e.what ();
					return {};
				}
			}
		}
	}

	auto AccountThreadWorker::FetchMessagesInFolder (const QStringList& folderName,
			const VmimeFolder_ptr& folder, const QByteArray& lastId) -> FolderMessages
	{
		const auto changeGuard = ChangeListener_->Disable ();

		const auto bytesCounter = TracerFactory_->CreateCounter ();

		qDebug () << Q_FUNC_INFO << folderName << folder.get () << lastId;

		auto messages = GetMessagesInFolder (folder, lastId);

		qDebug () << "done fetching, sent" << bytesCounter.GetSent ()
				<< "bytes, received" << bytesCounter.GetReceived () << "bytes";
		auto newMessages = Util::Map (messages, [this, &folderName] (const auto& msg)
				{
					auto res = FromHeaders (msg);
					res.first->AddFolder (folderName);
					return res;
				});
		auto existing = Storage_->LoadIDs (A_, folderName);

		QList<QByteArray> ids;

		QList<Message_ptr> updatedMessages;
		for (auto it = newMessages.begin (); it != newMessages.end ();)
		{
			const auto& msg = it->first;
			if (!existing.contains (msg->GetFolderID ()))
			{
				++it;
				continue;
			}

			existing.removeAll (msg->GetFolderID ());

			bool isUpdated = false;

			auto updated = Storage_->LoadMessage (A_, folderName, msg->GetFolderID ());

			if (updated->IsRead () != msg->IsRead ())
			{
				updated->SetRead (msg->IsRead ());
				isUpdated = true;
			}

			auto sumFolders = updated->GetFolders ();
			if (!folderName.isEmpty () &&
					!sumFolders.contains (folderName))
			{
				updated->AddFolder (folderName);
				isUpdated = true;
			}

			if (isUpdated)
				updatedMessages << updated;
			else
				ids << msg->GetFolderID ();

			it = newMessages.erase (it);
		}

		return
		{
			newMessages,
			updatedMessages,
			ids,
			lastId.isEmpty () ? existing : QList<QByteArray> {}
		};
	}

	namespace
	{
		void FullifyHeaderMessage (const Message_ptr& msg, const vmime::shared_ptr<vmime::net::message>& full)
		{
			vmime::messageParser mp { FromNetMessage (full) };

			QString html;
			QStringList plainParts;
			QStringList htmlPlainParts;

			for (const auto& tp : mp.getTextPartList ())
			{
				const auto& type = tp->getType ();
				if (type.getType () != vmime::mediaTypes::TEXT)
				{
					qWarning () << Q_FUNC_INFO
							<< "non-text in text part"
							<< tp->getType ().getType ().c_str ();
					continue;
				}

				if (type.getSubType () == vmime::mediaTypes::TEXT_HTML)
				{
					auto htp = vmime::dynamicCast<const vmime::htmlTextPart> (tp);
					html = Stringize (htp->getText (), htp->getCharset ());
					htmlPlainParts << Stringize (htp->getPlainText (), htp->getCharset ());
				}
				else if (type.getSubType () == vmime::mediaTypes::TEXT_PLAIN)
					plainParts << Stringize (tp->getText (), tp->getCharset ());
			}

			if (plainParts.isEmpty ())
				plainParts = htmlPlainParts;

			if (std::adjacent_find (plainParts.begin (), plainParts.end (),
					[] (const QString& left, const QString& right)
						{ return left.size () > right.size (); }) == plainParts.end ())
				std::reverse (plainParts.begin (), plainParts.end ());

			msg->SetBody (plainParts.join ("\n"));
			msg->SetHTMLBody (html);

			msg->SetAttachmentList ({});

			for (const auto& att : mp.getAttachmentList ())
			{
				const auto& type = att->getType ();
				if (type.getType () == vmime::mediaTypes::TEXT &&
						(type.getSubType () == vmime::mediaTypes::TEXT_HTML ||
						 type.getSubType () == vmime::mediaTypes::TEXT_PLAIN))
					continue;

				msg->AddAttachment (att);
			}
		}

		FolderType ToFolderType (int specialUse)
		{
			switch (static_cast<vmime::net::folderAttributes::SpecialUses> (specialUse))
			{
			case vmime::net::folderAttributes::SPECIALUSE_SENT:
				return FolderType::Sent;
			case vmime::net::folderAttributes::SPECIALUSE_DRAFTS:
				return FolderType::Drafts;
			case vmime::net::folderAttributes::SPECIALUSE_IMPORTANT:
				return FolderType::Important;
			case vmime::net::folderAttributes::SPECIALUSE_JUNK:
				return FolderType::Junk;
			case vmime::net::folderAttributes::SPECIALUSE_TRASH:
				return FolderType::Trash;
			default:
				return FolderType::Other;
			}
		}
	}

	QList<Folder> AccountThreadWorker::SyncIMAPFolders (vmime::shared_ptr<vmime::net::store> store)
	{
		const auto& root = store->getRootFolder ();
		const auto& inbox = store->getDefaultFolder ();

		QList<Folder> folders;
		for (const auto& folder : root->getFolders (true))
		{
			const auto& attrs = folder->getAttributes ();
			folders.append ({
					GetFolderPath (folder),
					folder->getFullPath () == inbox->getFullPath () ?
						FolderType::Inbox :
						ToFolderType (attrs.getSpecialUse ())
				});
		}
		return folders;
	}

	void AccountThreadWorker::handleMessagesChanged (const QStringList& folder, const QList<size_t>& numbers)
	{
		qDebug () << Q_FUNC_INFO << folder << numbers;
		auto set = vmime::net::messageSet::empty ();
		for (const auto& num : numbers)
			set.addRange (vmime::net::numberMessageRange { num });
	}

	void AccountThreadWorker::sendNoop ()
	{
		const auto at = static_cast<AccountThread*> (QThread::currentThread ());
		at->Schedule (TaskPriority::Low,
				[this] (AccountThreadWorker*)
				{
					if (CachedStore_)
						CachedStore_->noop ();
				});
	}

	void AccountThreadWorker::SetNoopTimeout (int timeout)
	{
		NoopTimer_->stop ();

		if (timeout > NoopTimer_->interval ())
			sendNoop ();

		NoopTimer_->start (timeout);
	}

	void AccountThreadWorker::SetNoopTimeoutChangeNotifier (const std::shared_ptr<AccountThreadNotifier<int>>& notifier)
	{
		SetNoopTimeout (notifier->GetData ());

		connect (notifier.get (),
				&AccountThreadNotifier<int>::changed,
				[this, notifier] { SetNoopTimeout (notifier->GetData ()); });
	}

	void AccountThreadWorker::Disconnect ()
	{
		CachedFolders_.clear ();

		if (!CachedStore_)
			return;

		if (CachedStore_->isConnected ())
			CachedStore_->disconnect ();

		CachedStore_.reset ();
	}

	void AccountThreadWorker::TestConnectivity ()
	{
		if (!CachedStore_)
			MakeStore ();

		sendNoop ();
	}

	auto AccountThreadWorker::Synchronize (const QList<QStringList>& foldersToFetch, const QByteArray& last) -> SyncResult
	{
		const auto& store = MakeStore ();
		const auto& folders = SyncIMAPFolders (store);
		const auto& fetchResult = FetchMessagesIMAP (foldersToFetch, last);
		return { folders, fetchResult };
	}

	auto AccountThreadWorker::GetMessageCount (const QStringList& folder) -> MsgCountResult_t
	{
		auto netFolder = GetFolder (folder, FolderMode::NoChange);
		if (!netFolder)
			return MsgCountResult_t::Left (FolderNotFound {});

		const auto& status = netFolder->getStatus ();
		auto count = status->getMessageCount ();
		if (count >= 3000)
			count = GetFolder (folder, FolderMode::ReadOnly)->getMessageCount ();
		return MsgCountResult_t::Right ({ count, status->getUnseenCount () });
	}

	auto AccountThreadWorker::SetReadStatus (bool read,
			const QList<QByteArray>& ids, const QStringList& folderPath) -> SetReadStatusResult_t
	{
		const auto& folder = GetFolder (folderPath, FolderMode::ReadWrite);
		if (!folder)
			return SetReadStatusResult_t::Left (FolderNotFound {});

		folder->setMessageFlags (ToMessageSet (ids),
				vmime::net::message::Flags::FLAG_SEEN,
				read ?
						vmime::net::message::FLAG_MODE_ADD :
						vmime::net::message::FLAG_MODE_REMOVE);

		QList<Message_ptr> messages;
		for (const auto& id : ids)
		{
			const auto& message = Storage_->LoadMessage (A_, folderPath, id);
			message->SetRead (read);

			messages << message;
		}

		return SetReadStatusResult_t::Right (messages);
	}

	FetchWholeMessageResult_t AccountThreadWorker::FetchWholeMessage (Message_ptr origMsg)
	{
		if (!origMsg)
			return FetchWholeMessageResult_t::Right ({});

		const QByteArray& sid = origMsg->GetFolderID ();
		auto folder = GetFolder (origMsg->GetFolders ().value (0), FolderMode::ReadOnly);
		if (!folder)
			return FetchWholeMessageResult_t::Left (FolderNotFound {});

		const auto& set = vmime::net::messageSet::byUID (sid.constData ());
		const auto attrs = vmime::net::fetchAttributes::FLAGS |
				vmime::net::fetchAttributes::UID |
				vmime::net::fetchAttributes::CONTENT_INFO |
				vmime::net::fetchAttributes::STRUCTURE |
				vmime::net::fetchAttributes::FULL_HEADER;
		const auto& messages = folder->getAndFetchMessages (set, attrs);
		if (messages.empty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "message with ID"
					<< sid.toHex ()
					<< "not found in"
					<< messages.size ();
			return FetchWholeMessageResult_t::Left (MessageNotFound {});
		}

		FullifyHeaderMessage (origMsg, messages.front ());

		qDebug () << "done";

		return FetchWholeMessageResult_t::Right (origMsg);
	}

	void AccountThreadWorker::FetchAttachment (Message_ptr msg,
			const QString& attName, const QString& path)
	{
		const auto& msgId = msg->GetFolderID ();

		const auto& folder = GetFolder (msg->GetFolders ().value (0), FolderMode::ReadWrite);
		const auto& msgSet = vmime::net::messageSet::byUID (msgId.constData ());
		const auto& messages = folder->getAndFetchMessages (msgSet, vmime::net::fetchAttributes::STRUCTURE);
		if (messages.empty ())
		{
			qWarning () << Q_FUNC_INFO
					<< "message with ID"
					<< msgId.toHex ()
					<< "not found in"
					<< messages.size ();
			return;
		}

		vmime::messageParser mp (messages.front ()->getParsedMessage ());
		for (const auto& att : mp.getAttachmentList ())
		{
			if (StringizeCT (att->getName ()) != attName)
				continue;

			auto data = att->getData ();

			QFile file (path);
			if (!file.open (QIODevice::WriteOnly))
			{
				qWarning () << Q_FUNC_INFO
						<< "unable to open"
						<< path
						<< file.errorString ();
				return;
			}

			OutputIODevAdapter adapter (&file);
			const auto pl = A_->MakeProgressListener (tr ("Fetching attachment %1...")
						.arg (attName));
			data->extract (adapter, pl.get ());

			break;
		}
	}

	void AccountThreadWorker::CopyMessages (const QList<QByteArray>& ids,
			const QStringList& from, const QList<QStringList>& tos)
	{
		if (ids.isEmpty () || tos.isEmpty ())
			return;

		const auto& folder = GetFolder (from, FolderMode::ReadWrite);
		if (!folder)
			return;

		const auto& set = ToMessageSet (ids);
		for (const auto& to : tos)
			folder->copyMessages (Folder2Path (to), set);
	}

	auto AccountThreadWorker::DeleteMessages (const QList<QByteArray>& ids,
			const QStringList& path) -> DeleteResult_t
	{
		if (ids.isEmpty ())
			return DeleteResult_t::Right ({});

		const auto& folder = GetFolder (path, FolderMode::ReadWrite);
		if (!folder)
		{
			qWarning () << Q_FUNC_INFO
					<< path
					<< "not found";
			return DeleteResult_t::Left (FolderNotFound {});
		}

		folder->deleteMessages (ToMessageSet (ids));
		folder->expunge ();

		return DeleteResult_t::Right ({});
	}

	namespace
	{
		vmime::shared_ptr<vmime::mailbox> FromPair (const QPair<QString, QString>& pair)
		{
			return vmime::make_shared<vmime::mailbox> (vmime::text (pair.first.toUtf8 ().constData ()),
					pair.second.toUtf8 ().constData ());
		}

		vmime::addressList ToAddressList (const Message::Addresses_t& addresses)
		{
			vmime::addressList result;
			for (const auto& pair : addresses)
				result.appendAddress (FromPair (pair));
			return result;
		}

		vmime::messageIdSequence ToMessageIdSequence (const QList<QByteArray>& ids)
		{
			vmime::messageIdSequence result;
			for (const auto& id : ids)
				result.appendMessageId (vmime::make_shared<vmime::messageId> (id.constData ()));
			return result;
		}

		vmime::messageId GenerateMsgId (const Message_ptr& msg)
		{
			const auto& senderAddress = msg->GetAddress (Message::Address::From).second;

			const auto& contents = msg->GetBody ().toUtf8 ();
			const auto& contentsHash = QCryptographicHash::hash (contents, QCryptographicHash::Sha1).toBase64 ();

			const auto& now = QDateTime::currentDateTime ();

			const auto& id = now.toString ("ddMMyyyy.hhmmsszzzap") + '%' + contentsHash + '%' + senderAddress;

			return vmime::string { id.toUtf8 ().constData () };
		}
	}

	void AccountThreadWorker::SendMessage (const Message_ptr& msg)
	{
		if (!msg)
			return;

		vmime::messageBuilder mb;
		mb.setSubject (vmime::text (msg->GetSubject ().toUtf8 ().constData ()));
		mb.setExpeditor (*FromPair (msg->GetAddress (Message::Address::From)));
		mb.setRecipients (ToAddressList (msg->GetAddresses (Message::Address::To)));
		mb.setCopyRecipients (ToAddressList (msg->GetAddresses (Message::Address::Cc)));
		mb.setBlindCopyRecipients (ToAddressList (msg->GetAddresses (Message::Address::Bcc)));

		const auto& html = msg->GetHTMLBody ();

		if (html.isEmpty ())
		{
			mb.getTextPart ()->setCharset (vmime::charsets::UTF_8);
			mb.getTextPart ()->setText (vmime::make_shared<vmime::stringContentHandler> (msg->GetBody ().toUtf8 ().constData ()));
		}
		else
		{
			mb.constructTextPart ({ vmime::mediaTypes::TEXT, vmime::mediaTypes::TEXT_HTML });
			const auto& textPart = vmime::dynamicCast<vmime::htmlTextPart> (mb.getTextPart ());
			textPart->setCharset (vmime::charsets::UTF_8);
			textPart->setText (vmime::make_shared<vmime::stringContentHandler> (html.toUtf8 ().constData ()));
			textPart->setPlainText (vmime::make_shared<vmime::stringContentHandler> (msg->GetBody ().toUtf8 ().constData ()));
		}

		for (const auto& descr : msg->GetAttachments ())
		{
			try
			{
				const QFileInfo fi (descr.GetName ());
				const auto& att = vmime::make_shared<vmime::fileAttachment> (descr.GetName ().toUtf8 ().constData (),
						vmime::mediaType (descr.GetType ().constData (), descr.GetSubType ().constData ()),
						vmime::text (descr.GetDescr ().toUtf8 ().constData ()));
				att->getFileInfo ().setFilename (fi.fileName ().toUtf8 ().constData ());
				att->getFileInfo ().setSize (descr.GetSize ());

				mb.appendAttachment (att);
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "failed to append"
						<< descr.GetName ()
						<< e.what ();
			}
		}

		const auto& vMsg = mb.construct ();
		const auto& userAgent = QString ("LeechCraft Snails %1")
				.arg (Core::Instance ().GetProxy ()->GetVersion ());
		const auto& header = vMsg->getHeader ();
		header->UserAgent ()->setValue (userAgent.toUtf8 ().constData ());

		if (!msg->GetInReplyTo ().isEmpty ())
			header->InReplyTo ()->setValue (ToMessageIdSequence (msg->GetInReplyTo ()));
		if (!msg->GetReferences ().isEmpty ())
			header->References ()->setValue (ToMessageIdSequence (msg->GetReferences ()));

		header->MessageId ()->setValue (GenerateMsgId (msg));

		auto pl = A_->MakeProgressListener (tr ("Sending message %1...").arg (msg->GetSubject ()));
		auto transport = MakeTransport ();
		try
		{
			if (!transport->isConnected ())
				transport->connect ();
		}
		catch (const vmime::exceptions::authentication_error& e)
		{
			qWarning () << Q_FUNC_INFO
					<< "authentication error:"
					<< e.what ()
					<< "with response"
					<< e.response ().c_str ();
			throw;
		}
		transport->send (vMsg, pl.get ());
	}
}
}
