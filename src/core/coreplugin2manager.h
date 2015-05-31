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

#include <QNetworkAccessManager>
#include <QMainWindow>
#include <QAction>
#include "util/xpc/basehookinterconnector.h"
#include "interfaces/core/ihookproxy.h"
#include "interfaces/iinfo.h"

class QMenu;
class QDockWidget;
class QSystemTrayIcon;

namespace LeechCraft
{
	class CorePlugin2Manager : public Util::BaseHookInterconnector
	{
		Q_OBJECT
	public:
		CorePlugin2Manager (QObject* = 0);
	signals:
		void hookDockWidgetActionVisToggled (LeechCraft::IHookProxy_ptr proxy,
				QMainWindow *window,
				QDockWidget *dock,
				bool toggleActionVisible);

		void hookAddingDockAction (LeechCraft::IHookProxy_ptr, QMainWindow*, QAction*, Qt::DockWidgetArea);
		void hookRemovingDockAction (LeechCraft::IHookProxy_ptr, QMainWindow*, QAction*, Qt::DockWidgetArea);

		void hookDockBarWillBeShown (LeechCraft::IHookProxy_ptr, QMainWindow*, QToolBar*, Qt::DockWidgetArea);

		void hookGonnaFillMenu (LeechCraft::IHookProxy_ptr);
		void hookGonnaFillQuickLaunch (LeechCraft::IHookProxy_ptr proxy);
		void hookGonnaShowStatusBar (LeechCraft::IHookProxy_ptr, bool);
		void hookNAMCreateRequest (LeechCraft::IHookProxy_ptr proxy,
				QNetworkAccessManager *manager,
				QNetworkAccessManager::Operation *op,
				QIODevice **dev);
		void hookTabContextMenuFill (LeechCraft::IHookProxy_ptr proxy,
				QMenu *menu, int index, int windowId);

		void hookTabAdding (LeechCraft::IHookProxy_ptr proxy,
				QWidget *tabWidget);
		void hookTabFinishedMoving (LeechCraft::IHookProxy_ptr proxy,
				int index, int windowId);
		void hookTabSetText (LeechCraft::IHookProxy_ptr proxy,
				int index, int windowId);
		void hookTabIsRemoving (LeechCraft::IHookProxy_ptr proxy,
				int index,
				int windowId);

		void hookGetPreferredWindowIndex (LeechCraft::IHookProxy_ptr proxy,
				const QWidget *widget);

		void hookTrayIconCreated (LeechCraft::IHookProxy_ptr,
				QSystemTrayIcon*);
		void hookTrayIconVisibilityChanged (LeechCraft::IHookProxy_ptr,
				QSystemTrayIcon*,
				bool);
	};
}
