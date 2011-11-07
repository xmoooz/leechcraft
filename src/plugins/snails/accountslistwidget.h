/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2011  Georg Rudoy
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

#ifndef PLUGINS_SNAILS_ACCOUNTSLISTWIDGET_H
#define PLUGINS_SNAILS_ACCOUNTSLISTWIDGET_H
#include <QWidget>
#include "ui_accountslistwidget.h"

namespace LeechCraft
{
namespace Snails
{
	class AccountsListWidget : public QWidget
	{
		Q_OBJECT

		Ui::AccountsListWidget Ui_;
	public:
		AccountsListWidget (QWidget* = 0);

		void SetAccountsModel (QAbstractItemModel*);
	private slots:
		void on_AddButton__released ();
		void on_ModifyButton__released ();
		void on_RemoveButton__released ();
	};
}
}

#endif
