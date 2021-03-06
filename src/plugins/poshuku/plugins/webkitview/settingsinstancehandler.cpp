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

#include "settingsinstancehandler.h"
#include <QFile>
#include <QUrl>
#include <QtDebug>
#include <qwebsettings.h>
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Poshuku
{
namespace WebKitView
{
	SettingsInstanceHandler::SettingsInstanceHandler (QWebSettings *settings, QObject *parent)
	: QObject { parent }
	, Settings_ { settings }
	{
		XmlSettingsManager::Instance ().RegisterObject ("DNSPrefetchEnabled",
				this, "cacheSettingsChanged");
		cacheSettingsChanged ();

		XmlSettingsManager::Instance ().RegisterObject ({
					"AllowJava",
					"OfflineStorageDB",
					"OfflineWebApplicationCache",
					"EnableNotifications",
					"DeveloperExtrasEnabled"
				},
				this, "generalSettingsChanged");
		generalSettingsChanged ();

		XmlSettingsManager::Instance ().RegisterObject ("UserStyleSheet",
				this, "setUserStyleSheet");
		setUserStyleSheet ();
	}

	void SettingsInstanceHandler::generalSettingsChanged ()
	{
		auto& xsm = XmlSettingsManager::Instance ();

		auto boolOpt = [this, &xsm] (QWebSettings::WebAttribute attr, const char *name)
		{
			Settings_->setAttribute (attr, xsm.property (name).toBool ());
		};
		boolOpt (QWebSettings::JavaEnabled, "AllowJava");
		boolOpt (QWebSettings::OfflineStorageDatabaseEnabled, "OfflineStorageDB");
		boolOpt (QWebSettings::OfflineWebApplicationCacheEnabled, "OfflineWebApplicationCache");
		boolOpt (QWebSettings::DeveloperExtrasEnabled, "DeveloperExtrasEnabled");
		boolOpt (QWebSettings::NotificationsEnabled, "EnableNotifications");
	}

	void SettingsInstanceHandler::cacheSettingsChanged ()
	{
		Settings_->setAttribute (QWebSettings::DnsPrefetchEnabled,
				XmlSettingsManager::Instance ().property ("DNSPrefetchEnabled").toBool ());
	}

	void SettingsInstanceHandler::setUserStyleSheet ()
	{
		const auto& pathStr = XmlSettingsManager::Instance ()
				.property ("UserStyleSheet").toString ();
		if (pathStr.isEmpty ())
		{
			Settings_->setUserStyleSheetUrl ({});
			return;
		}

		QFile file { pathStr };
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot open"
					<< pathStr
					<< file.errorString ();
			Settings_->setUserStyleSheetUrl ({});
			return;
		}

		const auto& contents = file.readAll ();

		const auto& uriContents = "data:text/css;charset=utf-8;base64," + contents.toBase64 ();
		Settings_->setUserStyleSheetUrl (QUrl::fromEncoded (uriContents));
	}
}
}
}
