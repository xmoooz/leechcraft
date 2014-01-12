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

#include <stdexcept>
#include <algorithm>
#include <QtDebug>
#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QIcon>
#include <interfaces/structures.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/itagsmanager.h>
#include "channelsmodel.h"
#include "item.h"
#include "xmlsettingsmanager.h"
#include "core.h"

namespace LeechCraft
{
namespace Aggregator
{
	ChannelsModel::ChannelsModel (QObject *parent)
	: QAbstractItemModel (parent)
	, Toolbar_ (0)
	, TabWidget_ (0)
	, Menu_ (0)
	{
		setObjectName ("Aggregator ChannelsModel");
		Headers_ << tr ("Feed")
			<< tr ("Unread items")
			<< tr ("Last build");
	}

	ChannelsModel::~ChannelsModel ()
	{
	}

	void ChannelsModel::SetWidgets (QToolBar *bar, QWidget *tab)
	{
		Toolbar_ = bar;
		TabWidget_ = tab;
	}

	int ChannelsModel::columnCount (const QModelIndex&) const
	{
		return Headers_.size ();
	}

	QVariant ChannelsModel::data (const QModelIndex& index, int role) const
	{
		if (role == LeechCraft::RoleControls)
			return QVariant::fromValue<QToolBar*> (Toolbar_);
		if (role == LeechCraft::RoleAdditionalInfo)
			return QVariant::fromValue<QWidget*> (TabWidget_);
		if (role == LeechCraft::RoleContextMenu)
			return QVariant::fromValue<QMenu*> (Menu_);

		if (!index.isValid ())
			return QVariant ();

		int row = index.row ();
		if (role == Qt::DisplayRole)
			switch (index.column ())
			{
				case ColumnTitle:
					return Channels_.at (row).Title_;
				case ColumnUnread:
					return Channels_.at (row).Unread_;
				case ColumnLastBuild:
					return Channels_.at (row).LastBuild_;
				default:
					return QVariant ();
			}
		else if (role == Qt::DecorationRole &&
				index.column () == 0)
		{
			QIcon result = QPixmap::fromImage (Channels_.at (row).Favicon_);
			if (result.isNull ())
				result = QIcon (":/resources/images/rss.png");
			return result;
		}
		//Color mark a channels as read/unread
		else if (role == Qt::ForegroundRole)
		{
			bool palette = XmlSettingsManager::Instance ()->
					property ("UsePaletteColors").toBool ();
			if (Channels_.at (row).Unread_)
			{
				if (XmlSettingsManager::Instance ()->
						property ("UnreadCustomColor").toBool ())
					return XmlSettingsManager::Instance ()->
							property ("UnreadItemsColor").value<QColor> ();
				else
					return palette ?
						QApplication::palette ().link ().color () :
						QVariant ();
			}
			else
				return palette ?
					QApplication::palette ().linkVisited ().color () :
					QVariant ();
		}
		else if (role == Qt::FontRole)
		{
			if (Channels_.at (row).Unread_)
				return XmlSettingsManager::Instance ()->
						property ("UnreadItemsFont");
			else
				return QVariant ();
		}
		else if (role == Qt::ToolTipRole)
		{
			const ChannelShort& cs = Channels_.at (row);
			QString result = QString ("<qt><b>%1</b><br />").arg (cs.Title_);
			if (cs.Author_.size ())
			{
				result += tr ("<strong>Author</strong>: %1").arg (cs.Author_);
				result += "<br />";
			}
			if (cs.Tags_.size ())
			{
				QStringList hrTags;
				Q_FOREACH (QString id, cs.Tags_)
					hrTags << Core::Instance ().GetProxy ()->
						GetTagsManager ()->GetTag (id);
				result += tr ("<b>Tags</b>: %1").arg (hrTags.join ("; "));
				result += "<br />";
			}
			QString elidedLink = QApplication::fontMetrics ()
					.elidedText (cs.Link_, Qt::ElideMiddle, 400);
			result += QString ("<a href='%1'>%2</a>")
					.arg (cs.Link_)
					.arg (elidedLink);
			result += "</qt>";
			return result;
		}
		else if (role == LeechCraft::RoleTags)
			return Channels_.at (row).Tags_;
		else
			return QVariant ();
	}

	Qt::ItemFlags ChannelsModel::flags (const QModelIndex& index) const
	{
		if (!index.isValid ())
			return 0;
		else
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	}

	QVariant ChannelsModel::headerData (int column, Qt::Orientation orient, int role) const
	{
		if (orient == Qt::Horizontal && role == Qt::DisplayRole)
			return Headers_.at (column);
		else
			return QVariant ();
	}

	QModelIndex ChannelsModel::index (int row, int column, const QModelIndex& parent) const
	{
		if (!hasIndex (row, column, parent))
			return QModelIndex ();

		return createIndex (row, column);
	}

	QModelIndex ChannelsModel::parent (const QModelIndex&) const
	{
		return QModelIndex ();
	}

	int ChannelsModel::rowCount (const QModelIndex& parent) const
	{
		return parent.isValid () ? 0 : Channels_.size ();
	}

	void ChannelsModel::AddChannel (const ChannelShort& channel)
	{
		beginInsertRows (QModelIndex (), rowCount (), rowCount ());
		Channels_ << channel;
		endInsertRows ();
	}

	void ChannelsModel::Update (const channels_container_t& channels)
	{
		for (size_t i = 0; i < channels.size (); ++i)
		{
			Channels_t::const_iterator pos =
				std::find (Channels_.begin (), Channels_.end (),
					channels.at (i));
			if (pos != Channels_.end ())
				continue;

			Channels_ << channels [i]->ToShort ();
		}
	}

	void ChannelsModel::UpdateChannelData (const ChannelShort& cs)
	{
		auto idx = std::find (Channels_.begin (), Channels_.end (), cs);
		if (idx == Channels_.end ())
			return;
		
		*idx = cs;
		int pos = std::distance (Channels_.begin (), idx);
		emit dataChanged (index (pos, 0), index (pos, 2));
		emit channelDataUpdated (cs.ChannelID_, cs.FeedID_);
	}

	ChannelShort& ChannelsModel::GetChannelForIndex (const QModelIndex& index)
	{
		if (!index.isValid ())
			throw std::runtime_error ("Invalid index");
		else
			return Channels_ [index.row ()];
	}

	void ChannelsModel::RemoveChannel (const ChannelShort& channel)
	{
		const Channels_t::iterator idx =
			std::find (Channels_.begin (), Channels_.end (), channel);
		if (idx == Channels_.end ())
			return;

		const int pos = std::distance (Channels_.begin (), idx);
		beginRemoveRows (QModelIndex (), pos, pos);
		Channels_.erase (idx);
		endRemoveRows ();
	}

	void ChannelsModel::Clear ()
	{
		beginResetModel ();
		Channels_.clear ();
		endResetModel ();
	}

	QModelIndex ChannelsModel::GetUnreadChannelIndex () const
	{
		for (int i = 0; i < Channels_.size (); ++i)
			if (Channels_.at (i).Unread_)
				return index (i, 0);
		return QModelIndex ();
	}

	int ChannelsModel::GetUnreadChannelsNumber () const
	{
		int result = 0;
		for (int i = 0; i < Channels_.size (); ++i)
			if (Channels_.at (i).Unread_)
				++result;
		return result;
	}

	int ChannelsModel::GetUnreadItemsNumber () const
	{
		int result = 0;
		for (int i = 0; i < Channels_.size (); ++i)
			result += Channels_.at (i).Unread_;
		return result;
	}

	void ChannelsModel::SetMenu (QMenu *menu)
	{
		Menu_ = menu;
	}
}
}
