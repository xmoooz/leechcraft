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

#include <memory>
#include <QtPlugin>

class QUrl;
class QModelIndex;

namespace Media
{
	/** @brief Interface for radios supporting streams adding or removal.
	 *
	 * This interface is to be implemented by an IRadioStation that
	 * supports adding or removal streams by URL. For example, a pseudo
	 * radiostation keeping bookmarks would implement this.
	 *
	 * @sa IRadioStation
	 */
	class Q_DECL_EXPORT IModifiableRadioStation
	{
	public:
		virtual ~IModifiableRadioStation () {}

		/** @brief Adds a new item.
		 *
		 * Adds an item with the given \em url under the given \em name.
		 *
		 * @param[in] url The URL of the stream to add.
		 * @param[in] name The name of the radio station (may be empty).
		 */
		virtual void AddItem (const QUrl& url, const QString& name) = 0;

		/** @brief Removes the previously added item.
		 *
		 * This function removes an item identified by the \em index. The
		 * \em index belongs to the model returned by the
		 * IRadioStationProvider::GetRadioListItems() method of the
		 * parent IRadioStationProvider that returned this radio station.
		 *
		 * @param[in] index The index of the stream to remove.
		 */
		virtual void RemoveItem (const QModelIndex& index) = 0;
	};
}

Q_DECLARE_INTERFACE (Media::IModifiableRadioStation, "org.LeechCraft.Media.IModifiableRadioStation/1.0")
