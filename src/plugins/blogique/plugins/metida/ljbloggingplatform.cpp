/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#include "ljbloggingplatform.h"
#include <QIcon>
#include <QInputDialog>
#include <QSettings>
#include <QtDebug>
#include <QTimer>
#include <QMainWindow>
#include <QDomElement>
#include <interfaces/core/irootwindowsmanager.h>
#include <util/passutils.h>
#include "core.h"
#include "ljaccount.h"
#include "ljaccountconfigurationwidget.h"
#include "postoptionswidget.h"
#include "localstorage.h"
#include "recentcommentssidewidget.h"
#include "xmlsettingsmanager.h"
#include "polldialog.h"

namespace LeechCraft
{
namespace Blogique
{
namespace Metida
{
	LJBloggingPlatform::LJBloggingPlatform (QObject *parent)
	: QObject (parent)
	, ParentBlogginPlatfromPlugin_ (parent)
	, PluginProxy_ (0)
	, LJUser_ (new QAction (Core::Instance ().GetCoreProxy ()->GetIcon ("user-properties"),
			tr ("Add LJ user"), this))
	, LJPoll_ (new QAction (Core::Instance ().GetCoreProxy ()->GetIcon ("office-chart-pie"),
			tr ("Create poll"), this))
	, LJCut_ (new QAction (Core::Instance ().GetCoreProxy ()->GetIcon ("user-properties"),
			tr ("Insert LJ cut"), this))
	, FirstSeparator_ (new QAction (this))
	, MessageCheckingTimer_ (new QTimer (this))
	, CommentsCheckingTimer_ (new QTimer (this))
	{
		FirstSeparator_->setSeparator (true);

		connect (LJUser_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleAddLJUser ()));
		connect (LJPoll_,
				SIGNAL (triggered ()),
				this,
				SLOT (handleAddLJPoll ()));

		connect (MessageCheckingTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (checkForMessages ()));
		connect (CommentsCheckingTimer_,
				SIGNAL (timeout ()),
				this,
				SLOT (checkForComments ()));

		XmlSettingsManager::Instance ().RegisterObject ("CheckingInboxEnabled",
				this, "handleMessageChecking");
		XmlSettingsManager::Instance ().RegisterObject ("CheckingCommentsEnabled",
				this, "handleCommentsChecking");
		handleMessageChecking ();
		handleCommentsChecking ();
	}

	QObject* LJBloggingPlatform::GetQObject ()
	{
		return this;
	}

	IBloggingPlatform::BloggingPlatfromFeatures LJBloggingPlatform::GetFeatures () const
	{
		return BPFSupportsProfiles | BPFSelectablePostDestination | BPFSupportsBackup;
	}

	QObjectList LJBloggingPlatform::GetRegisteredAccounts ()
	{
		QObjectList result;
		Q_FOREACH (auto acc, LJAccounts_)
			result << acc;
		return result;
	}

	QObject* LJBloggingPlatform::GetParentBloggingPlatformPlugin () const
	{
		return ParentBlogginPlatfromPlugin_;
	}

	QString LJBloggingPlatform::GetBloggingPlatformName () const
	{
		return "LiveJournal";
	}

	QIcon LJBloggingPlatform::GetBloggingPlatformIcon () const
	{
		return QIcon ("lcicons:/plugins/blogique/plugins/metida/resources/images/livejournalicon.svg");
	}

	QByteArray LJBloggingPlatform::GetBloggingPlatformID () const
	{
		return "Blogique.Metida.LiveJournal";
	}

	QList<QWidget*> LJBloggingPlatform::GetAccountRegistrationWidgets (IBloggingPlatform::AccountAddOptions)
	{
		QList<QWidget*> result;
		result << new LJAccountConfigurationWidget ();
		return result;
	}

	void LJBloggingPlatform::RegisterAccount (const QString& name,
			const QList<QWidget*>& widgets)
	{
		auto w = qobject_cast<LJAccountConfigurationWidget*> (widgets.value (0));
		if (!w)
		{
			qWarning () << Q_FUNC_INFO
					<< "got invalid widgets"
					<< widgets;
			return;
		}

		LJAccount *account = new LJAccount (name, this);
		account->FillSettings (w);

		const QString& pass = w->GetPassword ();
		if (!pass.isEmpty ())
			Util::SavePassword (pass,
					"org.LeechCraft.Blogique.PassForAccount/" + account->GetAccountID (),
					&Core::Instance ());

		LJAccounts_ << account;
		saveAccounts ();
		emit accountAdded (account);
		account->Init ();
		Core::Instance ().GetLocalStorage ()->AddAccount (account->GetAccountID ());
	}

	void LJBloggingPlatform::RemoveAccount (QObject *account)
	{
		LJAccount *acc = qobject_cast<LJAccount*> (account);
		if (LJAccounts_.removeAll (acc))
		{
			emit accountRemoved (account);
			Core::Instance ().GetLocalStorage ()->RemoveAccount (acc->GetAccountID ());
			account->deleteLater ();
			saveAccounts ();
		}
	}

	QList<QAction*> LJBloggingPlatform::GetEditorActions () const
	{
		return { FirstSeparator_, LJUser_, LJPoll_ };
	}

	QList<InlineTagInserter> LJBloggingPlatform::GetInlineTagInserters () const
	{
		return { InlineTagInserter { "lj-cut", QVariantMap (), [] (QAction *action)
				{
					action->setText ("Insert cut");
					action->setIcon (Core::Instance ().GetCoreProxy ()->
							GetIcon ("distribute-vertical-equal"));
				} } };
	}

	QList<QWidget*> LJBloggingPlatform::GetBlogiqueSideWidgets () const
	{
		return { new PostOptionsWidget, new RecentCommentsSideWidget };
	}

	void LJBloggingPlatform::SetPluginProxy (QObject *proxy)
	{
		PluginProxy_ = proxy;
	}

	void LJBloggingPlatform::Prepare ()
	{
		RestoreAccounts ();
	}

	void LJBloggingPlatform::Release ()
	{
		saveAccounts ();
	}

	IAdvancedHTMLEditor::CustomTags_t LJBloggingPlatform::GetCustomTags () const
	{
		IAdvancedHTMLEditor::CustomTags_t tags;

		IAdvancedHTMLEditor::CustomTag ljUserTag;
		ljUserTag.TagName_ = "lj";
		ljUserTag.ToKnown_ = [] (QDomElement& elem) -> void
		{
			const auto& user = elem.attribute ("user");
			elem.setTagName ("span");

			QDomElement linkElem = elem.ownerDocument ().createElement ("a");
			linkElem.setAttribute ("href", QString ("http://%1.livejournal.com/profile").arg (user));
			linkElem.setAttribute ("target", "_blank");

			QDomElement imgElem = elem.ownerDocument ().createElement ("img");
			imgElem.setAttribute ("src", "http://l-stat.livejournal.com/img/userinfo.gif?v=17080");
			linkElem.appendChild (imgElem);

			QDomElement nameElem = elem.ownerDocument ().createElement ("a");
			nameElem.setAttribute ("href", QString ("http://%1.livejournal.com/profile").arg (user));
			nameElem.setAttribute ("target", "_blank");
			nameElem.setAttribute ("id", "nameLink");
			nameElem.appendChild (elem.ownerDocument ().createTextNode (user));

			elem.appendChild (linkElem);
			elem.appendChild (nameElem);

			elem.removeAttribute ("user");
		};
		ljUserTag.FromKnown_ = [] (QDomElement& elem) -> bool
		{
			auto aElem = elem.firstChildElement ("a");
			while (!aElem.isNull ())
			{
				if (aElem.attribute ("id") == "nameLink")
					break;

				aElem = aElem.nextSiblingElement ("a");
			}
			if (aElem.isNull ())
				return false;
			const auto& username = aElem.text ();

			const auto& children = elem.childNodes ();
			while (!children.isEmpty ())
				elem.removeChild (children.at (0));

			elem.setTagName ("lj");
			elem.setAttribute ("user", username);

			return true;
		};
		tags << ljUserTag;

		IAdvancedHTMLEditor::CustomTag ljCutTag;
		ljCutTag.TagName_ = "lj-cut";
		ljCutTag.ToKnown_ = [] (QDomElement& elem)
		{
			elem.setTagName ("div");
			elem.setAttribute ("style", "overflow:auto;border-width:3px;border-style:dotted;margin-left:3em;padding:2em 2em;");
		};
		ljCutTag.FromKnown_ = [] (QDomElement& elem) -> bool
		{
			elem.setTagName ("lj-cut");
			return true;
		};

		tags << ljCutTag;
/*
		IAdvancedHTMLEditor::CustomTag ljPollTag;
		ljPollTag.TagName_ = "lj-poll";
		ljPollTag.ToKnown_ = [] (QDomElement& elem)
		{
			elem.setTagName ("div");
			elem.setAttribute ("style", "overflow:auto;border-width:3px;border-style:dashed;margin-left:3em;padding:2em 2em;");
			auto textElem = elem.ownerDocument ().createTextNode ("");
			elem.appendChild (textElem);
		};
		ljPollTag.FromKnown_ = [] (QDomElement& elem) -> bool
		{
			elem.setTagName ("lj-poll");
			return true;
		};

		tags << ljPollTag;*/


		return tags;
	}

	void LJBloggingPlatform::RestoreAccounts ()
	{
		QSettings settings (QSettings::IniFormat, QSettings::UserScope,
				QCoreApplication::organizationName (),
				QCoreApplication::applicationName () +
						"_Blogique_Metida_Accounts");
		int size = settings.beginReadArray ("Accounts");
		for (int i = 0; i < size; ++i)
		{
			settings.setArrayIndex (i);
			QByteArray data = settings.value ("SerializedData").toByteArray ();
			LJAccount *acc = LJAccount::Deserialize (data, this);
			if (!acc)
			{
				qWarning () << Q_FUNC_INFO
						<< "unserializable acount"
						<< i;
				continue;
			}
			LJAccounts_ << acc;
			emit accountAdded (acc);

			acc->Init ();
			Core::Instance ().GetLocalStorage ()->AddAccount (acc->GetAccountID ());
		}
		settings.endArray ();
	}

	void LJBloggingPlatform::saveAccounts ()
	{
		QSettings settings (QSettings::IniFormat, QSettings::UserScope,
				QCoreApplication::organizationName (),
				QCoreApplication::applicationName () +
						"_Blogique_Metida_Accounts");
		settings.beginWriteArray ("Accounts");
		for (int i = 0, size = LJAccounts_.size (); i < size; ++i)
		{
			settings.setArrayIndex (i);
			settings.setValue ("SerializedData",
					LJAccounts_.at (i)->Serialize ());
		}
		settings.endArray ();
		settings.sync ();
	}

	void LJBloggingPlatform::handleAddLJUser ()
	{
		auto rootWM = Core::Instance ().GetCoreProxy ()->GetRootWindowsManager ();
		QString name = QInputDialog::getText (rootWM->GetPreferredWindow (),
				tr ("Add LJ User"),
				tr ("Enter LJ user name"));
		if (name.isEmpty ())
			return;

		emit insertTag (QString ("<lj user=\"%1\" />").arg (name));
	}

	void LJBloggingPlatform::handleAddLJPoll ()
	{
		PollDialog pollDlg;
		if (pollDlg.exec () == QDialog::Rejected)
			return;

		QStringList pqParts;
		QString pqPart = QString ("<lj-pq type=\"%1\" %2>%3%4</lj-pq>");
		bool isPqParam = false;
		for (const auto& pollType : pollDlg.GetPollTypes ())
		{
			const auto& map = pollDlg.GetPollFields (pollType);
			QStringList pqParams;
			if (pollType == "check" ||
					pollType == "radio" ||
					pollType == "drop")
			{
				isPqParam = false;
				for (const auto& value : map.values ())
					pqParams << QString ("<lj-pi>%1</lj-pi>")
							.arg (value.toString ());
			}
			else
			{
				isPqParam = true;
				for (const auto& key : map.keys ())
					pqParams << QString ("%1=\"%2\"")
							.arg (key)
							.arg (map [key].toString ());
			}
			pqParts << pqPart
					.arg (pollType)
					.arg (isPqParam ? pqParams.join (" ") : QString ())
					.arg (pollDlg.GetPollQuestion (pollType))
					.arg (!isPqParam ? pqParams.join (" ") : QString ());
		}

		QString pollPart = QString ("<lj-poll name=\"%1\" whovote=\"%2\" whoview=\"%3\">%4</lj-poll>")
				.arg (pollDlg.GetPollName ())
				.arg (pollDlg.GetWhoCanVote ())
				.arg (pollDlg.GetWhoCanView ())
				.arg (pqParts.join (""));
		emit insertTag (pollPart);
	}

	void LJBloggingPlatform::handleAccountValidated (bool validated)
	{
		IAccount *acc = qobject_cast<IAccount*> (sender ());
		if (!acc)
		{
			qWarning () << Q_FUNC_INFO
					<< sender ()
					<< "is not an IAccount";;
			return;
		}

		emit accountValidated (acc->GetQObject (), validated);
	}

	void LJBloggingPlatform::handleMessageChecking ()
	{
		if (XmlSettingsManager::Instance ().Property ("CheckingInboxEnabled", true).toBool ())
			MessageCheckingTimer_->start (XmlSettingsManager::Instance ()
					.property ("UpdateInboxInterval").toInt () * 1000);
		else if (MessageCheckingTimer_->isActive ())
			MessageCheckingTimer_->stop ();
	}

	void LJBloggingPlatform::handleCommentsChecking ()
	{
		if (XmlSettingsManager::Instance ().Property ("CheckingCommentsEnabled", true).toBool ())
			CommentsCheckingTimer_->start (XmlSettingsManager::Instance ()
					.property ("UpdateCommentsInterval").toInt () * 60 * 1000);
		else if (CommentsCheckingTimer_->isActive ())
			CommentsCheckingTimer_->stop ();
	}

	void LJBloggingPlatform::checkForMessages ()
	{
		for (auto account : LJAccounts_)
			account->RequestInbox ();
	}

	void LJBloggingPlatform::checkForComments ()
	{
		for (auto account : LJAccounts_)
			account->RequestRecentComments ();
	}

}
}
}
