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

#include "declarativewindow.h"

#if QT_VERSION < 0x050000
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QGraphicsObject>
#else
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#endif

#include <util/sys/paths.h>
#include <util/qml/colorthemeproxy.h>
#include <util/qml/themeimageprovider.h>
#include <util/gui/unhoverdeletemixin.h>
#include <util/gui/autoresizemixin.h>
#include <util/gui/util.h>
#include "viewmanager.h"

namespace LeechCraft
{
namespace SB2
{
	DeclarativeWindow::DeclarativeWindow (const QUrl& url, QVariantMap params,
			const QPoint& orig, ViewManager *viewMgr, ICoreProxy_ptr proxy, QWidget *parent)
#if QT_VERSION < 0x050000
	: QDeclarativeView (parent)
#else
	: QQuickWidget (parent)
#endif
	{
		new Util::AutoResizeMixin (orig, [viewMgr] { return viewMgr->GetFreeCoords (); }, this);

		if (!params.take ("keepOnFocusLeave").toBool ())
			new Util::UnhoverDeleteMixin (this, SLOT (beforeDelete ()));

		setStyleSheet ("background: transparent");
		setWindowFlags (Qt::Tool | Qt::FramelessWindowHint);
		setAttribute (Qt::WA_TranslucentBackground);

		for (const auto& cand : Util::GetPathCandidates (Util::SysPath::QML, ""))
			engine ()->addImportPath (cand);

		rootContext ()->setContextProperty ("colorProxy",
				new Util::ColorThemeProxy (proxy->GetColorThemeManager (), this));
		for (const auto& key : params.keys ())
			rootContext ()->setContextProperty (key, params [key]);
		engine ()->addImageProvider ("ThemeIcons", new Util::ThemeImageProvider (proxy));
		setSource (url);

		connect (rootObject (),
				SIGNAL (closeRequested ()),
				this,
				SLOT (deleteLater ()));
	}

	void DeclarativeWindow::beforeDelete ()
	{
		QMetaObject::invokeMethod (rootObject (),
				"beforeDelete");
		deleteLater ();
	}
}
}
