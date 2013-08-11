/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#pragma once

#include <memory>
#include <QObject>
#include <QList>
#include "structures.h"

namespace LeechCraft
{
namespace Poleemery
{
	struct StorageImpl;

	class Storage : public QObject
	{
		std::shared_ptr<StorageImpl> Impl_;
	public:
		Storage (QObject* = 0);

		Storage (const Storage&) = delete;
		Storage (Storage&&) = delete;
		Storage& operator= (const Storage&) = delete;
		Storage& operator= (Storage&&) = delete;

		QList<Account> GetAccounts () const;
		void AddAccount (Account&);
		void UpdateAccount (const Account&);
		void DeleteAccount (const Account&);

		QList<ExpenseEntry> GetExpenseEntries ();
		QList<ExpenseEntry> GetExpenseEntries (const Account&);
		void AddExpenseEntry (ExpenseEntry&);
		void UpdateExpenseEntry (const ExpenseEntry&);
		void DeleteExpenseEntry (const ExpenseEntry&);

		QList<ReceiptEntry> GetReceiptEntries ();
		QList<ReceiptEntry> GetReceiptEntries (const Account&);
		void AddReceiptEntry (ReceiptEntry&);
		void UpdateReceiptEntry (const ReceiptEntry&);
		void DeleteReceiptEntry (const ReceiptEntry&);

		QList<Rate> GetRates ();
		QList<Rate> GetRates (const QDateTime& start, const QDateTime& end);
		QList<Rate> GetRate (const QString&);
		QList<Rate> GetRate (const QString&, const QDateTime& start, const QDateTime& end);
		void AddRate (Rate&);
	private:
		Category AddCategory (const QString&);
		void AddNewCategories (const ExpenseEntry&, const QStringList&);
		void LinkEntry2Cat (const ExpenseEntry&, const Category&);
		void UnlinkEntry2Cat (const ExpenseEntry&, const Category&);

		QList<ExpenseEntry> HandleNaked (const QList<NakedExpenseEntry>&);

		void InitializeTables ();
		void LoadCategories ();
	};
}
}
