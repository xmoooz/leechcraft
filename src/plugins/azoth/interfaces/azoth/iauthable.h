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

#ifndef PLUGINS_AZOTH_INTERFACES_IAUTHABLE_H
#define PLUGINS_AZOTH_INTERFACES_IAUTHABLE_H
#include <QString>
#include "azothcommon.h"

namespace LeechCraft
{
namespace Azoth
{
	/** @brief Represents an entry that supports authorizations.
	 */
	class IAuthable
	{
	public:
		virtual ~IAuthable () {}

		/** @brief Returns the AuthStatus between our user and this
		 * remote.
		 *
		 * @return Authorization status of this entry.
		 */
		virtual AuthStatus GetAuthStatus () const = 0;

		/** @brief Resends authorization to the entry.
		 *
		 * @param[in] reason Optional reason message, if applicable.
		 */
		virtual void ResendAuth (const QString& reason = QString ()) = 0;

		/** @brief Revokes authorization from the entry.
		 *
		 * @param[in] reason Optional reason message, if applicable.
		 */
		virtual void RevokeAuth (const QString& reason = QString ()) = 0;

		/** @brief Unsubscribes ourselves from the contact.
		 *
		 * @param[in] reason Optional reason message, if applicable.
		 */
		virtual void Unsubscribe (const QString& reason = QString ()) = 0;

		/** @brief Rerequest authorization.
		 *
		 * @param[in] reason Optional reason message, if applicable.
		 */
		virtual void RerequestAuth (const QString& reason = QString ()) = 0;
	};
}
}

Q_DECLARE_INTERFACE (LeechCraft::Azoth::IAuthable,
		"org.Deviant.LeechCraft.Azoth.IAuthable/1.0");

#endif
