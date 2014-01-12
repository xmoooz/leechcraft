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

#include "summary.h"
#include <QIcon>
#include <util/util.h>
#include <interfaces/entitytesthandleresult.h>
#include "core.h"
#include "summarywidget.h"

namespace LeechCraft
{
namespace Summary
{
	void Summary::Init (ICoreProxy_ptr proxy)
	{
		SummaryWidget::SetParentMultiTabs (this);

		Translator_.reset (Util::InstallTranslator ("summary"));

		Core::Instance ().SetProxy (proxy);

		connect (&Core::Instance (),
				SIGNAL (addNewTab (const QString&, QWidget*)),
				this,
				SIGNAL (addNewTab (const QString&, QWidget*)));
		connect (&Core::Instance (),
				SIGNAL (removeTab (QWidget*)),
				this,
				SIGNAL (removeTab (QWidget*)));
		connect (&Core::Instance (),
				SIGNAL (changeTabName (QWidget*, const QString&)),
				this,
				SIGNAL (changeTabName (QWidget*, const QString&)));
		connect (&Core::Instance (),
				SIGNAL (changeTabIcon (QWidget*, const QIcon&)),
				this,
				SIGNAL (changeTabIcon (QWidget*, const QIcon&)));
		connect (&Core::Instance (),
				SIGNAL (changeTooltip (QWidget*, QWidget*)),
				this,
				SIGNAL (changeTooltip (QWidget*, QWidget*)));
		connect (&Core::Instance (),
				SIGNAL (statusBarChanged (QWidget*, const QString&)),
				this,
				SIGNAL (statusBarChanged (QWidget*, const QString&)));
		connect (&Core::Instance (),
				SIGNAL (raiseTab (QWidget*)),
				this,
				SIGNAL (raiseTab (QWidget*)));

		TabClassInfo tabClass =
		{
			"Summary",
			tr ("Summary"),
			GetInfo (),
			GetIcon (),
			50,
			TabFeatures (TFOpenableByRequest | TFByDefault | TFSuggestOpening)
		};
		TabClasses_ << tabClass;
	}

	void Summary::SecondInit ()
	{
		Core::Instance ().SecondInit ();
	}

	void Summary::Release ()
	{
		Core::Instance ().Release ();
		Translator_.reset ();
	}

	QByteArray Summary::GetUniqueID () const
	{
		return "org.LeechCraft.Summary";
	}

	QString Summary::GetName () const
	{
		return "Summary";
	}

	QString Summary::GetInfo () const
	{
		return tr ("Summary of downloads and recent events");
	}

	QIcon Summary::GetIcon () const
	{
		static QIcon icon ("lcicons:/plugins/summary/resources/images/summary.svg");
		return icon;
	}

	TabClasses_t Summary::GetTabClasses () const
	{
		return TabClasses_;
	}

	void Summary::TabOpenRequested (const QByteArray& tabClass)
	{
		if (tabClass == "Summary")
			Core::Instance ().handleNewTabRequested ();
		else
			qWarning () << Q_FUNC_INFO
					<< "unknown tab class"
					<< tabClass;
	}

	QModelIndex Summary::MapToSource (const QModelIndex& index) const
	{
		return Core::Instance ().MapToSourceRecursively (index);
	}

	QTreeView* Summary::GetCurrentView () const
	{
		return Core::Instance ().GetCurrentView ();
	}

	void Summary::RecoverTabs (const QList<TabRecoverInfo>& infos)
	{
		Core::Instance ().RecoverTabs (infos);
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_summary, LeechCraft::Summary::Summary);
