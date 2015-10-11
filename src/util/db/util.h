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

#pragma once

#include <QVariant>
#include <QSqlQuery>
#include <QtDebug>
#include "dbconfig.h"

class QSqlDatabase;
class QString;

namespace LeechCraft
{
namespace Util
{
	/** @brief Runs the given query \em text on the given \em db.
	 *
	 * Prepares and executes a QSqlQuery containing the given query
	 * \em text on the given \em db. If the query fails, an exception
	 * is thrown.
	 *
	 * @param[in] db The database to execute the query \em text on.
	 * @param[in] text The text of the query to be executed.
	 * @throws std::exception If the query execution failed.
	 */
	UTIL_DB_API void RunTextQuery (const QSqlDatabase& db, const QString& text);

	/** @brief Loads the query text from the given resource file.
	 *
	 * Loads the query text from the resources for the given \em plugin
	 * and exact \em filename in it.
	 *
	 * The file name to be loaded is formed as
	 * <code>:/${plugin}/resources/sql/${filename}.sql</code>.
	 *
	 * @param[in] plugin The name of the plugin whose resources should be
	 * used.
	 * @param[in] filename The name of the file under
	 * <code>${plugin}/resources/sql</code>.
	 * @return The query text in the given file.
	 * @throws std::exception If the given file cannot be opened.
	 *
	 * @sa RunQuery()
	 */
	UTIL_DB_API QString LoadQuery (const QString& plugin, const QString& filename);

	/** @brief Loads the query from the given resource file and runs it.
	 *
	 * Loads the query text from the resources for the given \em plugin
	 * and exact \em filename in it and executes it on the given \em db.
	 *
	 * The file name to be loaded is formed as
	 * <code>:/${plugin}/resources/sql/${filename}.sql</code>.
	 *
	 * @param[in] db The database to execute the query on.
	 * @param[in] plugin The name of the plugin whose resources should be
	 * used.
	 * @param[in] filename The name of the file under
	 * <code>${plugin}/resources/sql</code>.
	 * @throws std::exception If the given file cannot be opened or if the
	 * query execution failed.
	 *
	 * @sa LoadQuery()
	 */
	UTIL_DB_API void RunQuery (const QSqlDatabase& db, const QString& plugin, const QString& filename);

	template<typename T = int>
	T GetLastId (const QSqlQuery& query)
	{
		const auto& lastVar = query.lastInsertId ();
		if (lastVar.isNull ())
			throw std::runtime_error { "No last ID has been reported." };

		if (!lastVar.canConvert<T> ())
		{
			qWarning () << Q_FUNC_INFO
					<< "cannot convert"
					<< lastVar;
			throw std::runtime_error { "Cannot convert last ID." };
		}

		return lastVar.value<T> ();
	}

	/** @brief Generates an unique thread-safe connection name.
	 *
	 * This function generates a connection name using the given \em base
	 * that is unique across all threads.
	 *
	 * @param[in] base The identifier base to be used to generate the
	 * unique connection string.
	 * @return An unique connection name across all threads.
	 */
	UTIL_DB_API QString GenConnectionName (const QString& base);
}
}
