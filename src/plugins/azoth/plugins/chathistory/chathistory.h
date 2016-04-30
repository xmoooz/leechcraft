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

#pragma once

#include <memory>
#include <QObject>
#include <QAction>
#include <interfaces/iinfo.h>
#include <interfaces/iplugin2.h>
#include <interfaces/iactionsexporter.h>
#include <interfaces/ihavetabs.h>
#include <interfaces/ihavesettings.h>
#include <interfaces/core/ihookproxy.h>
#include <interfaces/azoth/imessage.h>
#include <interfaces/azoth/ihistoryplugin.h>
#include "core.h"

class QTranslator;

namespace LeechCraft
{
namespace Azoth
{
namespace ChatHistory
{
	class ChatHistoryWidget;

	class Plugin : public QObject
				 , public IInfo
				 , public IPlugin2
				 , public IActionsExporter
				 , public IHaveTabs
				 , public IHaveSettings
				 , public IHistoryPlugin
	{
		Q_OBJECT
		Q_INTERFACES (IInfo
				IPlugin2
				IActionsExporter
				IHaveTabs
				IHaveSettings
				LeechCraft::Azoth::IHistoryPlugin)

		LC_PLUGIN_METADATA ("org.LeechCraft.Azoth.ChatHistory")

		Util::XmlSettingsDialog_ptr XSD_;

		std::shared_ptr<STGuard<Core>> Guard_;
		std::shared_ptr<QTranslator> Translator_;
		QAction *ActionHistory_;
		QHash<QObject*, QAction*> Entry2ActionHistory_;
		QHash<QObject*, QAction*> Entry2ActionEnableHistory_;

		QAction *SeparatorAction_;
	public:
		void Init (ICoreProxy_ptr);
		void SecondInit ();
		QByteArray GetUniqueID () const;
		void Release ();
		QString GetName () const;
		QString GetInfo () const;
		QIcon GetIcon () const;

		// IPlugin2
		QSet<QByteArray> GetPluginClasses () const;

		// IActionsExporter
		QList<QAction*> GetActions (ActionsEmbedPlace) const;
		QMap<QString, QList<QAction*>> GetMenuActions () const;

		// IHaveTabs
		TabClasses_t GetTabClasses () const;
		void TabOpenRequested (const QByteArray&);

		// IHaveSettings
		Util::XmlSettingsDialog_ptr GetSettingsDialog () const;

		// IHistoryPlugin
		bool IsHistoryEnabledFor (QObject*) const;
		void RequestLastMessages (QObject*, int);
		void AddRawMessage (const QString&, const QString&, const QString&, const HistoryItem&);
	private:
		void InitWidget (ChatHistoryWidget*);

		void HandleGotChatLogs (const QPointer<QObject>&, const ChatLogsResult_t&);
	public slots:
		void initPlugin (QObject*);

		void hookEntryActionAreasRequested (LeechCraft::IHookProxy_ptr proxy,
				QObject *action,
				QObject *entry);
		void hookEntryActionsRemoved (LeechCraft::IHookProxy_ptr proxy,
				QObject *entry);
		void hookEntryActionsRequested (LeechCraft::IHookProxy_ptr proxy,
				QObject *entry);
		void hookGotMessage2 (LeechCraft::IHookProxy_ptr proxy,
				QObject *message);
	private slots:

		void handlePushButton (const QString&);

		void handleHistoryRequested ();
		void handleEntryHistoryRequested ();
		void handleEntryEnableHistoryRequested (bool);
	signals:
		void addNewTab (const QString&, QWidget*);
		void removeTab (QWidget*);
		void changeTabName (QWidget*, const QString&);
		void changeTabIcon (QWidget*, const QIcon&);
		void changeTooltip (QWidget*, QWidget*);
		void statusBarChanged (QWidget*, const QString&);
		void raiseTab (QWidget*);

		void gotLastMessages (QObject*, const QList<QObject*>&);

		void gotActions (QList<QAction*>, LeechCraft::ActionsEmbedPlace);

		void gotEntity (const LeechCraft::Entity&);
	};
}
}
}
