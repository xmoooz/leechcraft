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

#include "navigationhistory.h"
#include <QFileInfo>
#include <QMenu>

namespace LeechCraft
{
namespace Monocle
{
	NavigationHistory::NavigationHistory (const EntryGetter_f& getter, QObject *parent)
	: QObject { parent }
	, EntryGetter_ { getter }
	, BackwardMenu_ { new QMenu }
	, ForwardMenu_ { new QMenu }
	{
	}

	QMenu* NavigationHistory::GetBackwardMenu () const
	{
		return BackwardMenu_;
	}

	QMenu* NavigationHistory::GetForwardMenu () const
	{
		return ForwardMenu_;
	}

	void NavigationHistory::GoBack () const
	{
		const auto& actions = BackwardMenu_->actions ();
		if (!actions.isEmpty ())
			actions.front ()->trigger ();
	}

	void NavigationHistory::GoForward () const
	{
		const auto& actions = ForwardMenu_->actions ();
		if (!actions.isEmpty ())
			actions.front ()->trigger ();
	}

	void NavigationHistory::HandleSearchNavigationRequested ()
	{
		AppendHistoryEntry ();
	}

	namespace
	{
		QString GetEntryText (const NavigationHistory::Entry& entry)
		{
			return NavigationHistory::tr ("Page %1 (%2)")
					.arg (entry.Position_.Page_)
					.arg (QFileInfo { entry.Document_ }.fileName ());
		}
	}

	void NavigationHistory::AppendHistoryEntry ()
	{
		const auto& entry = EntryGetter_ ();

		const auto action = new QAction { GetEntryText (entry), this };
		connect (action,
				&QAction::triggered,
				[entry, action, this] { GoTo (action, entry); });

		const auto& backActions = BackwardMenu_->actions ();
		if (backActions.isEmpty ())
		{
			BackwardMenu_->addAction (action);
			emit backwardHistoryAvailabilityChanged (true);
		}
		else
			BackwardMenu_->insertAction (backActions.front (), action);

		if (!ForwardMenu_->actions ().isEmpty ())
		{
			ForwardMenu_->clear ();
			emit forwardHistoryAvailabilityChanged (false);
		}
	}

	void NavigationHistory::GoTo (QAction *action, const Entry& entry)
	{
		emit entryNavigationRequested (entry);

		auto backActions = BackwardMenu_->actions ();
		auto fwdActions = ForwardMenu_->actions ();

		const auto isBack = backActions.contains (action);
		auto& from = isBack ? backActions : fwdActions;
		auto& to = isBack ? fwdActions : backActions;

		auto fromIdx = from.indexOf (action);
		std::copy (from.begin (), from.begin () + fromIdx, std::front_inserter (to));
		from.erase (from.begin (), from.begin () + fromIdx);

		BackwardMenu_->clear ();
		BackwardMenu_->addActions (backActions);
		ForwardMenu_->clear ();
		ForwardMenu_->addActions (fwdActions);

		emit backwardHistoryAvailabilityChanged (!backActions.isEmpty ());
		emit forwardHistoryAvailabilityChanged (!fwdActions.isEmpty ());
	}

	void NavigationHistory::handleDocumentNavigationRequested ()
	{
		AppendHistoryEntry ();
	}
}
}
