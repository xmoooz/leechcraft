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

#include <QtPlugin>
#include <QString>
#include <QList>
#include <QIcon>

/** @brief Base interface for data filter plugins.
 *
 * Data filter plugins provide some means to manipulate and alter data.
 * Examples of such plugins are image uploaders to various image bins,
 * text finders, etc.
 *
 * A single data filter plugin can support multiple data filter
 * variants, like particular image bins supported by an image uploader
 * or particular OpenSearch engines supported by an OpenSearch handler.
 * The list of possible data filter variants is returned from the
 * GetFilterVariants() function. The list can be dynamic (that is,
 * different in different calls).
 *
 * Plugins implementing this interface are also expected to implement
 * IEntityHandler, considering (and accepting) entities with MIME
 * "x-leechcraft/data-filter-request". Such entities will contain the
 * entity to filter (like, a piece of text or an image) in the
 * Entity::Entity_ field and may contain the "DataFilter" key in the
 * Entity::Additional_ map with the ID of the exact filter variant
 * to use (if user has already selected it).
 *
 * @sa Entity, IEntityHandler
 */
class Q_DECL_EXPORT IDataFilter
{
public:
	/** @brief Describes a single filter variant supported by this data
	 * filter.
	 */
	struct FilterVariant
	{
		/** @brief The ID of this filter variant.
		 *
		 * This ID will be passed in the Additional_ ["DataFilter"]
		 * field in the Entity structure.
		 */
		QByteArray ID_;

		/** @brief The human-readable name of the filter variant.
		 */
		QString Name_;

		/** @brief The description of the filter variant.
		 */
		QString Description_;

		/** @brief The icon representing the filter variant.
		 */
		QIcon Icon_;
	};

	virtual ~IDataFilter () {}

	/** @brief Returns the string describing the data filter.
	 *
	 * This function should return the human-readable string describing
	 * the data filter plugin in general in an imperative form, like
	 * "Upload image to" for an image uploader or "Search in" for a text
	 * searcher.
	 *
	 * @return The human-readable string like "Search in".
	 */
	virtual QString GetFilterVerb () const = 0;

	/** @brief Returns the list of concrete data filter variants.
	 *
	 * @return The list of exact data filter variants.
	 */
	virtual QList<FilterVariant> GetFilterVariants () const = 0;
};

Q_DECLARE_INTERFACE (IDataFilter, "org.Deviant.LeechCraft.IDataFilter/1.0");
