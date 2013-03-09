/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#pragma once

#include <QPoint>
#include <QMetaType>

namespace LeechCraft
{
namespace Monocle
{
	class Bookmark
	{
		int Page_;
		QPoint Position_;
	public:
		Bookmark ();
		Bookmark (int page, const QPoint& position);

		int GetPage () const;
		void SetPage (int);
		QPoint GetPosition () const;
		void SetPosition (const QPoint& p);
	};

	QDataStream& operator<< (QDataStream&, const Bookmark&);
	QDataStream& operator>> (QDataStream&, Bookmark&);
}
}

Q_DECLARE_METATYPE (LeechCraft::Monocle::Bookmark)
