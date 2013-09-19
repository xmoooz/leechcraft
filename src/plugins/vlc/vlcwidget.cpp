/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2013  Vladislav Tyulbashev
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

#include <vlc/vlc.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEvent>
#include <QFileDialog>
#include <QInputDialog>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QTime>
#include <QToolBar>
#include <QMenu>
#include <QSizePolicy>
#include <QLabel>
#include <QWidgetAction>
#include <QPushButton>
#include <QTimer>
#include <QDebug>
#include <QToolButton>
#include <QUrl>
#include <QEventLoop>
#include <QResizeEvent>
#include <QCursor>
#include <QDropEvent>
#include <QCoreApplication>
#include <QDockWidget>
#include <QSettings>
#include <QStringList>
#include <util/shortcuts/shortcutmanager.h>
#include <interfaces/core/irootwindowsmanager.h>
#include <interfaces/imwproxy.h>
#include "vlcwidget.h"
#include "vlcplayer.h"
#include "xmlsettingsmanager.h"

namespace
{
	int dist (QPoint a, QPoint b)
	{
		return (a - b).manhattanLength ();
	}
	
	const QStringList KNOWN_AUDIO_FILE_FORMATS = {".ac3", ".mp3", ".ogg"};
	const QStringList KNOWN_SUBTITLES_FILE_FORMATS = {".srt", ".smi", ".ssa", ".ass"};
}

namespace LeechCraft
{
namespace vlc
{
	const int PANEL_SIDE_MARGIN = 5;
	const int PANEL_BOTTOM_MARGIN = 5;
	const int PANEL_HEIGHT = 27;
	
	VlcWidget::VlcWidget (ICoreProxy_ptr proxy, Util::ShortcutManager *manager, QWidget *parent)
	: QWidget (parent)
	, Parent_ (parent)
	, Manager_ (manager)
	, AllowFullScreenPanel_ (false)
	, Autostart_ (true)
	{
		VlcMainWidget_ = new SignalledWidget;
		VlcMainWidget_->SetBackGroundColor (new QColor ("black"));
		PlaylistWidget_ = new PlaylistWidget (proxy->GetIcon ("media-playback-start"));
		
		QVBoxLayout *layout = new QVBoxLayout;
		layout->addWidget(VlcMainWidget_);
		setLayout (layout);
		
		PlaylistDock_ = new QDockWidget (this);
		PlaylistDock_->setFeatures (QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
		PlaylistDock_->setAllowedAreas (Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
		PlaylistDock_->setTitleBarWidget (new QLabel (tr ("Playlist"), PlaylistDock_));
		PlaylistDock_->titleBarWidget ()->setContentsMargins (10, 2, 10, 0);
		
		auto mw = proxy->GetRootWindowsManager ()->GetMWProxy (0);
		mw->AddDockWidget (Qt::LeftDockWidgetArea, PlaylistDock_);
		mw->AssociateDockWidget (PlaylistDock_, this);
		mw->ToggleViewActionVisiblity (PlaylistDock_, false);
		
		PlaylistDock_->setWidget (PlaylistWidget_);
		
		VlcPlayer_ = new VlcPlayer (VlcMainWidget_);
		QSizePolicy pol (QSizePolicy::Expanding, QSizePolicy::Expanding);
		pol.setHorizontalStretch (255);
		pol.setVerticalStretch (255);
		VlcMainWidget_->setSizePolicy (pol);
		
		PlaylistWidget_->Init (VlcPlayer_->GetInstance (), VlcPlayer_->GetPlayer ().get ());
		VlcPlayer_->Init (VlcMainWidget_);
		
		DisableScreenSaver_ = new QTimer (this);
		DisableScreenSaver_->setInterval (9000);
		
		GenerateToolBar ();
		PrepareFullScreen ();
		InterfaceUpdater_ = new QTimer (this);
		InterfaceUpdater_->setInterval (100);
		InterfaceUpdater_->start ();
		
		TerminatePanel_ = new QTimer (this);
		TerminatePanel_->setInterval (500);
		
		ConnectWidgetToMe (VlcMainWidget_);
		ConnectWidgetToMe (FullScreenWidget_);
		ConnectWidgetToMe (FullScreenPanel_);
		
		connect (TerminatePanel_,
				SIGNAL (timeout ()),
				this,
				SLOT (hideFullScreenPanel ()));
		
		connect (InterfaceUpdater_,
				SIGNAL (timeout ()),
				this,
				SLOT (updateInterface ()));
		
		connect (VlcMainWidget_,
				SIGNAL (customContextMenuRequested (QPoint)),
				this,
				SLOT (generateContextMenu (QPoint)));
		
		connect (ScrollBar_,
				SIGNAL (changePosition (double)),
				VlcPlayer_,
				SLOT (changePosition (double)));
		
		connect (Open_,
				SIGNAL (triggered ()),
				this,
				SLOT (addFile ()));
		
		connect (TogglePlay_,
				SIGNAL (triggered ()),
				PlaylistWidget_,
				SLOT(togglePlay ()));
		
		connect (Stop_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (stop ()));
		
		connect (FullScreenAction_,
				SIGNAL (triggered ()),
				this,
				SLOT (toggleFullScreen ()));
		
		connect (VlcPlayer_,
				SIGNAL (stable ()),
				ScrollBar_,
				SLOT (unBlockUpdating ()));
		
		connect (VlcPlayer_,
				SIGNAL (unstable ()),
				ScrollBar_,
				SLOT (blockUpdating ()));
		
		connect (DisableScreenSaver_,
				SIGNAL (timeout ()),
				this,
				SLOT (disableScreenSaver ()));
		
		connect (PlaylistWidget_,
				SIGNAL (savePlaylist (QStringList)),
				this,
				SLOT (savePlaylist (QStringList)));
		
		InitNavigations ();
		InitVolumeActions ();
		InitRewindActions ();
		setAcceptDrops (true);
		RestoreSettings ();
	}
	
	VlcWidget::~VlcWidget ()
	{
		VlcPlayer_->stop ();
		delete PlaylistWidget_;
		delete PlaylistDock_;
		delete VlcPlayer_;
		SaveSettings ();
		emit deleteMe (this);
	}
	
	void VlcWidget::RestoreSettings ()
	{
		Settings_ = new QSettings (QCoreApplication::organizationName (), QCoreApplication::applicationName () + "_VLC");
		RestorePlaylist ();
	}
	
	void VlcWidget::SaveSettings ()
	{
		delete Settings_;
	}
	
	void VlcWidget::savePlaylist (const QStringList& list)
	{
		qWarning () << list;
		Settings_->setValue ("Playlist", QVariant (list));
	}

	void VlcWidget::RestorePlaylist ()
	{
		QStringList playlist = Settings_->value ("Playlist").toStringList ();
		if (playlist.size() < 2)
			return;
		
		for (int i = 0; i < playlist.size () - 1; i++)
			PlaylistWidget_->AddUrl (QUrl::fromEncoded (playlist [i].toUtf8 ()), false);
		
		PlaylistWidget_->SetCurrentMedia (playlist [playlist.size () - 1].toInt ());
	}
	
	QObject* VlcWidget::ParentMultiTabs ()
	{
		return Parent_;
	}
	
	QToolBar* VlcWidget::GetToolBar () const
	{
		return Bar_;
	}
	
	void VlcWidget::Remove () 
	{	
		deleteLater ();
	}
	
	void VlcWidget::addFile ()
	{
		QString file = QFileDialog::getOpenFileName (this,
													tr ("Open file"),
													tr ("Videos (*.mkv *.avi *.mov *.mpg)"));
		
		if (QFile::exists (file))
		{
			PlaylistWidget_->Clear ();
			PlaylistWidget_->AddUrl (QUrl::fromLocalFile (file), Autostart_);
		}
	}
	
	void VlcWidget::addFolder () 
	{
		QString folder = QFileDialog::getExistingDirectory (this,
													tr ("Open folder"),
													tr ("Folder with video"));
		
		if (QFile::exists (folder))
		{
			PlaylistWidget_->Clear ();
			PlaylistWidget_->AddUrl (QUrl ("directory://" + folder), Autostart_);
		}
	}
	
	void VlcWidget::addSimpleDVD ()
	{
		QString folder = QFileDialog::getExistingDirectory (this,
													tr ("Open DVD"),
													tr ("Root of DVD directory"));
		
		if (QFile::exists (folder))
		{
			PlaylistWidget_->Clear ();
			PlaylistWidget_->AddUrl (QUrl ("dvdsimple://" + folder), Autostart_);
		}
	}
	
	void VlcWidget::addDVD ()
	{
		QString folder = QFileDialog::getExistingDirectory (this,
													tr ("Open DVD"),
													tr ("Root of DVD directory"));
		
		if (QFile::exists (folder))
		{
			PlaylistWidget_->Clear ();
			PlaylistWidget_->AddUrl (QUrl ("dvd://" + folder), Autostart_);
		}
	}

	void VlcWidget::addUrl ()
	{
		QString url = QInputDialog::getText (this, tr ("Open URL"), tr ("Enter URL"));
		
		if (!url.isEmpty ())
		{
			PlaylistWidget_->Clear ();
			PlaylistWidget_->AddUrl (QUrl (url), Autostart_);
		}
	}
	
	void VlcWidget::addSlave ()
	{
		const QString& url = QFileDialog::getOpenFileName (this,
													tr ("Open file"),
													tr ("Media (*.ac3)"));
		
		if (QFile::exists (url))
			VlcPlayer_->addUrl (QUrl::fromLocalFile(url));
	}
	
	void VlcWidget::updateInterface ()
	{
		if (VlcPlayer_->NowPlaying ()) 
		{
			TogglePlay_->setText (tr ("Pause"));
			TogglePlay_->setProperty ("ActionIcon", "media-playback-pause");
		}
		else
		{
			TogglePlay_->setText (tr ("Play"));
			TogglePlay_->setProperty ("ActionIcon", "media-playback-start");
		}
		
		if (FullScreen_) 
		{
			FullScreenVlcScrollBar_->setPosition (VlcPlayer_->GetPosition ());
			FullScreenVlcScrollBar_->update ();
			FullScreenTimeLeft_->setText (VlcPlayer_->GetCurrentTime ().toString ("HH:mm:ss"));
			FullScreenTimeAll_->setText (VlcPlayer_->GetFullTime ().toString ("HH:mm:ss"));
			
			if (FullScreenPanel_->isVisible ()) 
			{
				if (QCursor::pos ().x () > PANEL_SIDE_MARGIN && 
					QCursor::pos ().x () < FullScreenWidget_->width () - PANEL_SIDE_MARGIN &&
					QCursor::pos ().y () < FullScreenWidget_->height () - PANEL_BOTTOM_MARGIN && 
					QCursor::pos ().y () > FullScreenWidget_->height () - PANEL_BOTTOM_MARGIN - PANEL_HEIGHT)
				{
					fullScreenPanelRequested ();
					FullScreenPanel_->setWindowOpacity (0.8);
				}
				else
					FullScreenPanel_->setWindowOpacity (0.7);
			}
		}
		else 
		{
			if (FullScreenPanel_->isVisible ())
				FullScreenPanel_->hide ();
			ScrollBar_->setPosition (VlcPlayer_->GetPosition ());
			ScrollBar_->update ();
			TimeLeft_->setText (VlcPlayer_->GetCurrentTime ().toString ("HH:mm:ss"));
			TimeAll_->setText (VlcPlayer_->GetFullTime ().toString ("HH:mm:ss"));
		}
		
		
		if (FullScreen_) 
		{
			if (!FullScreenWidget_->isVisible ())
				toggleFullScreen ();
			else
				FullScreenWidget_->setFocus ();
		}
		else
			VlcMainWidget_->setFocus ();
	}

	void VlcWidget::mouseDoubleClickEvent (QMouseEvent *event)
	{	
		toggleFullScreen ();
		event->accept ();
	}

	void VlcWidget::mousePressEvent (QMouseEvent *event)
	{		
		if (event->button () == Qt::LeftButton)
			VlcPlayer_->DVDNavigate (libvlc_navigate_activate);
	}
	
	void VlcWidget::wheelEvent (QWheelEvent *event)
	{
		if (event->delta () > 0)
			SoundWidget_->increaseVolume ();
		else
			SoundWidget_->decreaseVolume ();
		
		fullScreenPanelRequested ();
		event->accept ();
	}
	
	void VlcWidget::keyPressEvent (QKeyEvent *event) 
	{
	}
	
	void VlcWidget::mouseMoveEvent (QMouseEvent *event)
	{
		if (FullScreen_ && dist (event->pos (), LastMouseEvent_) > 2)
			fullScreenPanelRequested ();
		
		LastMouseEvent_ = event->pos ();
		event->accept ();
	}


	void VlcWidget::toggleFullScreen () 
	{
		if (ForbidFullScreen_)
			return;
				
		ForbidFullScreen ();
		
		if (!FullScreen_)
		{
			FullScreen_ = true;
			AllowFullScreenPanel_ = true;
			FullScreenWidget_->SetBackGroundColor (new QColor ("black"));
			FullScreenWidget_->showFullScreen ();
// 			FullScreenWidget_->show ();
			VlcPlayer_->switchWidget (FullScreenWidget_);
		} 
		else 
		{
			AllowFullScreenPanel_ = false;
 			FullScreen_ = false;
			FullScreenWidget_->hide ();
			FullScreenPanel_->hide ();
			VlcPlayer_->switchWidget (VlcMainWidget_);
		}
	}
	
	void VlcWidget::GenerateToolBar () 
	{
		Bar_ = new QToolBar (this);
		OpenButton_ = new QToolButton (Bar_);
		OpenButton_->setMenu (GenerateMenuForOpenAction ());
		OpenButton_->setPopupMode (QToolButton::MenuButtonPopup);
		Open_ = new QAction (OpenButton_);
		Open_->setProperty ("ActionIcon", "folder");
		OpenButton_->setDefaultAction (Open_);
		Bar_->addWidget (OpenButton_);
		TogglePlay_ = Bar_->addAction (tr ("Play"));
		Manager_->RegisterAction ("org.vlc.toggle_play", TogglePlay_, true);
		TogglePlay_->setProperty ("ActionIcon", "media-playback-start");
		TogglePlay_->setProperty ("WatchActionIconChange", true);
		Stop_ = Bar_->addAction (tr ("Stop"));
		Stop_->setProperty ("ActionIcon", "media-playback-stop");
		FullScreenAction_ = Bar_->addAction (tr ("FullScreen"));
		FullScreenAction_->setProperty ("ActionIcon", "view-fullscreen");
		Manager_->RegisterAction ("org.vlc.toggle_fullscreen", FullScreenAction_, true);
		TimeLeft_ = new QLabel (this);
		Bar_->addWidget (TimeLeft_);
		ScrollBar_ = new VlcScrollBar;
		ScrollBar_->setBaseSize (200, 25);
		QWidget *tmp = new QWidget (this);
		QVBoxLayout *layout = new QVBoxLayout;
		layout->setContentsMargins (2, 2, 2, 2);
		layout->addWidget (ScrollBar_);
		tmp->setLayout (layout);
		QSizePolicy pol;
		pol.setHorizontalStretch (255);
		pol.setHorizontalPolicy (QSizePolicy::Ignored);
		pol.setVerticalPolicy (QSizePolicy::Expanding);
		tmp->setSizePolicy(pol);
		Bar_->addWidget (tmp);
		TimeAll_ = new QLabel;
		Bar_->addWidget (TimeAll_);
		SoundWidget_ = new SoundWidget (this, VlcPlayer_->GetPlayer ());
		SoundWidget_->setFixedSize (100, 25);
		layout = new QVBoxLayout;
		layout->addWidget (SoundWidget_);
		layout->setContentsMargins (2, 2, 2, 2);
		tmp = new QWidget (this);
		tmp->setLayout (layout);
		Bar_->addWidget (tmp);
	}
	
	TabClassInfo VlcWidget::GetTabClassInfo () const
	{
		return VlcWidget::GetTabInfo ();
	}
	
	TabClassInfo VlcWidget::GetTabInfo () 
	{
		static TabClassInfo main;
		main.Description_ = "Main tab for VLC plugin";
		main.Priority_ = 1;
		main.Icon_ = QIcon ();
		main.VisibleName_ = "Name of main tab of VLC plugin";
		main.Features_ = TabFeature::TFOpenableByRequest;
		main.TabClass_ = "org.LeechCraft.vlc";
		return main;
	};
	
	void VlcWidget::TabLostCurrent ()
	{
		InterfaceUpdater_->stop ();
		DisableScreenSaver_->stop ();
	}
	
	void VlcWidget::TabMadeCurrent ()
	{
		InterfaceUpdater_->start ();
		DisableScreenSaver_->start ();
	}
	
	void VlcWidget::ForbidFullScreen ()
	{
		ForbidFullScreen_ = true;
		QTimer::singleShot (500, this, SLOT (allowFullScreen ()));
	}
	
	void VlcWidget::allowFullScreen ()
	{
		ForbidFullScreen_ = false;
	}
	
	void VlcWidget::generateContextMenu (QPoint pos)
	{
		ContextMenu_ = new QMenu (this);
		
		QMenu *subtitles = new QMenu (tr ("subtitles"), ContextMenu_);
		QMenu *tracks = new QMenu (tr ("tracks"), ContextMenu_);
		
		for (int i = 0; i < VlcPlayer_->GetAudioTracksNumber (); i++)
		{
			QAction *action = new QAction (tracks);
			action->setData (VlcPlayer_->GetAudioTrackId (i));
			action->setText (VlcPlayer_->GetAudioTrackDescription (i));
			if (VlcPlayer_->GetAudioTrackId (i) == VlcPlayer_->GetCurrentAudioTrack ())
			{
				action->setCheckable (true);
				action->setChecked (true);
			}
			tracks->addAction (action);
		}
		
		for (int i = 0; i < VlcPlayer_->GetSubtitlesNumber (); i++) {
			QAction *action = new QAction (subtitles);
			action->setData (QVariant (VlcPlayer_->GetSubtitleId (i)));
			action->setText (VlcPlayer_->GetSubtitleDescription (i));
			if (VlcPlayer_->GetSubtitleId (i) == VlcPlayer_->GetCurrentSubtitle ())
			{
				action->setCheckable (true);
				action->setChecked (true);
			}
			subtitles->addAction (action);
		}
		
		subtitles->addSeparator ();
		subtitles->addAction (tr ("Add subtitles..."));
		
		tracks->addSeparator ();
		tracks->addAction (tr ("Add external sound track"));
		
		ContextMenu_->addMenu (subtitles);
		ContextMenu_->addMenu (tracks);
		
		connect (tracks,
				SIGNAL (triggered (QAction*)),
				this,
				SLOT (setAudioTrack (QAction*)));
		
		connect (subtitles,
				SIGNAL (triggered (QAction*)),
				this,
				SLOT (setSubtitles (QAction*)));
				
		ContextMenu_->exec (QCursor::pos ());
	}
	
	void VlcWidget::setSubtitles(QAction *action)
	{
		if (action->data ().isNull ())
			VlcPlayer_->AddSubtitles (GetNewSubtitles ());
		else 
		{
			int track = action->data ().toInt ();
			VlcPlayer_->setSubtitle (track);
		}
	}
	
	QString VlcWidget::GetNewSubtitles ()
	{
		return QFileDialog::getOpenFileName (this,
											tr ("Open file"),
											tr ("Subtitles (*.srt)"));
	}
	
	void VlcWidget::setAudioTrack (QAction *action)
	{
		if (action->data ().isNull ())
			addSlave ();
		else
		{
			int track = action->data ().toInt ();
			VlcPlayer_->setAudioTrack (track);
		}
	}
	
	void VlcWidget::ConnectWidgetToMe (SignalledWidget *widget)
	{
		connect (widget,
				SIGNAL (mouseDoubleClick (QMouseEvent*)),
				this,
				SLOT (mouseDoubleClickEvent (QMouseEvent*)));
		
		connect (widget,
				SIGNAL (keyPress (QKeyEvent*)),
				this,
				SLOT (keyPressEvent (QKeyEvent*)));
		
		connect (widget,
				SIGNAL (wheel (QWheelEvent*)),
				this,
				SLOT (wheelEvent (QWheelEvent*)));
		
		connect (widget,
				SIGNAL (mousePress (QMouseEvent*)),
				this,
				SLOT (mousePressEvent (QMouseEvent*)));	
	}
	
	void VlcWidget::PrepareFullScreen ()
	{
		FullScreen_ = false;
		ForbidFullScreen_ = false;
		FullScreenWidget_ = new SignalledWidget;
		FullScreenWidget_->addAction (TogglePlay_);
		FullScreenWidget_->addAction (FullScreenAction_);
		FullScreenPanel_ = new SignalledWidget (this, Qt::ToolTip);
		QHBoxLayout *panelLayout = new QHBoxLayout;
		FullScreenTimeLeft_ = new QLabel;
		FullScreenTimeAll_ = new QLabel;
		FullScreenVlcScrollBar_ = new VlcScrollBar (this);
		FullScreenSoundWidget_ = new SoundWidget (this, VlcPlayer_->GetPlayer ());
		FullScreenSoundWidget_->setFixedSize (100, 25);
		QSizePolicy fullScreenVlcScrollBarPolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
		fullScreenVlcScrollBarPolicy.setHorizontalStretch (255);
		FullScreenVlcScrollBar_->setSizePolicy (fullScreenVlcScrollBarPolicy);
		
		TogglePlayButton_ = new QToolButton;
		TogglePlayButton_->setDefaultAction (TogglePlay_);
		TogglePlayButton_->setAutoRaise (true);
		StopButton_ = new QToolButton;
		StopButton_->setDefaultAction (Stop_);
		StopButton_->setAutoRaise (true);
		FullScreenButton_ = new QToolButton;
		FullScreenButton_->setDefaultAction (FullScreenAction_);
		FullScreenButton_->setAutoRaise (true);
		
		panelLayout->addWidget (TogglePlayButton_);
		panelLayout->addWidget (StopButton_);
		panelLayout->addWidget (FullScreenButton_);
		panelLayout->addWidget (FullScreenTimeLeft_);
		panelLayout->addWidget (FullScreenVlcScrollBar_);
		panelLayout->addWidget (FullScreenTimeAll_);
		panelLayout->addWidget (FullScreenSoundWidget_);
		panelLayout->setContentsMargins (5, 0, 5, 0);
		
		FullScreenPanel_->setLayout (panelLayout);
		FullScreenPanel_->setWindowOpacity (0.8);
		
		FullScreenTimeLeft_->show ();
		FullScreenVlcScrollBar_->show ();
		FullScreenWidget_->setMouseTracking (true);
		FullScreenPanel_->setMouseTracking (true);

		
		connect (FullScreenWidget_,
				SIGNAL (mouseMove (QMouseEvent*)),
				this,
				SLOT (mouseMoveEvent (QMouseEvent*)));
		
		connect (FullScreenPanel_,
				SIGNAL (mouseMove (QMouseEvent*)),
				this,
				SLOT (mouseMoveEvent (QMouseEvent*)));
		
		connect (FullScreenVlcScrollBar_,
				SIGNAL (changePosition (double)),
				VlcPlayer_,
				SLOT (changePosition (double)));
		
		connect (FullScreenWidget_,
				SIGNAL (customContextMenuRequested (QPoint)),
				this,
				SLOT (generateContextMenu (QPoint)));
		
		connect (FullScreenPanel_,
				SIGNAL (customContextMenuRequested (QPoint)),
				this,
				SLOT (generateContextMenu (QPoint)));
		
		connect (FullScreenWidget_,
				SIGNAL (shown (QShowEvent*)),
				this,
				SLOT (fullScreenPanelRequested ()));
		
		connect (FullScreenWidget_,
				SIGNAL (resized (QResizeEvent*)),
				this,
				SLOT (fullScreenPanelRequested ()));
	}
	
	void VlcWidget::fullScreenPanelRequested ()
	{
		if (!AllowFullScreenPanel_ || !FullScreenWidget_->isVisible ())
			return;
		
		FullScreenPanel_->setGeometry (PANEL_SIDE_MARGIN, FullScreenWidget_->height () - PANEL_BOTTOM_MARGIN - PANEL_HEIGHT, 
									   FullScreenWidget_->width () - PANEL_SIDE_MARGIN * 2, PANEL_HEIGHT);
		if (!FullScreenPanel_->isVisible ())
			FullScreenPanel_->show ();
		else
			FullScreenPanel_->update ();
		
		TerminatePanel_->start ();
	}
	
	void VlcWidget::hideFullScreenPanel ()
	{
		TerminatePanel_->stop ();
		ForbidPanel ();
		FullScreenWidget_->setFocus ();
		FullScreenPanel_->hide ();
	}
	
	void VlcWidget::AllowPanel ()
	{
		AllowFullScreenPanel_ = true;
	}
	
	void VlcWidget::ForbidPanel ()
	{
		AllowFullScreenPanel_ = false;
		QTimer::singleShot (50, this, SLOT (AllowPanel ()));
	}
	
	QMenu* VlcWidget::GenerateMenuForOpenAction ()
	{
		QMenu *result = new QMenu;
		connect (result->addAction (tr ("Open folder")),
				SIGNAL (triggered ()),
				this,
				SLOT (addFolder ()));
		
		connect (result->addAction (tr ("Open url")),
				SIGNAL (triggered ()),
				this,
				SLOT (addUrl ()));
		
		connect (result->addAction (tr ("Open DVD")),
				SIGNAL (triggered ()),
				this,
				SLOT (addDVD ()));
		
		connect (result->addAction (tr ("Open Simple DVD")),
				SIGNAL (triggered ()),
				this,
				SLOT (addSimpleDVD ()));
		
		return result;
	}
	
	void VlcWidget::InitNavigations ()
	{
		NavigateDown_ = new QAction (this);
		NavigateEnter_ = new QAction (this);
		NavigateLeft_ = new QAction (this);
		NavigateRight_ = new QAction (this);
		NavigateUp_ = new QAction (this);
		
		Manager_->RegisterAction ("org.vlc.navigate_down", NavigateDown_, true);
		Manager_->RegisterAction ("org.vlc.navigate_enter", NavigateEnter_, true);
		Manager_->RegisterAction ("org.vlc.navigate_left", NavigateLeft_, true);
		Manager_->RegisterAction ("org.vlc.navigate_right", NavigateRight_, true);
		Manager_->RegisterAction ("org.vlc.navigate_up", NavigateUp_, true);
		
		connect (NavigateDown_, 
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (dvdNavigateDown ()));
				
		connect (NavigateEnter_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (dvdNavigateEnter ()));
		
		connect (NavigateLeft_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (dvdNavigateLeft ()));
		
		connect (NavigateRight_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (dvdNavigateRight ()));
		
		connect (NavigateUp_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (dvdNavigateUp ()));
		
		addAction (NavigateDown_);
		addAction (NavigateEnter_);
		addAction (NavigateLeft_);
		addAction (NavigateRight_);
		addAction (NavigateUp_);
		
		FullScreenWidget_->addAction (NavigateDown_);
		FullScreenWidget_->addAction (NavigateEnter_);
		FullScreenWidget_->addAction (NavigateLeft_);
		FullScreenWidget_->addAction (NavigateRight_);
		FullScreenWidget_->addAction (NavigateUp_);
	}
	
	void VlcWidget::InitVolumeActions()
	{
		IncreaseVolumeAction_ = new QAction (this);
		DecreaseVolumeAction_ = new QAction (this);
		
		Manager_->RegisterAction ("org.vlc.volume_increase", IncreaseVolumeAction_, true);
		Manager_->RegisterAction ("org.vlc.volume_decrease", DecreaseVolumeAction_, true);
		
		connect (IncreaseVolumeAction_,
				SIGNAL (triggered ()),
				SoundWidget_,
				SLOT (increaseVolume ()));
		
		connect (DecreaseVolumeAction_,
				SIGNAL (triggered ()),
				SoundWidget_,
				SLOT (decreaseVolume ()));
		
		addAction (IncreaseVolumeAction_);
		addAction (DecreaseVolumeAction_);
		
		FullScreenWidget_->addAction (IncreaseVolumeAction_);
		FullScreenWidget_->addAction (DecreaseVolumeAction_);
	}
	
	void VlcWidget::InitRewindActions ()
	{
		Plus3Percent_ = new QAction (this);
		Plus10Seconds_ = new QAction (this);
		Minus3Percent_ = new QAction (this);
		Minus10Seconds_ = new QAction (this);
		
		Manager_->RegisterAction ("org.vlc.plus_3_percent", Plus3Percent_, true);
		Manager_->RegisterAction ("org.vlc.plus_10_seconds", Plus10Seconds_, true);
		Manager_->RegisterAction ("org.vlc.minus_3_percent", Minus3Percent_, true);
		Manager_->RegisterAction ("org.vlc.minus_10_seconds", Minus10Seconds_, true);
		
		connect (Plus10Seconds_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (plus10seconds ()));
		
		connect (Minus10Seconds_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (minus10seconds ()));
		
		connect (Plus3Percent_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (plus3percent ()));
		
		connect (Minus3Percent_,
				SIGNAL (triggered ()),
				VlcPlayer_,
				SLOT (minus3percent ()));
		
		addAction (Plus10Seconds_);
		addAction (Plus3Percent_);
		addAction (Minus10Seconds_);
		addAction (Minus3Percent_);
		
		FullScreenWidget_->addAction (Plus10Seconds_);
		FullScreenWidget_->addAction (Plus3Percent_);
		FullScreenWidget_->addAction (Minus10Seconds_);
		FullScreenWidget_->addAction (Minus3Percent_);
	}
	
	void VlcWidget::dropEvent (QDropEvent *event)
	{
		QUrl main = event->mimeData ()->urls () [0];
		event->accept ();
		if (KNOWN_AUDIO_FILE_FORMATS.contains(main.toString ().right (4)))
			VlcPlayer_->addUrl (main);
		else if (KNOWN_SUBTITLES_FILE_FORMATS.contains(main.toString ().right (4)))
			VlcPlayer_->AddSubtitles (main.toEncoded ());
		else
		{
			PlaylistWidget_->Clear ();
			PlaylistWidget_->AddUrl (main, Autostart_);
		}
	}
	
	void VlcWidget::dragEnterEvent (QDragEnterEvent *event)
	{
		if (event->mimeData ()->urls ().size () == 1)
			event->accept ();
	}
	
	void VlcWidget::disableScreenSaver ()
	{
		system ("qdbus org.freedesktop.ScreenSaver /ScreenSaver SimulateUserActivity > /dev/null"); //hello kaffeinety
	}
	
	void VlcWidget::autostartChanged ()
	{
		Autostart_ = XmlSettingsManager::Instance ().property ("Autostart").toBool ();
	}
}
}
