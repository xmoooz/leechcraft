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

#include "listmodel.h"
#include <QtDebug>

namespace LeechCraft
{
	namespace Util
	{
		ListModel::ListModel (const QStringList& headers, QObject *parent)
		: QAbstractItemModel (parent)
		, Headers_ (headers)
		{
		}

		ListModel::~ListModel ()
		{
			qDeleteAll (Items_);
		}

		int ListModel::columnCount (const QModelIndex&) const
		{
			return Headers_.size ();
		}

		QVariant ListModel::data (const QModelIndex& index, int role) const
		{
			if (role != RolePointer)
				return Items_ [index.row ()]->Data (index.column (), role);
			else
				return QVariant::fromValue<void*> (Items_ [index.row ()]);
		}

		Qt::ItemFlags ListModel::flags (const QModelIndex&) const
		{
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		}

		QVariant ListModel::headerData (int section, Qt::Orientation orient, int role) const
		{
			if (orient != Qt::Horizontal ||
					role != Qt::DisplayRole)
				return QVariant ();

			return Headers_.at (section);
		}

		QModelIndex ListModel::index (int row, int column, const QModelIndex& parent) const
		{
			if (parent.isValid () ||
					!hasIndex (row, column))
				return QModelIndex ();

			return createIndex (row, column);
		}

		QModelIndex ListModel::parent (const QModelIndex&) const
		{
			return QModelIndex ();
		}

		int ListModel::rowCount (const QModelIndex& index) const
		{
			return index.isValid () ? 0 : Items_.size ();
		}

		void ListModel::Insert (ListModelItem *item, int pos)
		{
			if (pos == -1)
				pos = Items_.size ();

			beginInsertRows (QModelIndex (), pos, pos);
			Items_.insert (pos, item);
			endInsertRows ();
		}

		void ListModel::Remove (ListModelItem *item)
		{
			int pos = Items_.indexOf (item);
			if (pos == -1)
			{
				qWarning () << Q_FUNC_INFO
					<< "not found"
					<< item;
				return;
			}

			beginRemoveRows (QModelIndex (), pos, pos);
			Items_.removeAt (pos);
			endRemoveRows ();
		}

		void ListModel::Remove (int pos)
		{
			beginRemoveRows (QModelIndex (), pos, pos);
			Items_.removeAt (pos);
			endRemoveRows ();
		}

		void ListModel::Update (ListModelItem *item)
		{
			int pos = Items_.indexOf (item);
			if (pos == -1)
			{
				qWarning () << Q_FUNC_INFO
					<< "not found"
					<< item;
				return;
			}

			Update (pos);
		}

		void ListModel::Update (int pos)
		{
			emit dataChanged (index (pos, 0),
					index (pos, columnCount () - 1));
		}
		
		void ListModel::Clear ()
		{
			Items_.clear ();
			reset ();
		}

		void ListModel::SetHeaders (const QStringList& headers)
		{
			Headers_ = headers;
		}

		template<>
			QList<ListModelItem*> ListModel::GetItems () const
			{
				return Items_;
			}
	};
};

