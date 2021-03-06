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
#include <deque>
#include <QMainWindow>
#include <QToolBar>
#include <interfaces/iinfo.h>
#include <interfaces/idownload.h>
#include <interfaces/ientityhandler.h>
#include <interfaces/ijobholder.h>
#include <interfaces/iimportexport.h>
#include <interfaces/itaggablejobs.h>
#include <interfaces/ihavesettings.h>
#include <interfaces/ihaveshortcuts.h>
#include <interfaces/ihavetabs.h>
#include <interfaces/istartupwizard.h>
#include <interfaces/iactionsexporter.h>
#include <interfaces/ihavediaginfo.h>
#include <util/tags/tagscompleter.h>
#include <xmlsettingsdialog/xmlsettingsdialog.h>
#include "tabwidget.h"
#include "torrentinfo.h"

class QTimer;
class QToolBar;
class QComboBox;
class QTabWidget;
class QTranslator;
class QSortFilterProxyModel;

namespace LeechCraft
{
namespace BitTorrent
{
	class AddTorrent;
	class RepresentationModel;
	class SpeedSelectorAction;
	class TorrentTab;

	class TorrentPlugin : public QObject
						, public IInfo
						, public IDownload
						, public IJobHolder
						, public IImportExport
						, public ITaggableJobs
						, public IHaveSettings
						, public IHaveShortcuts
						, public IHaveTabs
						, public IStartupWizard
						, public IActionsExporter
						, public IHaveDiagInfo
	{
		Q_OBJECT

		Q_INTERFACES (IInfo
				IDownload
				IJobHolder
				IImportExport
				ITaggableJobs
				IHaveSettings
				IHaveShortcuts
				IHaveTabs
				IStartupWizard
				IActionsExporter
				IHaveDiagInfo)

		LC_PLUGIN_METADATA ("org.LeechCraft.BitTorrent")

		ICoreProxy_ptr Proxy_;

		std::shared_ptr<LeechCraft::Util::XmlSettingsDialog> XmlSettingsDialog_;
		std::unique_ptr<AddTorrent> AddTorrentDialog_;
		bool TorrentSelectionChanged_;
		std::unique_ptr<LeechCraft::Util::TagsCompleter> TagsAddDiaCompleter_;
		std::unique_ptr<TabWidget> TabWidget_;
		std::unique_ptr<QToolBar> Toolbar_;
		std::unique_ptr<QAction> OpenTorrent_,
			RemoveTorrent_,
			Resume_,
			Stop_,
			CreateTorrent_,
			MoveUp_,
			MoveDown_,
			MoveToTop_,
			MoveToBottom_,
			ForceReannounce_,
			ForceRecheck_,
			OpenMultipleTorrents_,
			IPFilter_,
			MoveFiles_,
			ChangeTrackers_,
			MakeMagnetLink_,
			OpenInTorrentTab_;

		SpeedSelectorAction *DownSelectorAction_,
				*UpSelectorAction_;

		enum
		{
			EAOpenTorrent_,
			EAChangeTrackers_,
			EACreateTorrent_,
			EAOpenMultipleTorrents_,
			EARemoveTorrent_,
			EAResume_,
			EAStop_,
			EAMoveUp_,
			EAMoveDown_,
			EAMoveToTop_,
			EAMoveToBottom_,
			EAForceReannounce_,
			EAForceRecheck_,
			EAMoveFiles_,
			EAImport_,
			EAExport_,
			EAMakeMagnetLink_,
			EAIPFilter_
		};

		TabClassInfo TabTC_;
		TorrentTab *TorrentTab_;

		QSortFilterProxyModel *ReprProxy_;
	public:
		// IInfo
		void Init (ICoreProxy_ptr) override;
		void SecondInit () override;
		QByteArray GetUniqueID () const override;
		QString GetName () const override;
		QString GetInfo () const override;
		QStringList Provides () const override;
		void Release () override;
		QIcon GetIcon () const override;

		// IDownload
		qint64 GetDownloadSpeed () const override;
		qint64 GetUploadSpeed () const override;
		void StartAll () override;
		void StopAll () override;
		EntityTestHandleResult CouldDownload (const LeechCraft::Entity&) const override;
		QFuture<Result> AddJob (LeechCraft::Entity) override;

		// IJobHolder
		QAbstractItemModel* GetRepresentation () const override;
		IJobHolderRepresentationHandler_ptr CreateRepresentationHandler () override;

		// IImportExport
		void ImportSettings (const QByteArray&) override;
		void ImportData (const QByteArray&) override;
		QByteArray ExportSettings () const override;
		QByteArray ExportData () const override;

		// ITaggableJobs
		void SetTags (int, const QStringList&) override;

		// IHaveSettings
		Util::XmlSettingsDialog_ptr GetSettingsDialog () const override;

		// IHaveShortcuts
		void SetShortcut (const QString&, const QKeySequences_t&) override;
		QMap<QString, ActionInfo> GetActionInfo () const override;

		// IHaveTabs
		TabClasses_t GetTabClasses () const override;
		void TabOpenRequested (const QByteArray&) override;

		// IStartupWizard
		QList<QWizardPage*> GetWizardPages () const override;

		// IToolBarEmbedder
		QList<QAction*> GetActions (ActionsEmbedPlace) const override;

		// IHaveDiagInfo
		QString GetDiagInfoString () const override;
	private slots:
		void on_OpenTorrent__triggered ();
		void on_OpenMultipleTorrents__triggered ();
		void on_IPFilter__triggered ();
		void on_CreateTorrent__triggered ();
		void on_RemoveTorrent__triggered ();
		void on_Resume__triggered ();
		void on_Stop__triggered ();
		void on_MoveUp__triggered ();
		void on_MoveDown__triggered ();
		void on_MoveToTop__triggered ();
		void on_MoveToBottom__triggered ();
		void on_ForceReannounce__triggered ();
		void on_ForceRecheck__triggered ();
		void on_ChangeTrackers__triggered ();
		void on_MoveFiles__triggered ();
		void on_MakeMagnetLink__triggered ();
		void on_OpenInTorrentTab__triggered ();
		void handleFastSpeedComboboxes ();
		void setActionsEnabled ();
	private:
		void SetupCore ();
		void SetupStuff ();
		void SetupActions ();
	signals:
		void addNewTab (const QString&, QWidget*) override;
		void changeTabIcon (QWidget*, const QIcon&) override;
		void changeTabName (QWidget*, const QString&) override;
		void raiseTab (QWidget*) override;
		void removeTab (QWidget*) override;
		void statusBarChanged (QWidget*, const QString&) override;

		void gotActions (QList<QAction*>, LeechCraft::ActionsEmbedPlace) override;
	};
}
}
