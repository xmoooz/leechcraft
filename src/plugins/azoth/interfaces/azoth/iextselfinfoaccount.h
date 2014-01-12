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

#ifndef PLUGINS_AZOTH_INTERFACES_IEXTSELFINFOACCOUNT_H
#define PLUGINS_AZOTH_INTERFACES_IEXTSELFINFOACCOUNT_H
#include <QMetaType>

namespace LeechCraft
{
namespace Azoth
{
	/** @brief Interface for accounts with extended self information.
	 *
	 * Implementing this interface allows other parts of Azoth to know
	 * such information as the avatar of this account, or which contact
	 * represents self-contact, if any.
	 *
	 * @sa IAccount
	 */
	class IExtSelfInfoAccount
	{
	public:
		virtual ~IExtSelfInfoAccount () {}

		/** @brief Returns the self-contact of this account.
		 *
		 * Self-contact represents the user of this account in the
		 * contact list. If there is no concept of self-contact in the
		 * given protocol, this function may return 0.
		 *
		 * Self-contact should also be returned from all the "standard"
		 * functions like IAccount::GetCLEntries() and such. Generally,
		 * it should be a normal ICLEntry-derived object.
		 *
		 * @return ICLEntry-derived object representing self contact.
		 */
		virtual QObject* GetSelfContact () const = 0;

		/** @brief Returns the avatar of this account.
		 *
		 * The returned avatar is typically used to represent the user
		 * of the account in contact list or in chat windows.
		 *
		 * @return The avatar of this account.
		 */
		virtual QImage GetSelfAvatar () const = 0;

		/** @brief Returns the icon of this account.
		 *
		 * The returned icon is used to distinguish this account from
		 * other accounts of the same protocol. For example, an XMPP
		 * account on GMail may choose to return a GMail-y icon.
		 *
		 * Returning a null icon means that general protocol icon will
		 * be used.
		 *
		 * @return The icon of this account.
		 */
		virtual QIcon GetAccountIcon () const = 0;
	};
}
}

Q_DECLARE_INTERFACE (LeechCraft::Azoth::IExtSelfInfoAccount,
		"org.Deviant.LeechCraft.Azoth.IExtSelfInfoAccount/1.0");

#endif
