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

#include "coreinstanceobject.h"
#include <QIcon>
#include <QDir>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyleFactory>
#include <util/util.h>
#include <util/shortcuts/shortcutmanager.h>
#include <util/sys/paths.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include "xmlsettingsmanager.h"
#include "pluginmanagerdialog.h"
#include "iconthemeengine.h"
#include "colorthemeengine.h"
#include "tagsviewer.h"
#include "core.h"
#include "settingstab.h"
#include "coreplugin2manager.h"
#include "acceptlangwidget.h"
#include "shortcutmanager.h"
#include "coreproxy.h"

namespace LeechCraft
{
	namespace
	{
		QMap<QString, QString> GetInstalledLanguages ()
		{
			QStringList filenames;

	#ifdef Q_OS_WIN32
			filenames << QDir (QCoreApplication::applicationDirPath () + "/translations")
					.entryList (QStringList ("leechcraft_*.qm"));
	#elif defined (Q_OS_MAC)
			filenames << QDir (QCoreApplication::applicationDirPath () + "/../Resources/translations")
					.entryList (QStringList ("leechcraft_*.qm"));
	#elif defined (INSTALL_PREFIX)
			filenames << QDir (INSTALL_PREFIX "/share/leechcraft/translations")
					.entryList (QStringList ("leechcraft_*.qm"));
	#else
			filenames << QDir ("/usr/local/share/leechcraft/translations")
					.entryList (QStringList ("leechcraft_*.qm"));
			filenames << QDir ("/usr/share/leechcraft/translations")
					.entryList (QStringList ("leechcraft_*.qm"));
	#endif

			int length = QString ("leechcraft_").size ();
			QMap<QString, QString> languages;
			Q_FOREACH (QString fname, filenames)
			{
				fname = fname.mid (length);
				fname.chop (3);					// for .qm
				QStringList parts = fname.split ('_', QString::SkipEmptyParts);

				QString language;
				Q_FOREACH (const QString& part, parts)
				{
					if (part.size () != 2)
						continue;
					if (!part.at (0).isLower ())
						continue;

					QLocale locale (part);
					if (locale.language () == QLocale::C)
						continue;

					language = QLocale::languageToString (locale.language ());

					while (part != parts.at (0))
						parts.pop_front ();

					languages [language] = parts.join ("_");
					break;
				}
			}

			return languages;
		}

		QAbstractItemModel* GetInstalledLangsModel ()
		{
			QMap<QString, QString> languages = GetInstalledLanguages ();

			QStandardItemModel *model = new QStandardItemModel ();
			QStandardItem *systemItem = new QStandardItem (QObject::tr ("System"));
			systemItem->setData ("system", Qt::UserRole);
			model->appendRow (systemItem);
			Q_FOREACH (const QString& language, languages.keys ())
			{
				QStandardItem *item = new QStandardItem (language);
				item->setData (languages [language], Qt::UserRole);
				model->appendRow (item);
			}
			return model;
		}
	}

	CoreInstanceObject::CoreInstanceObject (QObject *parent)
	: QObject (parent)
	, XmlSettingsDialog_ (new Util::XmlSettingsDialog ())
	, SettingsTab_ (new SettingsTab)
	, CorePlugin2Manager_ (new CorePlugin2Manager)
	, ShortcutManager_ (new ShortcutManager)
	, CoreShortcutManager_ (new Util::ShortcutManager (ICoreProxy_ptr (new CoreProxy)))
	{
		CoreShortcutManager_->SetObject (this);

		XmlSettingsDialog_->RegisterObject (XmlSettingsManager::Instance (),
				"coresettings.xml");
		connect (XmlSettingsDialog_.get (),
				SIGNAL (pushButtonClicked (QString)),
				this,
				SLOT (handleSettingsButton (QString)));

		connect (SettingsTab_,
				SIGNAL (remove (QWidget*)),
				this,
				SIGNAL (removeTab (QWidget*)));
	}

	void CoreInstanceObject::Init (ICoreProxy_ptr)
	{
#ifndef Q_OS_MAC
		const auto sysModifier = Qt::CTRL;
#else
		const auto sysModifier = Qt::ALT;
#endif
		CoreShortcutManager_->RegisterActionInfo ("SwitchToPrevTab",
				{
					tr ("Switch to previously active tab"),
					sysModifier + Qt::Key_Space,
					CoreProxy ().GetIcon ("edit-undo")
				});
		CoreShortcutManager_->RegisterActionInfo ("FullScreen",
				{
					tr ("Toggle fullscreen"),
					QString ("F11"),
					CoreProxy ().GetIcon ("view-fullscreen")
				});
		CoreShortcutManager_->RegisterActionInfo ("CloseTab",
				{
					tr ("Close tab"),
					QString ("Ctrl+W"),
					CoreProxy ().GetIcon ("tab-close")
				});
		CoreShortcutManager_->RegisterActionInfo ("SwitchToLeftTab",
				{
					tr ("Switch to tab to the left"),
					QString ("Ctrl+PgUp"),
					CoreProxy ().GetIcon ("go-previous")
				});
		CoreShortcutManager_->RegisterActionInfo ("SwitchToRightTab",
				{
					tr ("Switch to tab to the right"),
					QString ("Ctrl+PgDown"),
					CoreProxy ().GetIcon ("go-next")
				});
		CoreShortcutManager_->RegisterActionInfo ("Settings",
				{
					tr ("Settings"),
					QString ("Ctrl+P"),
					CoreProxy ().GetIcon ("configure")
				});
		CoreShortcutManager_->RegisterActionInfo ("Quit",
				{
					tr ("Quit LeechCraft"),
					QString ("F10"),
					CoreProxy ().GetIcon ("application-exit")
				});

		Classes_ << SettingsTab_->GetTabClassInfo ();

		XmlSettingsDialog_->SetCustomWidget ("PluginManager", new PluginManagerDialog);
		XmlSettingsDialog_->SetCustomWidget ("TagsViewer", new TagsViewer);

		XmlSettingsDialog_->SetDataSource ("Language",
			GetInstalledLangsModel ());

		XmlSettingsDialog_->SetDataSource ("IconSet",
				new QStringListModel (IconThemeEngine::Instance ().ListIcons ()));
		XmlSettingsManager::Instance ()->RegisterObject ("IconSet", this, "updateIconSet");
		updateIconSet ();

		QStringList pluginsIconsets;
		for (const auto& cand : Util::GetPathCandidates (Util::SysPath::Share, "global_icons/plugins/"))
			pluginsIconsets << QDir (cand).entryList (QDir::Dirs | QDir::NoDotAndDotDot);
		pluginsIconsets.sort ();
		pluginsIconsets.prepend ("Default");
		pluginsIconsets.removeDuplicates ();
		XmlSettingsDialog_->SetDataSource ("PluginsIconset",
				new QStringListModel (pluginsIconsets));

		XmlSettingsDialog_->SetDataSource ("ColorTheme",
				ColorThemeEngine::Instance ().GetThemesModel ());
		XmlSettingsManager::Instance ()->RegisterObject ("ColorTheme", this, "updateColorTheme");
		updateColorTheme ();

		QStringList appQStype = QStyleFactory::keys ();
		appQStype.prepend ("Default");
		XmlSettingsDialog_->SetDataSource ("AppQStyle", new QStringListModel (appQStype));

		XmlSettingsDialog_->SetCustomWidget ("ShortcutManager", ShortcutManager_);
		XmlSettingsDialog_->SetCustomWidget ("AcceptLanguages", new AcceptLangWidget);

		const auto& lang = XmlSettingsManager::Instance ()->property ("Language").toString ();
		if (lang != "system")
			QLocale::setDefault ({ lang });
	}

	void CoreInstanceObject::SecondInit ()
	{
		BuildNewTabModel ();
		SettingsTab_->Initialize ();

#ifdef STRICT_LICENSING
		QTimer::singleShot (10000,
				this,
				SLOT (notifyLicensing ()));
#endif
	}

	void CoreInstanceObject::Release ()
	{
		XmlSettingsDialog_.reset ();
	}

	QByteArray CoreInstanceObject::GetUniqueID () const
	{
		return "org.LeechCraft.CoreInstance";
	}

	QString CoreInstanceObject::GetName () const
	{
		return "LeechCraft";
	}

	QString CoreInstanceObject::GetInfo () const
	{
		return tr ("LeechCraft Core module.");
	}

	QIcon CoreInstanceObject::GetIcon () const
	{
		return QIcon ("lcicons:/resources/images/leechcraft.svg");
	}

	Util::XmlSettingsDialog_ptr CoreInstanceObject::GetSettingsDialog () const
	{
		return XmlSettingsDialog_;
	}

	TabClasses_t CoreInstanceObject::GetTabClasses () const
	{
		return Classes_;
	}

	void CoreInstanceObject::TabOpenRequested (const QByteArray& tabClass)
	{
		if (tabClass == "org.LeechCraft.SettingsPane")
		{
			emit addNewTab (tr ("Settings"), SettingsTab_);
			emit raiseTab (SettingsTab_);
		}
		else
			qWarning () << Q_FUNC_INFO
					<< "unknown tab class"
					<< tabClass;
	}

	QMap<QString, ActionInfo> CoreInstanceObject::GetActionInfo () const
	{
		return CoreShortcutManager_->GetActionInfo ();
	}

	void CoreInstanceObject::SetShortcut (const QString& id, const QKeySequences_t& sequences)
	{
		CoreShortcutManager_->SetShortcut (id, sequences);
	}

	QSet<QByteArray> CoreInstanceObject::GetExpectedPluginClasses () const
	{
		QSet<QByteArray> result;
		result << "org.LeechCraft.Core.Plugins/1.0";
		return result;
	}

	void CoreInstanceObject::AddPlugin (QObject *plugin)
	{
		CorePlugin2Manager_->AddPlugin (plugin);
	}

	CorePlugin2Manager* CoreInstanceObject::GetCorePluginManager () const
	{
		return CorePlugin2Manager_;
	}

	SettingsTab* CoreInstanceObject::GetSettingsTab () const
	{
		return SettingsTab_;
	}

	IShortcutProxy* CoreInstanceObject::GetShortcutProxy () const
	{
		return ShortcutManager_;
	}

	ShortcutManager* CoreInstanceObject::GetShortcutManager () const
	{
		return ShortcutManager_;
	}

	Util::ShortcutManager* CoreInstanceObject::GetCoreShortcutManager () const
	{
		return CoreShortcutManager_;
	}

	void CoreInstanceObject::BuildNewTabModel ()
	{
		QStandardItemModel *newTabsModel = new QStandardItemModel (this);
		QStandardItem *defaultItem = new QStandardItem (tr ("Context-dependent"));
		defaultItem->setData ("contextdependent", Qt::UserRole);
		newTabsModel->appendRow (defaultItem);

		QObjectList multitabs = Core::Instance ()
				.GetPluginManager ()->GetAllCastableRoots<IHaveTabs*> ();
		Q_FOREACH (QObject *object, multitabs)
		{
			IInfo *ii = qobject_cast<IInfo*> (object);
			IHaveTabs *iht = qobject_cast<IHaveTabs*> (object);
			Q_FOREACH (const TabClassInfo& info, iht->GetTabClasses ())
			{
				QStandardItem *item =
						new QStandardItem (ii->GetName () + ": " + info.VisibleName_);
				item->setToolTip (info.Description_);
				item->setIcon (info.Icon_);
				item->setData (ii->GetUniqueID () + '|' + info.TabClass_, Qt::UserRole);
				newTabsModel->appendRow (item);
			}
		}

		qDebug () << Q_FUNC_INFO
				<< "DefaultNewTab"
				<< XmlSettingsManager::Instance ()->property ("DefaultNewTab");

		Core::Instance ().GetCoreInstanceObject ()->
				GetSettingsDialog ()->SetDataSource ("DefaultNewTab", newTabsModel);
	}

	void CoreInstanceObject::handleSettingsButton (const QString& name)
	{
		auto pm = Core::Instance ().GetPluginManager ();
		if (name == "EnableAllPlugins")
			pm->SetAllPlugins (Qt::Checked);
		else if (name == "DisableAllPlugins")
			pm->SetAllPlugins (Qt::Unchecked);
	}

	void CoreInstanceObject::updateIconSet ()
	{
		IconThemeEngine::Instance ().UpdateIconSet (findChildren<QAction*> ());
		IconThemeEngine::Instance ().UpdateIconSet (findChildren<QTabWidget*> ());
	}

	void CoreInstanceObject::updateColorTheme ()
	{
		const auto& theme = XmlSettingsManager::Instance ()->
				property ("ColorTheme").toString ();
		ColorThemeEngine::Instance ().SetTheme (theme);
	}

#ifdef STRICT_LICENSING
	void CoreInstanceObject::notifyLicensing ()
	{
		if (XmlSettingsManager::Instance ()->
				Property ("NotifiedLicensing", false).toBool ())
			return;

		const QString& str = tr ("Due to licensing issues, some artwork "
				"may have been removed from this package. Consider "
				"using the LackMan plugin to install that artwork.");
		emit gotEntity (Util::MakeNotification ("LeechCraft", str, PWarning_));
		XmlSettingsManager::Instance ()->setProperty ("NotifiedLicensing", true);
	}
#endif
}
