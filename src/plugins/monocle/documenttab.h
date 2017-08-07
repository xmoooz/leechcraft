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
#include "interfaces/monocle/idocument.h"
#include "docstatemanager.h"
#include "ui_documenttab.h"

class QDockWidget;
class QTreeView;

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
		QLineEdit *PageNumLabel_ = nullptr;

		QAction *LayOnePage_ = nullptr;
		QAction *LayTwoPages_ = nullptr;

		QAction *SaveAction_ = nullptr;
		QAction *ExportPDFAction_ = nullptr;
		QAction *FindAction_ = nullptr;
		FindDialog *FindDialog_ = nullptr;

		PagesLayoutManager *LayoutManager_ = nullptr;
		TextSearchHandler *SearchHandler_ = nullptr;
		FormManager *FormManager_ = nullptr;
		AnnManager *AnnManager_ = nullptr;
		LinksManager *LinksManager_ = nullptr;

		QDockWidget *DockWidget_ = nullptr;
		TOCWidget *TOCWidget_ = nullptr;
		BookmarksWidget *BMWidget_ = nullptr;
		ThumbsWidget *ThumbsWidget_ = nullptr;
		AnnWidget *AnnWidget_ = nullptr;
		SearchTabWidget *SearchTabWidget_ = nullptr;
		QTreeView *OptContentsWidget_ = nullptr;

		IDocument_ptr CurrentDoc_;
		QString CurrentDocPath_;
		QList<PageGraphicsItem*> Pages_;
		QGraphicsScene Scene_;

		enum class MouseMode
		{
			Move,
			Select
		} MouseMode_;

		bool SaveStateScheduled_;

		int PrevCurrentPage_;

		struct OnloadData
		{
			int Num_;
			double X_;
			double Y_;
		} Onload_;
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

		bool eventFilter (QObject*, QEvent*) override;
	protected:
		void dragEnterEvent (QDragEnterEvent*) override;
		void dropEvent (QDropEvent*) override;
	private:
		void SetupToolbarOpen ();
		void SetupToolbarRotate ();
		void SetupToolbar ();

		QPoint GetViewportCenter () const;
		void Relayout ();

		QImage GetSelectionImg ();
		QString GetSelectionText () const;

		void RegenPageVisibility ();
	private slots:
		void handleLoaderReady (const IDocument_ptr&, const QString&);

		void handleNavigateRequested (QString, int, double, double);
		void handlePrintRequested ();

		void handleThumbnailClicked (int);

		void handlePageContentsChanged (int);

		void scheduleSaveState ();
		void saveState ();

		void handleRecentOpenAction (QAction*);

		void selectFile ();
		void handleSave ();
		void handleExportPDF ();

		void handlePrint ();
		void handlePresentation ();

		void handleGoPrev ();
		void handleGoNext ();
		void navigateNumLabel ();
		void updateNumLabel ();
		void checkCurrentPageChange (bool force = false);

		void rotateCCW ();
		void rotateCW ();

		void zoomOut ();
		void zoomIn ();

		void showOnePage ();
		void showTwoPages ();
		void syncUIToLayMode ();

		void recoverDocState (DocStateManager::State);

		void setMoveMode (bool);
		void setSelectionMode (bool);

		void handleCopyAsImage ();
		void handleSaveAsImage ();
		void handleCopyAsText ();

		void showDocInfo ();

		void delayedCenterOn (const QPoint&);

		void handleScaleChosen (int);
		void handleCustomScale (QString);

		void handleDockLocation (Qt::DockWidgetArea);
		void handleDockVisibility (bool);
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
