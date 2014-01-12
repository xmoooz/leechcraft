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

#include "aggregatorapp.h"
#include <QObject>
#include <QtDebug>
#include <Wt/WText>
#include <Wt/WContainerWidget>
#include <Wt/WBoxLayout>
#include <Wt/WCheckBox>
#include <Wt/WTreeView>
#include <Wt/WTableView>
#include <Wt/WStandardItemModel>
#include <Wt/WStandardItem>
#include <Wt/WOverlayLoadingIndicator>
#include <util/util.h>
#include <interfaces/aggregator/iproxyobject.h>
#include <interfaces/aggregator/channel.h>
#include "readchannelsfilter.h"

namespace LeechCraft
{
namespace Aggregator
{
namespace WebAccess
{
	namespace
	{
		Wt::WString ToW (const QString& str)
		{
			return Wt::WString (str.toUtf8 ().constData (), Wt::CharEncoding::UTF8);
		}
	}

	AggregatorApp::AggregatorApp (IProxyObject *ap, ICoreProxy_ptr cp,
			const Wt::WEnvironment& environment)
	: WApplication (environment)
	, AP_ (ap)
	, CP_ (cp)
	, ChannelsModel_ (new Wt::WStandardItemModel (0, 2, this))
	, ChannelsFilter_ (new ReadChannelsFilter (this))
	, ItemsModel_ (new Wt::WStandardItemModel (0, 2, this))
	{
		ChannelsFilter_->setSourceModel (ChannelsModel_);

		setTitle ("Aggregator WebAccess");
		setLoadingIndicator (new Wt::WOverlayLoadingIndicator ());

		SetupUI ();

		Q_FOREACH (Channel_ptr channel, AP_->GetAllChannels ())
		{
			const auto unreadCount = AP_->CountUnreadItems (channel->ChannelID_);

			auto title = new Wt::WStandardItem (ToW (channel->Title_));
			title->setIcon (Util::GetAsBase64Src (channel->Pixmap_).toStdString ());
			title->setData (channel->ChannelID_, ChannelRole::CID);
			title->setData (channel->FeedID_, ChannelRole::FID);
			title->setData (unreadCount, ChannelRole::UnreadCount);

			auto unread = new Wt::WStandardItem (ToW (QString::number (unreadCount)));
			unread->setData (channel->ChannelID_, ChannelRole::CID);

			ChannelsModel_->appendRow ({ title, unread });
		}
	}

	void AggregatorApp::HandleChannelClicked (const Wt::WModelIndex& idx)
	{
		ItemsModel_->clear ();
		ItemView_->setText (Wt::WString ());

		const IDType_t& cid = boost::any_cast<IDType_t> (idx.data (ChannelRole::CID));
		Q_FOREACH (Item_ptr item, AP_->GetChannelItems (cid))
		{
			if (!item->Unread_)
				continue;

			auto title = new Wt::WStandardItem (ToW (item->Title_));
			title->setData (item->ItemID_, ItemRole::IID);
			title->setData (item->ChannelID_, ItemRole::ParentCh);
			title->setData (item->Unread_, ItemRole::IsUnread);
			title->setData (ToW (item->Link_), ItemRole::Link);
			title->setData (ToW (item->Description_), ItemRole::Text);

			auto date = new Wt::WStandardItem (ToW (item->PubDate_.toString ()));

			ItemsModel_->insertRow (0, { title, date });
		}

		ItemsTable_->setColumnWidth (0, Wt::WLength (500, Wt::WLength::Pixel));
		ItemsTable_->setColumnWidth (1, Wt::WLength (180, Wt::WLength::Pixel));
	}

	void AggregatorApp::HandleItemClicked (const Wt::WModelIndex& idx)
	{
		auto titleIdx = idx.model ()->index (idx.row (), 0);
		auto pubDate = idx.model ()->index (idx.row (), 1);
		auto text = Wt::WString ("<div><a href='{1}' target='_blank'>{2}</a><br />{3}<br /><hr/>{4}</div>")
				.arg (boost::any_cast<Wt::WString> (titleIdx.data (ItemRole::Link)))
				.arg (boost::any_cast<Wt::WString> (titleIdx.data ()))
				.arg (boost::any_cast<Wt::WString> (pubDate.data ()))
				.arg (boost::any_cast<Wt::WString> (titleIdx.data (ItemRole::Text)));
		ItemView_->setText (text);
	}

	void AggregatorApp::SetupUI ()
	{
		auto rootLay = new Wt::WBoxLayout (Wt::WBoxLayout::LeftToRight);
		root ()->setLayout (rootLay);

		auto leftPaneLay = new Wt::WBoxLayout (Wt::WBoxLayout::TopToBottom);
		rootLay->addLayout (leftPaneLay, 2);

		auto showReadChannels = new Wt::WCheckBox (ToW (QObject::tr ("Include read channels")));
		showReadChannels->setToolTip (ToW (QObject::tr ("Also display channels that have no unread items.")));
		showReadChannels->setChecked (false);
		showReadChannels->checked ().connect ([ChannelsFilter_] (Wt::NoClass) { ChannelsFilter_->SetHideRead (false); });
		showReadChannels->unChecked ().connect ([ChannelsFilter_] (Wt::NoClass) { ChannelsFilter_->SetHideRead (true); });
		leftPaneLay->addWidget (showReadChannels);

		auto channelsTree = new Wt::WTreeView ();
		channelsTree->setModel (ChannelsFilter_);
		channelsTree->clicked ().connect (this, &AggregatorApp::HandleChannelClicked);
		channelsTree->setAlternatingRowColors (true);
		leftPaneLay->addWidget (channelsTree, 1, Wt::AlignTop);

		auto rightPaneLay = new Wt::WBoxLayout (Wt::WBoxLayout::TopToBottom);
		rootLay->addLayout (rightPaneLay, 7);

		ItemsTable_ = new Wt::WTableView ();
		ItemsTable_->setModel (ItemsModel_);
		ItemsTable_->clicked ().connect (this, &AggregatorApp::HandleItemClicked);
		ItemsTable_->setAlternatingRowColors (true);
		ItemsTable_->setWidth (Wt::WLength (100, Wt::WLength::Percentage));
		rightPaneLay->addWidget (ItemsTable_, 2, Wt::AlignJustify);

		ItemView_ = new Wt::WText ();
		rightPaneLay->addWidget (ItemView_, 5);
	}
}
}
}
