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

#include "settingswidget.h"
#include "util/sll/qtutil.h"
#include "xmlsettingsdialog/xmlsettingsdialog.h"
#include "interfaces/iinfo.h"
#include "interfaces/ihavesettings.h"
#include "interfaces/ipluginready.h"
#include "interfaces/iplugin2.h"
#include "core.h"

namespace LeechCraft
{
	SettingsWidget::SettingsWidget (QObject *settable,
			const QObjectList& subplugins,
			const MatchesGetter_t& matches,
			QWidget *parent)
	: QWidget { parent }
	, MatchesGetter_ { matches }
	{
		Ui_.setupUi (this);
		Ui_.DialogContents_->setLayout (new QVBoxLayout);

		const auto catsWidth = Ui_.Cats_->minimumSize ().width ();
		Ui_.CatsSplitter_->setSizes ({ catsWidth, catsWidth * 5 });

		FillPages (settable, false);
		for (const auto& sub : subplugins)
			FillPages (sub, true);

		auto w = qobject_cast<IHaveSettings*> (settable)->GetSettingsDialog ()->GetWidget ();
		Ui_.DialogContents_->layout ()->addWidget (w);
		w->show ();

		Ui_.SectionName_->setText (tr ("Settings for %1")
				.arg (qobject_cast<IInfo*> (settable)->GetName ()));
	}

	SettingsWidget::~SettingsWidget ()
	{
		if (Ui_.DialogContents_->layout ()->count ())
		{
			const auto item = Ui_.DialogContents_->layout ()->takeAt (0);
			item->widget ()->setParent (nullptr);
			item->widget ()->hide ();
			delete item;
		}
	}

	void SettingsWidget::Accept ()
	{
		for (const auto& ihs : GetUniqueIHS ())
			ihs->GetSettingsDialog ()->accept ();
	}

	void SettingsWidget::Reject ()
	{
		for (const auto& ihs : GetUniqueIHS ())
			ihs->GetSettingsDialog ()->reject ();
	}

	void SettingsWidget::UpdateSearchHighlights ()
	{
		const auto& matches = MatchesGetter_ ();

		for (const auto& [item, page] : Util::Stlize (Item2Page_))
		{
			const bool enabled = !matches.contains (page.first) ||
					matches [page.first].contains (page.second);
			auto flags = item->flags ();
			if (enabled)
				flags |= Qt::ItemIsEnabled;
			else
				flags &= ~Qt::ItemIsEnabled;
			item->setFlags (flags);
		}
	}

	void SettingsWidget::FillPages (QObject *obj, bool sub)
	{
		IInfo *ii = qobject_cast<IInfo*> (obj);
		IHaveSettings *ihs = qobject_cast<IHaveSettings*> (obj);
		auto sd = ihs->GetSettingsDialog ();

		const QStringList& pages = sd->GetPages ();
		int pgId = 0;
		for (const auto& page : pages)
		{
			QString itemName;
			if (sub)
				itemName = pages.size () == 1 && ii->GetName ().contains (page) ?
						ii->GetName () :
						(ii->GetName () + ": " + page);
			else
				itemName = page;

			auto icon = sd->GetPageIcon (pgId);
			if (icon.isNull ())
				icon = ii->GetIcon ();
			if (icon.isNull ())
				icon = QIcon ("lcicons:/resources/images/defaultpluginicon.svg");

			auto item = new QTreeWidgetItem (QStringList (itemName));
			item->setIcon (0, icon);
			item->setToolTip (0, itemName);
			Ui_.Cats_->addTopLevelItem (item);

			Item2Page_ [item] = qMakePair (ihs, pgId++);
		}

		UpdateSearchHighlights ();
	}

	QSet<IHaveSettings*> SettingsWidget::GetUniqueIHS () const
	{
		QSet<IHaveSettings*> uniques;
		for (const auto& pair : Item2Page_)
			uniques << pair.first;
		return uniques;
	}

	void SettingsWidget::on_Cats__currentItemChanged (QTreeWidgetItem *current)
	{
		const auto& pair = Item2Page_ [current];
		if (!pair.first)
			return;

		auto sd = pair.first->GetSettingsDialog ();
		sd->SetPage (pair.second);

		if (Ui_.DialogContents_->layout ()->count ())
		{
			const auto item = Ui_.DialogContents_->layout ()->takeAt (0);
			item->widget ()->setParent (nullptr);
			item->widget ()->hide ();
			delete item;
		}

		const auto w = sd->GetWidget ();
		Ui_.DialogContents_->layout ()->addWidget (w);
		w->show ();
	}
}
