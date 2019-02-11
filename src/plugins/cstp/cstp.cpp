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

#include "cstp.h"
#include <algorithm>
#include <QMenu>
#include <QTranslator>
#include <QLocale>
#include <QFileInfo>
#include <QTabWidget>
#include <QToolBar>
#include <QMessageBox>
#include <QModelIndex>
#include <QDir>
#include <QUrl>
#include <QTextCodec>
#include <QTranslator>
#include <QMainWindow>
#include <interfaces/entitytesthandleresult.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ientitymanager.h>
#include <interfaces/core/irootwindowsmanager.h>
#include <util/sll/either.h>
#include <util/xpc/util.h>
#include <util/util.h>
#include "core.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace CSTP
{
	void CSTP::Init (ICoreProxy_ptr coreProxy)
	{
		Proxy_ = coreProxy;

		Core::Instance ().SetCoreProxy (coreProxy);

		Util::InstallTranslator ("cstp");

		XmlSettingsDialog_.reset (new Util::XmlSettingsDialog ());
		XmlSettingsDialog_->RegisterObject (&XmlSettingsManager::Instance (),
				"cstpsettings.xml");

		SetupToolbar ();

		Core::Instance ().SetToolbar (Toolbar_);

		connect (&Core::Instance (),
				SIGNAL (error (QString)),
				this,
				SLOT (handleError (QString)));
	}

	void CSTP::SecondInit ()
	{
	}

	void CSTP::Release ()
	{
		Core::Instance ().Release ();
		XmlSettingsManager::Instance ().Release ();
		XmlSettingsDialog_.reset ();
	}

	QByteArray CSTP::GetUniqueID () const
	{
		return "org.LeechCraft.CSTP";
	}

	QString CSTP::GetName () const
	{
		return "CSTP";
	}

	QString CSTP::GetInfo () const
	{
		return "Common Stream Transfer Protocols";
	}

	QStringList CSTP::Provides () const
	{
		return { "http", "https", "remoteable", "resume" };
	}

	QIcon CSTP::GetIcon () const
	{
		static QIcon icon ("lcicons:/plugins/cstp/resources/images/cstp.svg");
		return icon;
	}

	qint64 CSTP::GetDownloadSpeed () const
	{
		return Core::Instance ().GetTotalDownloadSpeed ();
	}

	qint64 CSTP::GetUploadSpeed () const
	{
		return 0;
	}

	void CSTP::StartAll ()
	{
		Core::Instance ().startAllTriggered ();
	}

	void CSTP::StopAll ()
	{
		Core::Instance ().stopAllTriggered ();
	}

	EntityTestHandleResult CSTP::CouldDownload (const LeechCraft::Entity& e) const
	{
		return Core::Instance ().CouldDownload (e);
	}

	QFuture<IDownload::Result> CSTP::AddJob (LeechCraft::Entity e)
	{
		return Core::Instance ().AddTask (e);
	}

	QAbstractItemModel* CSTP::GetRepresentation () const
	{
		return Core::Instance ().GetRepresentationModel ();
	}

	void CSTP::handleTasksTreeSelectionCurrentRowChanged (const QModelIndex& si, const QModelIndex&)
	{
		auto index = Core::Instance ().GetCoreProxy ()->MapToSource (si);
		if (index.model () != GetRepresentation ())
			index = QModelIndex {};
		Core::Instance ().ItemSelected (index);
	}

	Util::XmlSettingsDialog_ptr CSTP::GetSettingsDialog () const
	{
		return XmlSettingsDialog_;
	}

	void CSTP::SetupToolbar ()
	{
		Toolbar_ = new QToolBar;
		Toolbar_->setWindowTitle ("CSTP");

		QAction *remove = Toolbar_->addAction (tr ("Remove"));
		connect (remove,
				SIGNAL (triggered ()),
				&Core::Instance (),
				SLOT (removeTriggered ()));
		remove->setProperty ("ActionIcon", "list-remove");

		QAction *removeAll = Toolbar_->addAction (tr ("Remove all"));
		connect (removeAll,
				SIGNAL (triggered ()),
				&Core::Instance (),
				SLOT (removeAllTriggered ()));
		removeAll->setProperty ("ActionIcon", "edit-clear-list");

		Toolbar_->addSeparator ();

		QAction *start = Toolbar_->addAction (tr ("Start"));
		connect (start,
				SIGNAL (triggered ()),
				&Core::Instance (),
				SLOT (startTriggered ()));
		start->setProperty ("ActionIcon", "media-playback-start");

		QAction *stop = Toolbar_->addAction (tr ("Stop"));
		connect (stop,
				SIGNAL (triggered ()),
				&Core::Instance (),
				SLOT (stopTriggered ()));
		stop->setProperty ("ActionIcon", "media-playback-stop");

		QAction *startAll = Toolbar_->addAction (tr ("Start all"));
		connect (startAll,
				SIGNAL (triggered ()),
				&Core::Instance (),
				SLOT (startAllTriggered ()));
		startAll->setProperty ("ActionIcon", "media-seek-forward");

		QAction *stopAll = Toolbar_->addAction (tr ("Stop all"));
		connect (stopAll,
				SIGNAL (triggered ()),
				&Core::Instance (),
				SLOT (stopAllTriggered ()));
		stopAll->setProperty ("ActionIcon", "media-record");
	}

	void CSTP::handleFileExists (Core::FileExistsBehaviour *remove)
	{
		auto rootWM = Core::Instance ().GetCoreProxy ()->GetRootWindowsManager ();
		auto userReply = QMessageBox::warning (rootWM->GetPreferredWindow (),
				tr ("File exists"),
				tr ("File %1 already exists, continue download?"),
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (userReply == QMessageBox::Yes)
			*remove = Core::FileExistsBehaviour::Continue;
		else if (userReply == QMessageBox::No)
			*remove = Core::FileExistsBehaviour::Remove;
		else
			*remove = Core::FileExistsBehaviour::Abort;
	}

	void CSTP::handleError (const QString& error)
	{
		Proxy_->GetEntityManager ()->HandleEntity (Util::MakeNotification ("HTTP error", error, Priority::Critical));
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_cstp, LeechCraft::CSTP::CSTP);
