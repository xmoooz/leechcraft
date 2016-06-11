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

#include "storagemanager.h"
#include <cmath>
#include <QMessageBox>
#include <util/util.h>
#include <util/threads/futures.h>
#include <util/sll/visitor.h>
#include <util/db/consistencychecker.h>
#include <interfaces/azoth/imessage.h>
#include <interfaces/azoth/iclentry.h>
#include <interfaces/azoth/iaccount.h>
#include <interfaces/azoth/irichtextmessage.h>
#include "storagethread.h"
#include "storage.h"
#include "core.h"

namespace LeechCraft
{
namespace Azoth
{
namespace ChatHistory
{
	StorageManager::StorageManager (Core *core)
	: StorageThread_ { std::make_shared<StorageThread> () }
	, Core_ { core }
	{
		StorageThread_->SetPaused (true);
		StorageThread_->SetAutoQuit (true);

		auto checker = new Util::ConsistencyChecker { Storage::GetDatabasePath (), "Azoth ChatHistory" };
		Util::Sequence (this, checker->StartCheck ()) >>
				[this] (const Util::ConsistencyChecker::CheckResult_t& result)
				{
					Util::Visit (result,
							[this] (const Util::ConsistencyChecker::Succeeded&) { StartStorage (); },
							[this] (const Util::ConsistencyChecker::Failed& failed)
							{
								qWarning () << Q_FUNC_INFO
										<< "db is broken, gonna repair";
								Util::Sequence (this, failed->DumpReinit ()) >>
										[this] (const Util::ConsistencyChecker::DumpResult_t& result)
										{
											Util::Visit (result,
													[] (const Util::ConsistencyChecker::DumpError& err)
													{
														QMessageBox::critical (nullptr,
																"Azoth ChatHistory",
																err.Error_);
													},
													[this] (const Util::ConsistencyChecker::DumpFinished& res)
													{
														HandleDumpFinished (res.OldFileSize_, res.NewFileSize_);
													});
										};
							});
				};
	}

	namespace
	{
		QString GetVisibleName (const ICLEntry *entry)
		{
			if (entry->GetEntryType () == ICLEntry::EntryType::PrivateChat)
			{
				const auto parent = entry->GetParentCLEntry ();
				return parent->GetEntryName () + "/" + entry->GetEntryName ();
			}
			else
				return entry->GetEntryName ();
		}
	}

	void StorageManager::Process (QObject *msgObj)
	{
		IMessage *msg = qobject_cast<IMessage*> (msgObj);
		if (msg->GetMessageType () != IMessage::Type::ChatMessage &&
			msg->GetMessageType () != IMessage::Type::MUCMessage)
			return;

		if (msg->GetBody ().isEmpty ())
			return;

		if (msg->GetDirection () == IMessage::Direction::Out &&
				msg->GetMessageType () == IMessage::Type::MUCMessage)
			return;

		const double secsDiff = msg->GetDateTime ().secsTo (QDateTime::currentDateTime ());
		if (msg->GetMessageType () == IMessage::Type::MUCMessage &&
				std::abs (secsDiff) >= 2)
			return;

		ICLEntry *entry = qobject_cast<ICLEntry*> (msg->ParentCLEntry ());
		if (!entry)
		{
			qWarning () << Q_FUNC_INFO
					<< "message's other part doesn't implement ICLEntry"
					<< msg->GetQObject ()
					<< msg->OtherPart ();
			return;
		}
		if (!Core_->IsLoggingEnabled (entry))
			return;

		const auto irtm = qobject_cast<IRichTextMessage*> (msgObj);

		AddLogItems (entry->GetParentAccount ()->GetAccountID (),
				entry->GetEntryID (),
				GetVisibleName (entry),
				{
					LogItem
					{
						msg->GetDateTime (),
						msg->GetDirection (),
						msg->GetBody (),
						msg->GetOtherVariant (),
						msg->GetMessageType (),
						irtm ? irtm->GetRichBody () : QString {},
						msg->GetEscapePolicy ()
					}
				},
				false);
	}

	void StorageManager::AddLogItems (const QString& accountId, const QString& entryId,
			const QString& visibleName, const QList<LogItem>& items, bool fuzzy)
	{
		StorageThread_->Schedule (&Storage::AddMessages,
				accountId,
				entryId,
				visibleName,
				items,
				fuzzy);
	}

	QFuture<IHistoryPlugin::MaxTimestampResult_t> StorageManager::GetMaxTimestamp (const QString& accId)
	{
		return StorageThread_->Schedule (&Storage::GetMaxTimestamp, accId);
	}

	QFuture<QStringList> StorageManager::GetOurAccounts ()
	{
		return StorageThread_->Schedule (&Storage::GetOurAccounts);
	}

	QFuture<UsersForAccountResult_t> StorageManager::GetUsersForAccount (const QString& accountID)
	{
		return StorageThread_->Schedule (&Storage::GetUsersForAccount, accountID);
	}

	QFuture<ChatLogsResult_t> StorageManager::GetChatLogs (const QString& accountId,
			const QString& entryId, int backpages, int amount)
	{
		return StorageThread_->Schedule (&Storage::GetChatLogs, accountId, entryId, backpages, amount);
	}

	QFuture<SearchResult_t> StorageManager::Search (const QString& accountId, const QString& entryId,
			const QString& text, int shift, bool cs)
	{
		return StorageThread_->Schedule (&Storage::Search,
				accountId, entryId, text, shift, cs);
	}

	QFuture<SearchResult_t> StorageManager::Search (const QString& accountId, const QString& entryId, const QDateTime& dt)
	{
		return StorageThread_->Schedule (&Storage::SearchDate,
				accountId, entryId, dt);
	}

	QFuture<DaysResult_t> StorageManager::GetDaysForSheet (const QString& accountId, const QString& entryId, int year, int month)
	{
		return StorageThread_->Schedule (&Storage::GetDaysForSheet,
				accountId, entryId, year, month);
	}

	void StorageManager::ClearHistory (const QString& accountId, const QString& entryId)
	{
		StorageThread_->Schedule (&Storage::ClearHistory, accountId, entryId);
	}

	void StorageManager::RegenUsersCache ()
	{
		StorageThread_->Schedule (&Storage::RegenUsersCache);
	}

	void StorageManager::StartStorage ()
	{
		StorageThread_->SetPaused (false);
		StorageThread_->start (QThread::LowestPriority);
		Util::Sequence (this, StorageThread_->Schedule (&Storage::Initialize)) >>
				[this] (const Storage::InitializationResult_t& res)
				{
					if (res.IsRight ())
					{
						StorageThread_->SetPaused (false);
						return;
					}

					HandleStorageError (res.GetLeft ());
				};
	}

	void StorageManager::HandleStorageError (const Storage::InitializationError_t& error)
	{
		Util::Visit (error,
				[] (const Storage::GeneralError& err)
				{
					QMessageBox::critical (nullptr,
							"Azoth ChatHistory",
							tr ("Unable to initialize permanent storage. %1.")
								.arg (err.ErrorText_));
				});
	}

	void StorageManager::HandleDumpFinished (qint64 oldSize, qint64 newSize)
	{
		StartStorage ();

		Util::Sequence (this, StorageThread_->Schedule (&Storage::GetAllHistoryCount)) >>
				[=] (const boost::optional<int>& count)
				{
					const auto& text = QObject::tr ("Finished restoring history database contents. "
							"Old file size: %1, new file size: %2, %3 records recovered. %4");
					const auto& greet = newSize > oldSize * 0.9 ?
							QObject::tr ("Yay, seems like most of the contents are intact!") :
							QObject::tr ("Sadly, seems like quite some history is lost.");

					QMessageBox::information (nullptr,
							"Azoth ChatHistory",
							text.arg (Util::MakePrettySize (oldSize))
								.arg (Util::MakePrettySize (newSize))
								.arg (count.get_value_or (0))
								.arg (greet));
				};
	}
}
}
}
