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

#include "core.h"
#include <interfaces/ijobholder.h>
#include <util/tags/tagsfiltermodel.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/core/icoretabwidget.h>
#include <interfaces/core/itagsmanager.h>
#include <interfaces/core/irootwindowsmanager.h>
#include "summarywidget.h"
#include "summarytagsfilter.h"

Q_DECLARE_METATYPE (QToolBar*)

namespace LeechCraft
{
namespace Summary
{
	Core::Core ()
	: MergeModel_ (new Util::MergeModel ({ {}, {}, {} }))
	, Current_ (0)
	{
		MergeModel_->setObjectName ("Core MergeModel");
		MergeModel_->setProperty ("__LeechCraft_own_core_model", true);
	}

	Core& Core::Instance ()
	{
		static Core core;
		return core;
	}

	void Core::Release ()
	{
		delete Current_;
		Current_ = nullptr;
	}

	void Core::SetProxy (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;

		auto rootWM = proxy->GetRootWindowsManager ();
		for (int i = 0; i < rootWM->GetWindowsCount (); ++i)
			handleWindow (i);

		connect (rootWM->GetQObject (),
				SIGNAL (windowAdded (int)),
				this,
				SLOT (handleWindow (int)));

		connect (Proxy_->GetPluginsManager ()->GetQObject (),
				SIGNAL (pluginInjected (QObject*)),
				this,
				SLOT (handlePluginInjected (QObject*)));
	}

	ICoreProxy_ptr Core::GetProxy () const
	{
		return Proxy_;
	}

	void Core::SecondInit ()
	{
		for (const auto plugin : Proxy_->GetPluginsManager ()->GetAllCastableTo<IJobHolder*> ())
			MergeModel_->AddModel (plugin->GetRepresentation ());
	}

	QTreeView* Core::GetCurrentView () const
	{
		return Current_ ? Current_->GetUi ().PluginsTasksTree_ : 0;
	}

	bool Core::SameModel (const QModelIndex& i1, const QModelIndex& i2) const
	{
		const QModelIndex& mapped1 = MapToSourceRecursively (i1);
		const QModelIndex& mapped2 = MapToSourceRecursively (i2);
		return mapped1.model () == mapped2.model ();
	}

	QToolBar* Core::GetControls (const QModelIndex& index) const
	{
		if (!index.isValid ())
			return 0;

		const QVariant& data = index.data (RoleControls);
		return data.value<QToolBar*> ();
	}

	QWidget* Core::GetAdditionalInfo (const QModelIndex& index) const
	{
		if (!index.isValid ())
			return 0;

		const QVariant& data = index.data (RoleAdditionalInfo);
		return data.value<QWidget*> ();
	}

	QSortFilterProxyModel* Core::GetTasksModel () const
	{
		const auto filter = new SummaryTagsFilter ();
		filter->setProperty ("__LeechCraft_own_core_model", true);
		filter->setDynamicSortFilter (true);
		filter->setSourceModel (MergeModel_.get ());
		filter->setFilterCaseSensitivity (Qt::CaseInsensitive);
		return filter;
	}

	QStringList Core::GetTagsForIndex (int index, QAbstractItemModel *model) const
	{
		int starting = 0;
		auto merger = dynamic_cast<Util::MergeModel*> (model);
		if (!merger)
		{
			qWarning () << Q_FUNC_INFO
					<< "could not get model" << model;
			return QStringList ();
		}
		auto modIter = merger->GetModelForRow (index, &starting);

		QStringList ids = (*modIter)->data ((*modIter)->
				index (index - starting, 0), RoleTags).toStringList ();
		QStringList result;
		Q_FOREACH (const QString& id, ids)
			result << Proxy_->GetTagsManager ()->GetTag (id);
		return result;
	}

	QModelIndex Core::MapToSourceRecursively (QModelIndex index) const
	{
		if (!index.isValid ())
			return QModelIndex ();

		while (true)
		{
			const auto model = index.model ();
			if (!model ||
					!model->property ("__LeechCraft_own_core_model").toBool ())
				break;

			auto pModel = qobject_cast<const QAbstractProxyModel*> (model);
			if (pModel)
			{
				index = pModel->mapToSource (index);
				continue;
			}

			auto mModel = qobject_cast<const Util::MergeModel*> (model);
			if (mModel)
			{
				index = mModel->mapToSource (index);
				continue;
			}

			qWarning () << Q_FUNC_INFO
					<< "unhandled parent own core model"
					<< model;
			break;
		}

		return index;
	}

	SummaryWidget* Core::CreateSummaryWidget ()
	{
		if (Current_)
			return Current_;

		SummaryWidget *result = new SummaryWidget ();
		connect (result,
				SIGNAL (changeTabName (const QString&)),
				this,
				SLOT (handleChangeTabName (const QString&)));
		connect (result,
				SIGNAL (needToClose ()),
				this,
				SLOT (handleNeedToClose ()));
		connect (result,
				SIGNAL (raiseTab (QWidget*)),
				this,
				SIGNAL (raiseTab (QWidget*)));
		return result;
	}

	void Core::handleChangeTabName (const QString& name)
	{
		emit changeTabName (Current_, name);
	}

	void Core::handleCurrentTabChanged (int newIndex)
	{
		if (!Current_)
			return;

		auto tw = qobject_cast<ICoreTabWidget*> (sender ());
		if (!tw)
		{
			qWarning () << Q_FUNC_INFO
					<< "sender is not a tab widget:"
					<< sender ();
			return;
		}

		auto newTab = tw->Widget (newIndex);
		Current_->SetUpdatesEnabled (Current_ == newTab);
	}

	void Core::RecoverTabs (const QList<TabRecoverInfo>& infos)
	{
		if (infos.isEmpty () || Current_)
			return;

		const auto& info = infos.first ();

		Current_ = CreateSummaryWidget ();

		for (const auto& pair : info.DynProperties_)
			Current_->setProperty (pair.first, pair.second);

		Current_->RestoreState (info.Data_);

		emit addNewTab (tr ("Summary"), Current_);
		emit changeTabIcon (Current_, QIcon ("lcicons:/plugins/summary/resources/images/summary.svg"));
	}

	void Core::handleNewTabRequested ()
	{
		if (Current_)
		{
			emit raiseTab (Current_);
			return;
		}

		Current_ = CreateSummaryWidget ();

		emit addNewTab (tr ("Summary"), Current_);
		emit changeTabIcon (Current_, QIcon ("lcicons:/plugins/summary/resources/images/summary.svg"));
		emit raiseTab (Current_);
	}

	void Core::handleNeedToClose ()
	{
		emit removeTab (Current_);
		Current_->deleteLater ();
		Current_ = 0;
	}

	void Core::handleWindow (int index)
	{
		auto rootWM = Core::Instance ().GetProxy ()->GetRootWindowsManager ();
		connect (rootWM->GetTabWidget (index)->GetQObject (),
				SIGNAL (currentChanged (int)),
				this,
				SLOT (handleCurrentTabChanged (int)));
	}

	void Core::handlePluginInjected (QObject *object)
	{
		if (auto ijh = qobject_cast<IJobHolder*> (object))
			MergeModel_->AddModel (ijh->GetRepresentation ());
	}
}
}
