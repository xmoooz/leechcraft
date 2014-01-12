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

#include "resourcedownloadhandler.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QHash>
#include <QNetworkRequest>
#include <QSettings>
#include "userscript.h"



namespace LeechCraft
{
namespace Poshuku
{
namespace FatApe
{
	ResourceDownloadHandler::ResourceDownloadHandler (const QString& resourceName, 
			UserScript *script, QNetworkReply *reply)
	: ResourceName_ (resourceName)
	, Script_ (script)
	, Reply_ (reply)
	{}

	void ResourceDownloadHandler::handleFinished ()
	{
		QFile resource (Script_->GetResourcePath (ResourceName_));
		QSettings settings (QCoreApplication::organizationName (),
			QCoreApplication::applicationName () + "_Poshuku_FatApe");


		if (!resource.open (QFile::WriteOnly))
		{
			qWarning () << Q_FUNC_INFO
				<< "unable to save resource"
				<< ResourceName_
				<< "from"
				<< Reply_->url ().toString ();
			return;
		}
		resource.write (Reply_->readAll ());

		settings.setValue (QString ("resources/%1/%2/%3")
				.arg (qHash (Script_->Namespace ()))
				.arg (Script_->Name ())
				.arg (ResourceName_), 
				Reply_->header (QNetworkRequest::ContentTypeHeader));
		Reply_->deleteLater ();
		deleteLater ();
	}
}
}
}

