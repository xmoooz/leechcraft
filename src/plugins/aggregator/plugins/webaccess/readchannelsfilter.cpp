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

#include "readchannelsfilter.h"
#include <QtDebug>
#include "aggregatorapp.h"

namespace LeechCraft
{
namespace Aggregator
{
namespace WebAccess
{
	ReadChannelsFilter::ReadChannelsFilter (Wt::WObject *parent)
	: WSortFilterProxyModel (parent)
	{
		setDynamicSortFilter (true);
	}

	void ReadChannelsFilter::SetHideRead (bool hide)
	{
		HideRead_ = hide;
		sort (0);
	}

	bool ReadChannelsFilter::filterAcceptRow (int row, const Wt::WModelIndex& parent) const
	{
		if (HideRead_)
		{
			auto idx = sourceModel ()->index (row, 0, parent);
			if (idx.isValid ())
			{
				const auto data = idx.data (AggregatorApp::ChannelRole::UnreadCount);
				try
				{
					return boost::any_cast<int> (data) > 0;
				}
				catch (const std::exception& e)
				{
					qWarning () << Q_FUNC_INFO
							<< "cannot get unread count"
							<< e.what ()
							<< "; stored:"
							<< data.type ().name ();
					return true;
				}
			}
		}
		return Wt::WSortFilterProxyModel::filterAcceptRow (row, parent);
	}
}
}
}
