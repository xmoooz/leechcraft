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

#include "mrimprotocol.h"
#include <QIcon>
#include <QSettings>
#include <QtDebug>
#include "mrimaccount.h"
#include "mrimaccountconfigwidget.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Vader
{
	MRIMProtocol::MRIMProtocol (IProxyObject *azothProxy,
			const ICoreProxy_ptr& coreProxy, QObject *parent)
	: QObject { parent }
	, AzothProxy_ { azothProxy }
	, CoreProxy_ { coreProxy }
	{
		RestoreAccounts ();
	}

	MRIMProtocol::~MRIMProtocol ()
	{
		for (auto acc : Accounts_)
			emit accountRemoved (acc);
	}

	IProxyObject* MRIMProtocol::GetAzothProxy () const
	{
		return AzothProxy_;
	}

	const ICoreProxy_ptr& MRIMProtocol::GetCoreProxy () const
	{
		return CoreProxy_;
	}

	QObject* MRIMProtocol::GetQObject ()
	{
		return this;
	}

	IProtocol::ProtocolFeatures MRIMProtocol::GetFeatures () const
	{
		return PFNone;
	}

	QList<QObject*> MRIMProtocol::GetRegisteredAccounts ()
	{
		QList<QObject*> result;
		Q_FOREACH (auto acc, Accounts_)
			result << acc;
		return result;
	}

	QObject* MRIMProtocol::GetParentProtocolPlugin () const
	{
		return parent ();
	}

	QString MRIMProtocol::GetProtocolName () const
	{
		return "Mail.ru Agent";
	}

	QIcon MRIMProtocol::GetProtocolIcon () const
	{
		static QIcon icon ("lcicons:/plugins/azoth/plugins/vader/resources/images/mrim.svg");
		return icon;
	}

	QByteArray MRIMProtocol::GetProtocolID () const
	{
		return "org.LC.MRIM";
	}

	QList<QWidget*> MRIMProtocol::GetAccountRegistrationWidgets (IProtocol::AccountAddOptions)
	{
		return { new MRIMAccountConfigWidget };
	}

	void MRIMProtocol::RegisterAccount (const QString& name, const QList<QWidget*>& widgets)
	{
		auto w = qobject_cast<MRIMAccountConfigWidget*> (widgets.value (0));
		if (!w)
		{
			qWarning () << Q_FUNC_INFO
					<< "first widget is invalid"
					<< widgets;
			return;
		}

		MRIMAccount *acc = new MRIMAccount (name, this);
		acc->FillConfig (w);
		Accounts_ << acc;

		emit accountAdded (acc);

		saveAccounts ();
	}

	void MRIMProtocol::RemoveAccount (QObject *acc)
	{
		if (Accounts_.removeAll (qobject_cast<MRIMAccount*> (acc)))
		{
			emit accountRemoved (acc);
			saveAccounts ();
			acc->deleteLater ();
		}
	}

	void MRIMProtocol::RestoreAccounts ()
	{
		QSettings settings (QSettings::IniFormat, QSettings::UserScope,
				QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Azoth_Vader_Accounts");
		int size = settings.beginReadArray ("Accounts");
		for (int i = 0; i < size; ++i)
		{
			settings.setArrayIndex (i);
			const QByteArray& data = settings.value ("SerializedData").toByteArray ();
			MRIMAccount *acc = MRIMAccount::Deserialize (data, this);
			if (!acc)
			{
				qWarning () << Q_FUNC_INFO
						<< "undeserializable acount"
						<< i;
				continue;
			}

			connect (acc,
					SIGNAL (accountSettingsChanged ()),
					this,
					SLOT (saveAccounts ()));

			Accounts_ << acc;

			emit accountAdded (acc);
		}
		settings.endArray ();
	}

	void MRIMProtocol::saveAccounts ()
	{
		QSettings settings (QSettings::IniFormat, QSettings::UserScope,
				QCoreApplication::organizationName (),
				QCoreApplication::applicationName () + "_Azoth_Vader_Accounts");
		settings.beginWriteArray ("Accounts");
		for (int i = 0, size = Accounts_.size ();
				i < size; ++i)
		{
			settings.setArrayIndex (i);
			settings.setValue ("SerializedData", Accounts_.at (i)->Serialize ());
		}
		settings.endArray ();
		settings.sync ();
	}
}
}
}
