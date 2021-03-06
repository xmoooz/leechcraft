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

#include <QWidget>
#include <QComboBox>
#include <interfaces/ihavetabs.h>
#include <interfaces/ihaverecoverabletabs.h>
#include <interfaces/idndtab.h>
#include <util/sll/bitflags.h>
#include <util/xpc/screensaverprohibitor.h>
#include "interfaces/monocle/idocument.h"
#include "docstatemanager.h"
#include "navigationhistory.h"
#include "ui_documenttab.h"

class QDockWidget;
class QTreeView;
class QMenu;

namespace LeechCraft
{
namespace Monocle
{
	enum class LayoutMode;

	class PagesLayoutManager;
	class PageGraphicsItem;
	class TextSearchHandler;
	class TOCWidget;
	class BookmarksWidget;
	class ThumbsWidget;
	class AnnWidget;
	class FindDialog;
	class FormManager;
	class LinksManager;
	class AnnManager;
	class SearchTabWidget;
	class DocumentBookmarksManager;
	class PageNumLabel;
	class SmoothScroller;

	class DocumentTab : public QWidget
					  , public ITabWidget
					  , public IRecoverableTab
					  , public IDNDTab
	{
		Q_OBJECT
		Q_INTERFACES (ITabWidget IRecoverableTab IDNDTab)

		Ui::DocumentTab Ui_;

		TabClassInfo TC_;
		QObject *ParentPlugin_;

		QToolBar *Toolbar_;
		QComboBox *ScalesBox_ = nullptr;
		QAction *ZoomOut_ = nullptr;
		QAction *ZoomIn_ = nullptr;
		PageNumLabel *PageNumLabel_ = nullptr;

		QAction *LayOnePage_ = nullptr;
		QAction *LayTwoPages_ = nullptr;
		QAction *LayTwoPagesShifted_ = nullptr;

		QAction *SaveAction_ = nullptr;
		QAction *ExportPDFAction_ = nullptr;
		QAction *FindAction_ = nullptr;
		FindDialog *FindDialog_ = nullptr;

		SmoothScroller *Scroller_ = nullptr;

		PagesLayoutManager *LayoutManager_ = nullptr;
		TextSearchHandler *SearchHandler_ = nullptr;
		FormManager *FormManager_ = nullptr;
		AnnManager *AnnManager_ = nullptr;
		LinksManager *LinksManager_ = nullptr;

		QDockWidget *DockWidget_ = nullptr;
		TOCWidget *TOCWidget_ = nullptr;
		DocumentBookmarksManager *DocBMManager_ = nullptr;
		BookmarksWidget *BMWidget_ = nullptr;
		ThumbsWidget *ThumbsWidget_ = nullptr;
		AnnWidget *AnnWidget_ = nullptr;
		SearchTabWidget *SearchTabWidget_ = nullptr;
		QTreeView *OptContentsWidget_ = nullptr;

		NavigationHistory * const NavHistory_;

		IDocument_ptr CurrentDoc_;
		QString CurrentDocPath_;
		QList<PageGraphicsItem*> Pages_;
		QGraphicsScene Scene_;

		bool SaveStateScheduled_ = false;

		int PrevCurrentPage_;

		IDocument::Position Onload_ { -1, {} };

		Util::ScreensaverProhibitor ScreensaverProhibitor_;
	public:
		enum class DocumentOpenOption
		{
			None = 0x0,
			IgnoreErrors = 0x1
		};
		using DocumentOpenOptions = Util::BitFlags<DocumentOpenOption>;

		DocumentTab (const TabClassInfo&, QObject*);

		TabClassInfo GetTabClassInfo () const override;
		QObject* ParentMultiTabs () override;
		void Remove () override;
		QToolBar* GetToolBar () const override;
		void TabMadeCurrent () override;
		void TabLostCurrent () override;

		QString GetTabRecoverName () const override;
		QIcon GetTabRecoverIcon () const override;
		QByteArray GetTabRecoverData () const override;

		void FillMimeData (QMimeData*) override;
		void HandleDragEnter (QDragMoveEvent*) override;
		void HandleDrop (QDropEvent*) override;

		void RecoverState (const QByteArray&);

		void ReloadDoc (const QString&);
		bool SetDoc (const QString&, DocumentOpenOptions);

		void CreateViewCtxMenuActions (QMenu*);

		int GetCurrentPage () const;
		void SetCurrentPage (int, bool immediate = false);

		QPoint GetCurrentCenter () const;
		void CenterOn (const QPoint&);
	protected:
		void dragEnterEvent (QDragEnterEvent*) override;
		void dropEvent (QDropEvent*) override;
	private:
		void SetupToolbarOpen ();
		void SetupToolbarRotate ();
		void SetupToolbarNavigation ();
		void SetupToolbar ();

		QPoint GetViewportCenter () const;
		void Relayout ();
		void SetLayoutMode (LayoutMode);

		QImage GetSelectionImg ();
		QString GetSelectionText () const;

		void RegenPageVisibility ();

		NavigationHistory::Entry GetNavigationHistoryEntry () const;
		void NavigateToPath (QString, const IDocument::Position&);
		void NavigateWithinDocument (const IDocument::Position&);

		void CheckCurrentPageChange ();
	private slots:
		void handleNavigateRequested (const QString&, const IDocument::Position&);

		void handleLoaderReady (DocumentOpenOptions, const IDocument_ptr&, const QString&);

		void handlePrintRequested ();

		void handlePageContentsChanged (int);

		void scheduleSaveState ();
		void saveState ();

		void selectFile ();
		void handleSave ();
		void handleExportPDF ();

		void handlePrint ();
		void handlePresentation ();

		void zoomOut ();
		void zoomIn ();

		void recoverDocState (DocStateManager::State);

		void setMoveMode (bool);
		void setSelectionMode (bool);

		void handleCopyAsImage ();
		void handleSaveAsImage ();
		void handleCopyAsText ();

		void showDocInfo ();

		void handleScaleChosen (int);
		void handleCustomScale (QString);
	signals:
		void changeTabName (QWidget*, const QString&);
		void removeTab (QWidget*);

		void tabRecoverDataChanged () override;

		void fileLoaded (const QString&);

		void currentPageChanged (int);
		void pagesVisibilityChanged (const QMap<int, QRect>&);
	};
}
}
