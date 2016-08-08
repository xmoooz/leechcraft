/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#include "blogiquewidget.h"
#include <stdexcept>
#include <QComboBox>
#include <QToolBar>

#if QT_VERSION < 0x050000
#include <QDeclarativeView>
#include <QDeclarativeContext>
#include <QDeclarativeEngine>
#include <QGraphicsObject>
#else
#include <QQuickWidget>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#endif

#include <QInputDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardItemModel>
#include <QWidgetAction>
#include <QWebView>
#include <util/xpc/util.h>
#include <util/sys/paths.h>
#include <util/qml/themeimageprovider.h>
#include <util/qml/colorthemeproxy.h>
#include <interfaces/itexteditor.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/irootwindowsmanager.h>
#include <interfaces/core/iiconthememanager.h>
#include <interfaces/imwproxy.h>
#include "interfaces/blogique/ibloggingplatform.h"
#include "interfaces/blogique/iblogiquesidewidget.h"
#include "interfaces/blogique/iprofile.h"
#include "blogentrieswidget.h"
#include "blogique.h"
#include "commentswidget.h"
#include "core.h"
#include "draftentrieswidget.h"
#include "dummytexteditor.h"
#include "profiledialog.h"
#include "storagemanager.h"
#include "submittodialog.h"
#include "tagsproxymodel.h"
#include "updateentriesdialog.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Blogique
{
	QObject *BlogiqueWidget::S_ParentMultiTabs_ = 0;
	const QString ImageProviderID = "ThemeIcons";

	BlogiqueWidget::BlogiqueWidget (QWidget *parent)
	: QWidget (parent)
#if QT_VERSION < 0x050000
	, TagsCloud_ (new QDeclarativeView)
	, Tags_ (new QDeclarativeView)
#else
	, TagsCloud_ (new QQuickWidget)
	, Tags_ (new QQuickWidget)
#endif
	, ToolBar_ (new QToolBar)
	, ProgressToolBar_ (new QToolBar (this))
	, AccountsBox_ (new QComboBox ())
	, DraftEntriesWidget_ (new DraftEntriesWidget (this))
	, BlogEntriesWidget_ (new BlogEntriesWidget)
	, CommentsWidget_ (new CommentsWidget (this))
	, TagsProxyModel_ (new TagsProxyModel (this))
	, TagsModel_ (new QStandardItemModel (this))
	{
		Ui_.setupUi (this);

#if QT_VERSION < 0x050000
		TagsCloud_->setResizeMode (QDeclarativeView::SizeRootObjectToView);
		Tags_->setResizeMode (QDeclarativeView::SizeRootObjectToView);
#else
		TagsCloud_->setResizeMode (QQuickWidget::SizeRootObjectToView);
		Tags_->setResizeMode (QQuickWidget::SizeRootObjectToView);
#endif

		TagsCloud_->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
		Ui_.PluginOptionsWidget_->layout ()->addWidget (TagsCloud_);

		Tags_->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Ignored);
		Ui_.TagsBox_->layout ()->addWidget (Tags_);

		auto dwa = static_cast<Qt::DockWidgetArea> (XmlSettingsManager::Instance ()
				.Property ("DockWidgetArea", Qt::RightDockWidgetArea).toInt ());
		if (dwa == Qt::NoDockWidgetArea)
			dwa = Qt::RightDockWidgetArea;

		auto rootWM = Core::Instance ().GetCoreProxy ()->GetRootWindowsManager ();
		auto mw = rootWM->GetMWProxy (rootWM->GetPreferredWindowIndex ());
		Ui_.SideWidget_->setWindowIcon (Core::Instance ().GetIcon ());
		Ui_.SideWidget_->toggleViewAction ()->setIcon (Core::Instance ().GetIcon ());
		mw->AddDockWidget (dwa, Ui_.SideWidget_);
		mw->AssociateDockWidget (Ui_.SideWidget_, this);
		mw->ToggleViewActionVisiblity (Ui_.SideWidget_, false);

		SetTextEditor ();
		SetDefaultSideWidgets ();

		connect (&Core::Instance (),
				SIGNAL (requestEntriesBegin ()),
				this,
				SLOT (handleRequestEntriesBegin ()));

		QProgressBar *submitProgressBar = new QProgressBar;
		submitProgressBar->setRange (0, 0);
		ProgressBarLabel_ = new QLabel;
		ProgressBarLabelAction_ = ProgressToolBar_->addWidget (ProgressBarLabel_);
		ProgressBarAction_ = ProgressToolBar_->addWidget (submitProgressBar);
		submitProgressBar->setOrientation (Qt::Horizontal);

		SetToolBarActions ();

		connect (this,
				SIGNAL (addNewTab (QString, QWidget*)),
				&Core::Instance (),
				SIGNAL (addNewTab (QString, QWidget*)));
		connect (this,
				SIGNAL (changeTabName (QWidget*, QString)),
				&Core::Instance (),
				SIGNAL (changeTabName (QWidget*, QString)));

		connect (BlogEntriesWidget_,
				SIGNAL (fillCurrentWidgetWithBlogEntry (Entry)),
				this,
				SLOT (fillCurrentTabWithEntry (Entry)));
		connect (BlogEntriesWidget_,
				SIGNAL (fillNewWidgetWithBlogEntry (Entry, QByteArray)),
				this,
				SLOT (fillNewTabWithEntry (Entry, QByteArray)));
		connect (BlogEntriesWidget_,
				SIGNAL (entryAboutToBeRemoved ()),
				this,
				SLOT (handleRemovingEntryBegin ()));
		connect (BlogEntriesWidget_,
				SIGNAL (entriesListUpdated ()),
				this,
				SLOT (handleRequestEntriesEnd ()));
		connect (DraftEntriesWidget_,
				SIGNAL (fillCurrentWidgetWithDraftEntry (Entry)),
				this,
				SLOT (fillCurrentTabWithEntry (Entry)));
		connect (DraftEntriesWidget_,
				SIGNAL (fillNewWidgetWithDraftEntry (Entry, QByteArray)),
				this,
				SLOT (fillNewTabWithEntry (Entry, QByteArray)));
		connect (Ui_.Subject_,
				SIGNAL (textChanged (QString)),
				this,
				SLOT (handleEntryChanged (QString)));

		ShowProgress ();

		DraftEntriesWidget_->loadDraftEntries ();

		PrepareQmlWidgets ();
	}

	QObject* BlogiqueWidget::ParentMultiTabs ()
	{
		return S_ParentMultiTabs_;
	}

	TabClassInfo BlogiqueWidget::GetTabClassInfo () const
	{
		return qobject_cast<Plugin*> (S_ParentMultiTabs_)->
				GetTabClasses ().first ();
	}

	QToolBar* BlogiqueWidget::GetToolBar () const
	{
		return ToolBar_;
	}

	void BlogiqueWidget::Remove ()
	{
		emit removeTab (this);
		BlogEntriesWidget_->deleteLater ();
		PostTargetBox_->deleteLater ();
		Ui_.SideWidget_->deleteLater ();
		deleteLater ();
	}

	void BlogiqueWidget::FillWidget (const Entry& e, const QByteArray& accId)
	{
		for (int i = 0; !accId.isEmpty () && i < AccountsBox_->count (); ++i)
		{
			if (Id2Account_.contains (i) &&
					Id2Account_ [i]->GetAccountID () == accId)
			{
				AccountsBox_->setCurrentIndex (i);
				break;
			}
		}

		IAccount *acc = Id2Account_.value (AccountsBox_->currentIndex ());
		auto ibp = qobject_cast<IBloggingPlatform*> (acc->GetParentBloggingPlatform ());
		if (ibp &&
				(ibp->GetFeatures () & IBloggingPlatform::BPFSelectablePostDestination) &&
				PostTargetBox_)
			PostTargetBox_->setCurrentIndex (PostTargetBox_->findText (e.Target_, Qt::MatchFixedString));

		EntryType_ = e.EntryType_;
		EntryId_ = e.EntryId_;
		EntryUrl_ = e.EntryUrl_;
		Ui_.Subject_->setText (e.Subject_);
		PostEdit_->SetContents (e.Content_, ContentType::HTML);

		for (auto w : SidePluginsWidgets_)
		{
			auto ibsw = qobject_cast<IBlogiqueSideWidget*> (w);
			if (!ibsw)
				continue;

			switch (ibsw->GetWidgetType ())
			{
			case SideWidgetType::PostOptionsSideWidget:
			{
				QVariantMap params = e.PostOptions_;
				params ["content"] = e.Content_;
				ibsw->SetPostOptions (params);

				SetPostTags (e.Tags_);
				SetPostDate (e.Date_);
				break;
			}
			case SideWidgetType::CustomSideWidget:
				ibsw->SetCustomData (e.CustomData_);
				break;
			default:
				break;
			}
		}

		EntryChanged_ = false;
	}

	void BlogiqueWidget::SetParentMultiTabs (QObject *tab)
	{
		S_ParentMultiTabs_ = tab;
	}

	QByteArray BlogiqueWidget::GetTabRecoverData () const
	{
		QByteArray result;
		auto entry = GetCurrentEntry ();
		if (entry.IsEmpty ())
			return result;

		QByteArray accId;
		IAccount *acc = Id2Account_.value (AccountsBox_->currentIndex ());
		if (acc)
			accId = acc->GetAccountID ();

		QDataStream stream (&result, QIODevice::WriteOnly);
		stream << qint64 (1)
				<< entry.Subject_
				<< entry.Content_
				<< entry.Date_
				<< entry.Tags_
				<< entry.Target_
				<< entry.PostOptions_
				<< entry.CustomData_
				<< accId;

		return result;
	}

	QString BlogiqueWidget::GetTabRecoverName () const
	{
		return !Ui_.Subject_->text ().isEmpty () ?
			Ui_.Subject_->text () :
			tr ("No subject");
	}

	QIcon BlogiqueWidget::GetTabRecoverIcon () const
	{
		return Core::Instance ().GetIcon ();
	}

	void BlogiqueWidget::SetTextEditor ()
	{
		auto plugs = Core::Instance ().GetCoreProxy ()->
				GetPluginsManager ()->GetAllCastableTo<ITextEditor*> ();

		QVBoxLayout *editFrameLay = new QVBoxLayout ();
		editFrameLay->setContentsMargins (0, 0, 0, 0);
		Ui_.PostFrame_->setLayout (editFrameLay);

		if (plugs.isEmpty ())
		{
			DummyTextEditor *dummy = new DummyTextEditor (this);
			PostEdit_ = qobject_cast<IEditorWidget*> (dummy);
			if (PostEdit_)
			{
				connect (dummy,
						SIGNAL (textChanged ()),
						this,
						SLOT (handleEntryChanged ()));
				PostEditWidget_ = dummy;
				editFrameLay->setContentsMargins (4, 4, 4, 4);
				editFrameLay->addWidget (dummy);
			}
			else
				delete dummy;
		}

		Q_FOREACH (ITextEditor *plug, plugs)
		{
			if (!plug->SupportsEditor (ContentType::PlainText))
				continue;

			QWidget *w = plug->GetTextEditor (ContentType::PlainText);
			PostEdit_ = qobject_cast<IEditorWidget*> (w);
			if (!PostEdit_)
			{
				delete w;
				continue;
			}

			connect (w,
					SIGNAL (textChanged ()),
					this,
					SLOT (handleEntryChanged ()));
			PostEditWidget_ = w;
			editFrameLay->addWidget (w);
			break;
		}
	}

	void BlogiqueWidget::SetToolBarActions ()
	{
		Ui_.NewEntry_->setProperty ("ActionIcon", "document-new");
		ToolBar_->addAction (Ui_.NewEntry_);
		connect (Ui_.NewEntry_,
				SIGNAL (triggered ()),
				this,
				SLOT (newEntry ()));

		Ui_.SaveEntry_->setProperty ("ActionIcon", "document-save");
		ToolBar_->addAction (Ui_.SaveEntry_);
		connect (Ui_.SaveEntry_,
				SIGNAL (triggered ()),
				this,
				SLOT (saveEntry ()));

		Ui_.SaveNewEntry_->setProperty ("ActionIcon", "document-save-as");
		ToolBar_->addAction (Ui_.SaveNewEntry_);
		connect (Ui_.SaveNewEntry_,
				SIGNAL (triggered ()),
				this,
				SLOT (saveNewEntry ()));

		Ui_.Submit_->setProperty ("ActionIcon", "svn-commit");
		ToolBar_->addAction (Ui_.Submit_);
		connect (Ui_.Submit_,
				SIGNAL (triggered ()),
				this,
				SLOT (submit ()));

		Ui_.SubmitTo_->setProperty ("ActionIcon", "mail-folder-outbox");
		connect (Ui_.SubmitTo_,
				SIGNAL (triggered ()),
				this,
				SLOT (submitTo ()));

		Ui_.OpenInBrowser_->setProperty ("ActionIcon", "applications-internet");
		Ui_.ShowProfile_->setProperty ("ActionIcon", "user-properties");
		Ui_.PreviewPost_->setProperty ("ActionIcon", "view-preview");

		ToolBar_->addSeparator ();

		QList<QAction*> editorActions;
		if (PostEdit_)
		{
			editorActions << PostEdit_->GetEditorAction (EditorAction::Find);
			editorActions << PostEdit_->GetEditorAction (EditorAction::Replace);
			editorActions.removeAll (0);
		}
		if (!editorActions.isEmpty ())
		{
			PostEdit_->AppendSeparator ();
			for (auto action : editorActions)
				PostEdit_->AppendAction (action);
			PostEdit_->AppendSeparator ();
		}

		for (IAccount *acc : Core::Instance ().GetAccounts ())
		{
			AccountsBox_->addItem (acc->GetAccountName ());
			Id2Account_ [AccountsBox_->count () - 1] = acc;
		}
		int index = AccountsBox_->findText (XmlSettingsManager::Instance ()
				.property ("LastActiveAccountName").toString (),
					Qt::MatchFixedString);
		if (index > AccountsBox_->count ())
			index = -1;

		AccountsBox_->addItem (Core::Instance ().GetCoreProxy ()->
					GetIconThemeManager ()->GetIcon ("list-add"),
				tr ("Add new account..."));

		AccountsBoxAction_ = ToolBar_->addWidget (AccountsBox_);

		PostTargetBox_ = new QComboBox;

		AccountsBox_->setCurrentIndex (index);

		connect (AccountsBox_,
				SIGNAL (currentIndexChanged (int)),
				this,
				SLOT (handleCurrentAccountChanged (int)));

		handleCurrentAccountChanged (AccountsBox_->currentIndex ());
	}

	void BlogiqueWidget::SetDefaultSideWidgets ()
	{
		Ui_.DockWidgetGridLayout_->addWidget (Ui_.Tools_, 1, 0);
		Ui_.DockWidgetGridLayout_->addWidget (ProgressToolBar_, 0, 0);

		for (int i = 0; i < Ui_.Tools_->count (); ++i)
		{
			auto w = Ui_.Tools_->widget (i);
			Ui_.Tools_->removeItem (i);
			w->deleteLater ();
		}
		Ui_.Tools_->addItem (DraftEntriesWidget_, DraftEntriesWidget_->GetName ());
		Ui_.Tools_->addItem (CommentsWidget_, CommentsWidget_->GetName ());
	}

	void BlogiqueWidget::PrepareQmlWidgets ()
	{
		TagsProxyModel_->setSourceModel (TagsModel_);
		Tags_->rootContext ()->setContextProperty ("mainWidget",
				this);
		Tags_->rootContext ()->setContextProperty ("tagsModel",
				TagsProxyModel_);
		Tags_->rootContext ()->setContextProperty ("colorProxy",
				new Util::ColorThemeProxy (Core::Instance ()
						.GetCoreProxy ()->GetColorThemeManager (), this));
		Tags_->engine ()->addImageProvider (ImageProviderID,
				new Util::ThemeImageProvider (Core::Instance ().GetCoreProxy ()));

		for (const auto& cand : Util::GetPathCandidates (Util::SysPath::QML, ""))
			Tags_->engine ()->addImportPath (cand);

		Tags_->setSource (QUrl::fromLocalFile (Util::GetSysPath (Util::SysPath::QML,
				"blogique", "tagwidget.qml")));
		connect (Tags_->rootObject (),
				SIGNAL (tagTextChanged (QString)),
				this,
				SLOT (handleTagTextChanged (QString)));

		TagsCloud_->setVisible (Ui_.SelectTags_->isChecked ());
		TagsCloud_->rootContext ()->setContextProperty ("colorProxy",
				new Util::ColorThemeProxy (Core::Instance ()
						.GetCoreProxy ()->GetColorThemeManager (), this));
		TagsCloud_->setSource (QUrl::fromLocalFile (Util::GetSysPath (Util::SysPath::QML,
					"blogique", "tagscloud.qml")));
		connect (TagsCloud_->rootObject (),
				SIGNAL (tagSelected (QString)),
				this,
				SIGNAL (tagSelected (QString)));
		connect (Tags_->rootObject (),
				SIGNAL (tagRemoved (QString)),
				this,
				SLOT (handleTagRemoved (QString)));
		connect (Tags_->rootObject (),
				SIGNAL (tagAdded (QString)),
				this,
				SLOT (handleTagAdded (QString)));
	}

	void BlogiqueWidget::RemovePostingTargetsWidget ()
	{
		if (PostTargetAction_ &&
				PostTargetAction_->isVisible ())
		{
			PostTargetAction_->setVisible (false);
			PostTargetBox_->clear ();
		}
	}

	void BlogiqueWidget::SetPostDate(const QDateTime& dt)
	{
		Ui_.TimestampBox_->setChecked (true);
		Ui_.Year_->setValue (dt.date ().year ());
		Ui_.Month_->setCurrentIndex (dt.date ().month () - 1);
		Ui_.Date_->setValue (dt.date ().day ());
		Ui_.Time_->setTime (dt.time ());
	}

	QDateTime BlogiqueWidget::GetPostDate () const
	{
		return !Ui_.TimestampBox_->isChecked () ?
				QDateTime::currentDateTime () :
				QDateTime (QDate (Ui_.Year_->value (),
							Ui_.Month_->currentIndex () + 1,
							Ui_.Date_->value ()),
						Ui_.Time_->time ());
	}

	void BlogiqueWidget::SetPostTags (const QStringList& tags)
	{
		QMetaObject::invokeMethod (Tags_->rootObject (),
				"setTags",
				Q_ARG (QVariant, QVariant::fromValue (tags)));
		QMetaObject::invokeMethod (TagsCloud_->rootObject (),
				"setTags",
				Q_ARG (QVariant, QVariant::fromValue (tags)));
	}

	QStringList BlogiqueWidget::GetPostTags () const
	{
		QVariant tags;
		QMetaObject::invokeMethod (Tags_->rootObject (),
				"getTags",
				Q_RETURN_ARG (QVariant, tags));
		return tags.toStringList ();
	}

	void BlogiqueWidget::ClearEntry ()
	{
		Ui_.Subject_->clear ();
		PostEdit_->SetContents (QString (), ContentType::PlainText);
		EntryUrl_.clear ();
		EntryId_ = 0;

		for (auto w : SidePluginsWidgets_)
		{
			auto ibsw = qobject_cast<IBlogiqueSideWidget*> (w);
			if (!ibsw)
				continue;

			switch (ibsw->GetWidgetType ())
			{
			case SideWidgetType::PostOptionsSideWidget:
			{
				ibsw->SetPostOptions (QVariantMap ());
				SetPostDate (QDateTime::currentDateTime ());
				SetPostTags (QStringList ());
				break;
			}
			case SideWidgetType::CustomSideWidget:
				ibsw->SetCustomData (QVariantMap ());
				break;
			default:
				break;
			}
		}
		EntryChanged_ = false;
	}

	Entry BlogiqueWidget::GetCurrentEntry (bool interactive) const
	{
		if (!PostEdit_)
			return Entry ();

		const QString& content = PostEdit_->GetContents (ContentType::HTML);
		if (interactive &&
				content.isEmpty ())
		{
			QMessageBox::warning (0,
					tr ("LeechCraft"),
					tr ("Entry can't be empty."));
			return Entry ();
		}

		Entry e;
		for (auto w : SidePluginsWidgets_)
		{
			auto ibsw = qobject_cast<IBlogiqueSideWidget*> (w);
			if (!ibsw)
				continue;

			switch (ibsw->GetWidgetType ())
			{
			case SideWidgetType::PostOptionsSideWidget:
			{
				e.PostOptions_.unite (ibsw->GetPostOptions ());
				e.Date_ = GetPostDate ();
				e.Tags_ = GetPostTags ();
				break;
			}
			case SideWidgetType::CustomSideWidget:
				e.CustomData_.unite (ibsw->GetCustomData ());
				break;
			default:
				break;
			}
		}

		e.Target_ = PostTargetBox_->currentText ();
		e.Content_ = content;
		e.Subject_ = Ui_.Subject_->text ();
		e.EntryType_ = EntryType_;
		e.EntryId_ = EntryId_;
		if (e.Date_.isNull ())
			e.Date_ = QDateTime::currentDateTime ();

		return e;
	}

	void BlogiqueWidget::ShowProgress (const QString& labelText)
	{
		ProgressBarLabelAction_->setVisible (!labelText.isEmpty ());
		ProgressBarLabel_->setText (labelText);
		ProgressBarAction_->setVisible (!labelText.isEmpty ());
	}

	void BlogiqueWidget::handleAutoSave ()
	{
		if (!EntryChanged_)
			return;

		saveEntry ();
		EntryChanged_ = true;
	}

	void BlogiqueWidget::handleEntryPosted ()
	{
		ShowProgress ();
	}

	void BlogiqueWidget::handleCurrentAccountChanged (int id)
	{
		if (id == -1)
			return;

		if (id == AccountsBox_->count () - 1)
		{
			Core::Instance ().GetCoreProxy ()->GetPluginsManager ()->
					OpenSettings (ParentMultiTabs ());
			AccountsBox_->setCurrentIndex (-1);
			return;
		}

		if (!Id2Account_.contains (id))
			return;

		if (PrevAccountId_ != -1)
		{
			auto ibp = qobject_cast<IBloggingPlatform*> (Id2Account_ [PrevAccountId_]->
					GetParentBloggingPlatform ());
			for (auto action : ibp->GetEditorActions ())
				PostEdit_->RemoveAction (action);

			for (auto action : InlineTagInserters_)
				PostEdit_->RemoveAction (action);

			on_SelectTags__toggled (false);
			Ui_.SelectTags_->setChecked (false);
			for (auto w : SidePluginsWidgets_)
			{
				auto ibsw = qobject_cast<IBlogiqueSideWidget*> (w);
				if (ibsw &&
						ibsw->GetWidgetType () == SideWidgetType::PostOptionsSideWidget)
					Ui_.PluginOptionsWidget_->layout ()->removeWidget (w);
				else
				{
					int index = Ui_.Tools_->indexOf (w);
					Ui_.Tools_->removeItem (index);
				}

				if (w)
					w->deleteLater ();
			}
			SidePluginsWidgets_.clear ();

			RemovePostingTargetsWidget ();

			ToolBar_->removeAction (Ui_.OpenInBrowser_);
			ToolBar_->removeAction (Ui_.ShowProfile_);
			ToolBar_->removeAction (Ui_.PreviewPost_);
			ToolBar_->removeAction (Ui_.SubmitTo_);

		}

		auto account = Id2Account_ [id];
		BlogEntriesWidget_->clear ();
		BlogEntriesWidget_->SetAccount (account);

		auto ibp = qobject_cast<IBloggingPlatform*> (account->
				GetParentBloggingPlatform ());

		if (ibp->GetFeatures () & IBloggingPlatform::BPFSelectablePostDestination)
		{
			if (!PostTargetAction_)
				PostTargetAction_ = ToolBar_->addWidget (PostTargetBox_);
			else
				PostTargetAction_->setVisible (true);

			IProfile *profile = qobject_cast<IProfile*> (account->GetProfile ());
			if (profile)
				for (const auto& target : profile->GetPostingTargets ())
					PostTargetBox_->addItem (target.first, target.second);
		}

		if (ibp->GetFeatures () & IBloggingPlatform::BPFLocalBlog)
			ToolBar_->insertAction (AccountsBoxAction_, Ui_.SubmitTo_);
		else
		{
			ToolBar_->insertAction (AccountsBoxAction_, Ui_.OpenInBrowser_);
			if (ibp->GetFeatures () & IBloggingPlatform::BPFSupportsProfiles)
				ToolBar_->insertAction (AccountsBoxAction_, Ui_.ShowProfile_);

			if (ibp->GetFeatures () & IBloggingPlatform::BPFPostPreviewSupport)
				ToolBar_->insertAction (AccountsBoxAction_, Ui_.PreviewPost_);
		}

		for (auto action : ibp->GetEditorActions ())
			PostEdit_->AppendAction (action);

		for (const auto& inserter : ibp->GetInlineTagInserters ())
		{
			if (auto iahe = qobject_cast<IAdvancedHTMLEditor*> (PostEditWidget_))
			{
				auto act = iahe->AddInlineTagInserter (inserter.TagName_, inserter.Parameters_);
				inserter.ActionCustomizer_ (act);
				InlineTagInserters_ << act;
			}
		}

		if (auto iahe = qobject_cast<IAdvancedHTMLEditor*> (PostEditWidget_))
			iahe->SetCustomTags (ibp->GetCustomTags ());

		bool exists = false;

		for (int i = 0; i < Ui_.Tools_->count (); ++i)
			if (Ui_.Tools_->widget (i) == BlogEntriesWidget_)
			{
				exists = true;
				break;
			}
		if (!exists)
			Ui_.Tools_->insertItem (0, BlogEntriesWidget_, BlogEntriesWidget_->GetName ());

		for (auto w : ibp->GetBlogiqueSideWidgets ())
		{
			IBlogiqueSideWidget *ibsw = qobject_cast<IBlogiqueSideWidget*> (w);
			if (!ibsw)
			{
				qWarning () << Q_FUNC_INFO
						<< "Side widget"
						<< w
						<< "from"
						<< ibp
						<< "is not an IBlogiqueSideWidget";
				continue;
			}

			SidePluginsWidgets_ << w;
			ibsw->SetAccount (account->GetQObject ());
			if (ibsw->GetWidgetType () == SideWidgetType::PostOptionsSideWidget)
				Ui_.PluginOptionsWidget_->layout ()->addWidget (w);
			else
				Ui_.Tools_->addItem (w, ibsw->GetName ());
		}

		PrevAccountId_ = id;

		XmlSettingsManager::Instance ().setProperty ("LastActiveAccountName",
				Id2Account_ [id]->GetAccountName ());
	}

	void BlogiqueWidget::fillCurrentTabWithEntry (const Entry& entry)
	{
		if (EntryChanged_)
		{
			IAccount *acc = Id2Account_.value (AccountsBox_->currentIndex ());
			if (!acc)
				return;
			int res = QMessageBox::question (this,
					"LeechCraft Blogique",
					tr ("You have unsaved changes in your current tab."
						" Do you want to open this entry in a new tab instead?"),
					QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
			switch (res)
			{
			case QMessageBox::Yes:
				fillNewTabWithEntry (entry, acc->GetAccountID ());
				break;
			case QMessageBox::No:
				FillWidget (entry);
				break;
			case QMessageBox::Cancel:
			default:
				break;
			}
		}
		else
			FillWidget (entry);

		emit changeTabName (this, entry.Subject_);
	}

	void BlogiqueWidget::fillNewTabWithEntry (const Entry& entry,
			const QByteArray& accountId)
	{
		auto w = Core::Instance ().CreateBlogiqueWidget ();
		w->FillWidget (entry, accountId);
		emit addNewTab ("Blogique", w);
		emit changeTabName (w, entry.Subject_);
	}

	void BlogiqueWidget::handleEntryChanged (const QString&)
	{
		EntryChanged_ = true;
		emit tabRecoverDataChanged ();
	}

	void BlogiqueWidget::handleRemovingEntryBegin ()
	{
		ShowProgress (tr ("Removing entry..."));
	}

	void BlogiqueWidget::handleEntryRemoved ()
	{
		ShowProgress ();
	}

	void BlogiqueWidget::handleRequestEntriesBegin ()
	{
		ShowProgress (tr ("Updating entries..."));
	}

	void BlogiqueWidget::handleRequestEntriesEnd ()
	{
		ShowProgress ();
	}

	void BlogiqueWidget::handleTagsUpdated (const QHash<QString, int>& tags)
	{
		TagsModel_->clear ();
		QMetaObject::invokeMethod (TagsCloud_->rootObject (),
				"clearTags");

		int max = 0;
		for (const auto& tag : tags.keys ())
		{
			QStandardItem *item = new QStandardItem (tag);
			item->setData (tags.value (tag), TagFrequency);
			TagsModel_->appendRow (item);
			if (tags.value (tag) > max)
				max = tags.value (tag);
		}

		for (const auto& tag : tags.keys ())
			QMetaObject::invokeMethod (TagsCloud_->rootObject (),
					"addTag",
					Q_ARG (QVariant, tag),
					Q_ARG (QVariant, tags.value (tag)),
					Q_ARG (QVariant, max));
		QMetaObject::invokeMethod (TagsCloud_->rootObject (),
				"updateTagsCloud");
	}

	void BlogiqueWidget::handleInsertTag (const QString& tag)
	{
		auto iahe = qobject_cast<IAdvancedHTMLEditor*> (PostEditWidget_);
		if (!iahe)
			return;

		iahe->InsertHTML (tag);
	}

	void BlogiqueWidget::handleGotError (int errorCode,
			const QString& errorString, const QString& localizedErrorString)
	{
		ShowProgress ();
		qWarning () << Q_FUNC_INFO
				<< "error code:"
				<< errorCode
				<< "error text:"
				<< errorString;

		Core::Instance ().SendEntity (Util::MakeNotification ("Blogique",
				tr ("%1 (original message: %2)")
					.arg (localizedErrorString, errorString),
				PWarning_));
	}

	void BlogiqueWidget::handleAccountAdded (QObject *acc)
	{
		if (auto account = qobject_cast<IAccount*> (acc))
		{
			AccountsBox_->insertItem (AccountsBox_->count () - 1, account->GetAccountName ());
			Id2Account_ [AccountsBox_->count () - 2] = account;
		}
	}

	void BlogiqueWidget::handleAccountRemoved (QObject *acc)
	{
		if (auto account = qobject_cast<IAccount*> (acc))
		{
			for (const auto& value : Id2Account_.values ())
			{
				if (value != account)
					continue;
				auto id = Id2Account_.key (account);
				Id2Account_.remove (id);
				AccountsBox_->removeItem (id);
				break;
			}
		}
	}

	void BlogiqueWidget::newEntry ()
	{
		if (EntryChanged_)
		{
			int res = QMessageBox::question (this,
					"LeechCraft Blogique",
					tr ("Do you want to save current entry?"),
					QMessageBox::Yes | QMessageBox::No);
			switch (res)
			{
			case QMessageBox::Yes:
				saveEntry ();
				ClearEntry ();
				break;
			case QMessageBox::No:
				ClearEntry ();
				break;
			default:
				return;
			}
		}
		else
			ClearEntry ();
	}

	void BlogiqueWidget::saveEntry (const Entry& entry)
	{
		EntryChanged_ = false;
		EntryType_ = EntryType::Draft;
		const Entry& e = entry.IsEmpty () ?
			GetCurrentEntry (true) :
			entry;
		if (!e.IsEmpty ())
		{
			try
			{
				switch (e.EntryType_)
				{
				case EntryType::Draft:
					EntryId_ = Core::Instance ().GetStorageManager ()->UpdateDraft (e, EntryId_);
					break;
				case EntryType::BlogEntry:
				case EntryType::None:
					EntryId_ = Core::Instance ().GetStorageManager ()->SaveNewDraft (e);
					break;
				}
			}
			catch (const std::runtime_error& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "error saving draft"
						<< e.what ();
			}
		}
		else
			EntryType_ = EntryType::None;

		DraftEntriesWidget_->loadDraftEntries ();
	}

	void BlogiqueWidget::saveNewEntry (const Entry& entry)
	{
		EntryChanged_ = false;
		EntryType_ = EntryType::Draft;
		const Entry& e = entry.IsEmpty () ?
			GetCurrentEntry (true) :
			entry;

		if (!e.IsEmpty ())
		{
			try
			{
				EntryId_ = Core::Instance ().GetStorageManager ()->SaveNewDraft (e);
			}
			catch (const std::runtime_error& e)
			{
				qWarning () << Q_FUNC_INFO
						<< "error saving draft"
						<< e.what ();
			}
		}
		else
			EntryType_ = EntryType::None;

		DraftEntriesWidget_->loadDraftEntries ();
	}

	void BlogiqueWidget::submit (const Entry& event)
	{
		IAccount *acc = Id2Account_.value (AccountsBox_->currentIndex ());
		if (!acc)
			return;

		EntryChanged_ = false;
		const auto& e = event.IsEmpty () ?
			GetCurrentEntry (true) :
			event;

		if (!e.IsEmpty ())
		{
			if (EntryType_ == EntryType::BlogEntry)
			{
				QMessageBox mbox (QMessageBox::Question,
						"LeechCraft",
						tr ("Do you want to update entry or to post new one?"),
						QMessageBox::Yes | QMessageBox::Cancel,
						this);
				mbox.setDefaultButton (QMessageBox::Cancel);
				mbox.setButtonText (QMessageBox::Yes, tr ("Update post"));
				QPushButton newPostButton (tr ("Post new"));
				mbox.addButton (&newPostButton, QMessageBox::AcceptRole);

				if (mbox.exec () == QMessageBox::Cancel)
					return;
				else if (mbox.clickedButton () == &newPostButton)
				{
					ShowProgress (tr ("Posting entry..."));
					acc->submit (e);
				}
				else
				{
					ShowProgress (tr ("Posting entry..."));
					acc->UpdateEntry (e);
				}
			}
			else
			{
				ShowProgress (tr ("Posting entry..."));
				acc->submit (e);
			}

		}
	}

	void BlogiqueWidget::submitTo (const Entry& entry)
	{
		SubmitToDialog dlg;
		if (dlg.exec () == QDialog::Rejected)
			return;

		for (const auto& pair : dlg.GetPostingTargets ())
		{
			auto e = entry.IsEmpty () ?
				GetCurrentEntry (true) :
				entry;
			e.Target_ = pair.second;
			pair.first->submit (e);
		}
	}

	void BlogiqueWidget::on_SideWidget__dockLocationChanged (Qt::DockWidgetArea area)
	{
		if (area != Qt::AllDockWidgetAreas &&
				area != Qt::NoDockWidgetArea)
			XmlSettingsManager::Instance ().setProperty ("DockWidgetArea", area);
	}

	void BlogiqueWidget::on_ShowProfile__triggered ()
	{
		IAccount *acc = Id2Account_.value (AccountsBox_->currentIndex ());
		if (!acc)
			return;

		ProfileDialog *pd = new ProfileDialog (acc, this);
		pd->setAttribute (Qt::WA_DeleteOnClose);
		pd->show ();
	}

	void BlogiqueWidget::on_CurrentTime__released ()
	{
		QDateTime current = QDateTime::currentDateTime ();
		Ui_.Year_->setValue (current.date ().year ());
		Ui_.Month_->setCurrentIndex (current.date ().month () - 1);
		Ui_.Date_->setValue (current.date ().day ());
		Ui_.Time_->setTime (current.time ());
	}

	void BlogiqueWidget::on_SelectTags__toggled (bool checked)
	{
		const auto& layout = Ui_.PluginOptionsWidget_->layout ();
		for (int i = 0; i < layout->count (); ++i)
		{
			auto w = layout->itemAt (i)->widget ();
			if (w)
				w->setVisible (!checked);
		}
		TagsCloud_->setVisible (checked);
	}

	void BlogiqueWidget::handleTagTextChanged (const QString& text)
	{
		TagsProxyModel_->setFilterFixedString (text);
		TagsProxyModel_->countUpdated ();
	}

	void BlogiqueWidget::handleTagRemoved (const QString& tag)
	{
		QMetaObject::invokeMethod (TagsCloud_->rootObject (),
				"selectTag",
				Q_ARG (QVariant, tag),
				Q_ARG (QVariant, false));
	}

	void BlogiqueWidget::handleTagAdded (const QString& tag)
	{
		QMetaObject::invokeMethod (TagsCloud_->rootObject (),
				"selectTag",
				Q_ARG (QVariant, tag),
				Q_ARG (QVariant, true));
	}

	void BlogiqueWidget::on_OpenInBrowser__triggered ()
	{
		if (EntryUrl_.isEmpty ())
			return;

		Core::Instance ().SendEntity (Util::MakeEntity (EntryUrl_,
				{},
				FromUserInitiated | OnlyHandle));
	}

	void BlogiqueWidget::on_PreviewPost__triggered ()
	{
		IAccount *acc = Id2Account_.value (AccountsBox_->currentIndex ());
		if (!acc)
			return;

		const auto& e = GetCurrentEntry (true);

		if (!e.IsEmpty ())
			acc->preview (e);
	}
}
}

