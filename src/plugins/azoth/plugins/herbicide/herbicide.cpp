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

#include "herbicide.h"
#include <QIcon>
#include <QAction>
#include <QTranslator>
#include <QSettings>
#include <QCoreApplication>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <util/util.h>
#include <util/sll/slotclosure.h>
#include <interfaces/azoth/iclentry.h>
#include <interfaces/azoth/imessage.h>
#include <interfaces/azoth/iaccount.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include "xmlsettingsmanager.h"
#include "confwidget.h"
#include "logger.h"

uint qHash (const QRegExp& rx)
{
	return qHash (rx.pattern ());
}

namespace LeechCraft
{
namespace Azoth
{
namespace Herbicide
{
	void Plugin::Init (ICoreProxy_ptr)
	{
		Util::InstallTranslator ("azoth_herbicide");

		SettingsDialog_ = std::make_shared<Util::XmlSettingsDialog> ();
		SettingsDialog_->RegisterObject (&XmlSettingsManager::Instance (),
				"azothherbicidesettings.xml");

		ConfWidget_ = new ConfWidget (&XmlSettingsManager::Instance ());
		SettingsDialog_->SetCustomWidget ("ConfWidget", ConfWidget_);

		Logger_ = new Logger;

		handleWhitelistChanged ();
		handleBlacklistChanged ();
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Azoth.Herbicide";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "Azoth Herbicide";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("A simple antispam plugin for Azoth.");
	}

	QIcon Plugin::GetIcon () const
	{
		static QIcon icon ("lcicons:/plugins/azoth/plugins/herbicide/resources/images/herbicide.svg");
		return icon;
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Plugins.Azoth.Plugins.IGeneralPlugin";
		return result;
	}

	Util::XmlSettingsDialog_ptr Plugin::GetSettingsDialog () const
	{
		return SettingsDialog_;
	}

	QList<QAction*> Plugin::CreateActions (IAccount *acc)
	{
		const auto configAction = new QAction { tr ("Configure antispam settings..."), this };
		new Util::SlotClosure<Util::NoDeletePolicy>
		{
			[this, acc] { ShowAccountAntispamConfig (acc); },
			configAction,
			SIGNAL (triggered ()),
			configAction
		};
		return { configAction };
	}

	bool Plugin::IsConfValid () const
	{
		if (!XmlSettingsManager::Instance ()
				.property ("EnableQuest").toBool ())
			return false;

		if (ConfWidget_->GetQuestion ().isEmpty () ||
				ConfWidget_->GetAnswers ().isEmpty ())
			return false;

		return true;
	}

	bool Plugin::IsEntryAllowed (QObject *entryObj) const
	{
		const auto entry = qobject_cast<ICLEntry*> (entryObj);
		if (!entry)
			return true;

		if ((entry->GetEntryFeatures () & ICLEntry::FMaskLongetivity) == ICLEntry::FPermanentEntry)
		{
			Logger_->LogEvent (Logger::Event::Granted, entry, "entry is permanent");
			return true;
		}

		if (AllowedEntries_.contains (entryObj))
		{
			Logger_->LogEvent (Logger::Event::Granted, entry, "entry has been previously allowed");
			return true;
		}

		const auto& id = entry->GetHumanReadableID ();

		const auto checkRxList = [&id] (const QSet<QRegExp>& rxList)
		{
			return std::any_of (rxList.begin (), rxList.end (),
					[&id] (const QRegExp& rx) { return rx.exactMatch (id); });
		};

		if (checkRxList (Whitelist_))
		{
			Logger_->LogEvent (Logger::Event::Granted, entry, "entry is in the whitelist");
			return true;
		}

		if (XmlSettingsManager::Instance ().property ("AskOnlyBL").toBool ())
		{
			const auto isBlacklisted = checkRxList (Blacklist_);
			if (isBlacklisted)
				Logger_->LogEvent (Logger::Event::Denied, entry, "entry is in the blacklist");
			else
				Logger_->LogEvent (Logger::Event::Granted, entry, "entry is not in the blacklist and blacklist-only mode is enabled");
			return !isBlacklisted;
		}

		Logger_->LogEvent (Logger::Event::Denied, entry, "fallback case");

		return false;
	}

	void Plugin::ChallengeEntry (IHookProxy_ptr proxy, QObject *entryObj)
	{
		auto entry = qobject_cast<ICLEntry*> (entryObj);
		AskedEntries_ << entryObj;

		auto greeting = XmlSettingsManager::Instance ().property ("QuestPrefix").toString ();
		if (!greeting.isEmpty ())
			greeting += "\n";
		const auto& text = greeting + ConfWidget_->GetQuestion ();

		Logger_->LogEvent (Logger::Event::Challenged, entry, text);

		const auto msg = entry->CreateMessage (IMessage::Type::ChatMessage, QString (), text);
		OurMessages_ << msg;
		msg->Send ();

		proxy->CancelDefault ();
	}

	void Plugin::GreetEntry (QObject *entryObj)
	{
		AllowedEntries_ << entryObj;

		AskedEntries_.remove (entryObj);

		auto entry = qobject_cast<ICLEntry*> (entryObj);

		const auto& text = XmlSettingsManager::Instance ().property ("QuestSuccessReply").toString ();
		const auto msg = entry->CreateMessage (IMessage::Type::ChatMessage, QString (), text);
		OurMessages_ << msg;
		msg->Send ();

		if (DeniedAuth_.contains (entryObj))
			QMetaObject::invokeMethod (entry->GetParentAccount ()->GetQObject (),
					"authorizationRequested",
					Q_ARG (QObject*, entryObj),
					Q_ARG (QString, DeniedAuth_.take (entryObj)));
	}

	namespace
	{
		class AccountSettingsManager : public Util::BaseSettingsManager
		{
			const QByteArray GroupName_;
		public:
			AccountSettingsManager (IAccount*, QObject*);
		protected:
			QSettings* BeginSettings () const override;
			void EndSettings (QSettings*) const override;
		};

		AccountSettingsManager::AccountSettingsManager (IAccount *acc, QObject *parent)
		: Util::BaseSettingsManager { parent }
		, GroupName_ { acc->GetAccountID () }
		{
			Util::BaseSettingsManager::Init ();
		}

		QSettings* AccountSettingsManager::BeginSettings () const
		{
			const auto settings = new QSettings (QCoreApplication::organizationName (),
					QCoreApplication::applicationName () + "_Azoth_Herbicide");
			settings->beginGroup (GroupName_);
			return settings;
		}

		void AccountSettingsManager::EndSettings (QSettings *settings) const
		{
			settings->endGroup ();
		}
	}

	void Plugin::ShowAccountAntispamConfig (IAccount *acc)
	{
		auto dia = new QDialog;
		dia->setLayout (new QVBoxLayout);

		auto xsd = new Util::XmlSettingsDialog;
		const auto accsm = new AccountSettingsManager { acc, xsd };

		xsd->RegisterObject (accsm, "azothherbicidesettings.xml");
		auto confWidget = new ConfWidget (accsm);
		xsd->SetCustomWidget ("ConfWidget", confWidget);

		dia->layout ()->addWidget (xsd->GetWidget ());

		auto buttons = new QDialogButtonBox { QDialogButtonBox::Ok | QDialogButtonBox::Cancel };
		dia->layout ()->addWidget (buttons);

		connect (buttons,
				SIGNAL (accepted ()),
				dia,
				SLOT (accept ()));
		connect (buttons,
				SIGNAL (accepted ()),
				xsd,
				SLOT (accept ()));
		connect (buttons,
				SIGNAL (rejected ()),
				dia,
				SLOT (reject ()));

		dia->setAttribute (Qt::WA_DeleteOnClose);
		dia->open ();
		connect (dia,
				SIGNAL (finished (int)),
				xsd,
				SLOT (deleteLater ()));
	}

	void Plugin::hookGotAuthRequest (IHookProxy_ptr proxy, QObject *entry, QString msg)
	{
		if (!IsConfValid ())
			return;

		if (!XmlSettingsManager::Instance ().property ("EnableForAuths").toBool ())
			return;

		if (IsEntryAllowed (entry))
			return;

		if (!AskedEntries_.contains (entry))
		{
			ChallengeEntry (proxy, entry);
			DeniedAuth_ [entry] = msg;
		}
	}

	void Plugin::hookGotMessage (LeechCraft::IHookProxy_ptr proxy,
				QObject *message)
	{
		if (!IsConfValid ())
			return;

		const auto msg = qobject_cast<IMessage*> (message);
		if (!msg)
		{
			qWarning () << Q_FUNC_INFO
					<< message
					<< "doesn't implement IMessage";
			return;
		}

		if (OurMessages_.contains (msg))
		{
			OurMessages_.remove (msg);
			proxy->CancelDefault ();
			return;
		}

		if (msg->GetMessageType () != IMessage::Type::ChatMessage)
			return;

		QObject *entryObj = msg->OtherPart ();
		ICLEntry *entry = qobject_cast<ICLEntry*> (entryObj);

		if (IsEntryAllowed (entryObj))
			return;

		if (!AskedEntries_.contains (entryObj))
			ChallengeEntry (proxy, entryObj);
		else if (ConfWidget_->GetAnswers ().contains (msg->GetBody ().toLower ()))
		{
			Logger_->LogEvent (Logger::Event::Succeeded, entry, msg->GetBody ());
			GreetEntry (entryObj);
		}
		else
		{
			Logger_->LogEvent (Logger::Event::Failed, entry, msg->GetBody ());

			const auto& text = XmlSettingsManager::Instance ().property ("QuestFailureReply").toString ();
			const auto msg = entry->CreateMessage (IMessage::Type::ChatMessage, QString (), text);
			OurMessages_ << msg;
			msg->Send ();

			proxy->CancelDefault ();
		}
	}

	namespace
	{
		QSet<QRegExp> GetRegexps (const QByteArray& prop)
		{
			QSet<QRegExp> result;

			const auto& strings = XmlSettingsManager::Instance ().property (prop).toStringList ();
			for (auto string : strings)
			{
				string = string.trimmed ();
				if (string.isEmpty ())
					continue;
				result << QRegExp (string);
			}

			return result;
		}
	}

	void Plugin::handleWhitelistChanged ()
	{
		Whitelist_ = GetRegexps ("WhitelistRegexps");
	}

	void Plugin::handleBlacklistChanged ()
	{
		Blacklist_ = GetRegexps ("BlacklistRegexps");
	}
}
}
}

LC_EXPORT_PLUGIN (leechcraft_azoth_herbicide, LeechCraft::Azoth::Herbicide::Plugin);
