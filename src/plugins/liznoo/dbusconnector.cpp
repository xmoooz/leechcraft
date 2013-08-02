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

#include "dbusconnector.h"
#include <QTimer>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusConnectionInterface>
#include <QtDebug>
#include <util/util.h>
#include "batteryinfo.h"

namespace LeechCraft
{
namespace Liznoo
{
	DBusConnector::DBusConnector (QObject *parent)
	: QObject (parent)
	, SB_ (QDBusConnection::systemBus ())
	{
		auto iface = SB_.interface ();
		auto checkRunning = [&iface]
			{
				return !iface->registeredServiceNames ()
					.value ().filter ("org.freedesktop.UPower").isEmpty ();
			};
		if (!checkRunning ())
		{
			iface->startService ("org.freedesktop.UPower");
			if (!checkRunning ())
			{
				qWarning () << Q_FUNC_INFO
						<< "failed to autostart UPower, we won't work :(";
				return;
			}
		}

		SB_.connect ("org.freedesktop.UPower",
				"/org/freedesktop/UPower",
				"org.freedesktop.UPower",
				"DeviceAdded",
				this,
				SLOT (requeryDevice (const QString&)));
		SB_.connect ("org.freedesktop.UPower",
				"/org/freedesktop/UPower",
				"org.freedesktop.UPower",
				"DeviceChanged",
				this,
				SLOT (requeryDevice (const QString&)));
		SB_.connect ("org.freedesktop.UPower",
				"/org/freedesktop/UPower",
				"org.freedesktop.UPower",
				"Sleeping",
				this,
				SLOT (handleGonnaSleep ()));
		SB_.connect ("org.freedesktop.UPower",
				"/org/freedesktop/UPower",
				"org.freedesktop.UPower",
				"Resuming",
				this,
				SLOT (handleWokeUp ()));

		QTimer::singleShot (1000,
				this,
				SLOT (enumerateDevices ()));
	}

	void DBusConnector::changeState (PlatformLayer::PowerState state)
	{
		QDBusInterface face ("org.freedesktop.UPower",
				"/org/freedesktop/UPower",
				"org.freedesktop.UPower",
				SB_);

		auto st2meth = [] (PlatformLayer::PowerState state)
		{
			switch (state)
			{
			case PlatformLayer::PowerState::Suspend:
				return "Suspend";
			case PlatformLayer::PowerState::Hibernate:
				return "Hibernate";
			}

			qWarning () << Q_FUNC_INFO
					<< "unknown state";
			return "";
		};

		face.call (QDBus::NoBlock, st2meth (state));
	}

	void DBusConnector::handleGonnaSleep ()
	{
		Entity e = Util::MakeEntity ("Sleeping",
				QString (),
				TaskParameter::Internal,
				"x-leechcraft/power-state-changed");
		e.Additional_ ["TimeLeft"] = 1000;
		emit gotEntity (e);
	}

	void DBusConnector::handleWokeUp ()
	{
		Entity e = Util::MakeEntity ("WokeUp",
				QString (),
				TaskParameter::Internal,
				"x-leechcraft/power-state-changed");
		emit gotEntity (e);
	}

	void DBusConnector::enumerateDevices()
	{
		QDBusInterface face ("org.freedesktop.UPower",
				"/org/freedesktop/UPower",
				"org.freedesktop.UPower",
				SB_);

		auto res = face.call ("EnumerateDevices");
		for (const auto& argument : res.arguments ())
		{
			auto arg = argument.value<QDBusArgument> ();
			QList<QDBusObjectPath> paths;
			arg >> paths;
			for (const auto& path : paths)
				requeryDevice (path.path ());
		}
	}

	namespace
	{
		QString TechIdToString (int id)
		{
			QMap<int, QString> id2str;
			id2str [1] = "Li-Ion";
			id2str [2] = "Li-Polymer";
			id2str [3] = "Li-Iron-Phosphate";
			id2str [4] = "Lead acid";
			id2str [5] = "NiCd";
			id2str [6] = "NiMh";

			return id2str.value (id, "<unknown>");
		}
	}

	void DBusConnector::requeryDevice (const QString& id)
	{
		QDBusInterface face ("org.freedesktop.UPower",
				id,
				"org.freedesktop.UPower.Device",
				SB_);
		if (face.property ("Type").toInt () != 2)
			return;

		BatteryInfo info;
		info.ID_ = id;
		info.Percentage_ = face.property ("Percentage").toInt ();
		info.TimeToFull_ = face.property ("TimeToFull").toLongLong ();
		info.TimeToEmpty_ = face.property ("TimeToEmpty").toLongLong ();
		info.Voltage_ = face.property ("Voltage").toDouble ();
		info.Energy_ = face.property ("Energy").toDouble ();
		info.EnergyFull_ = face.property ("EnergyFull").toDouble ();
		info.DesignEnergyFull_ = face.property ("EnergyFullDesign").toDouble ();
		info.EnergyRate_ = face.property ("EnergyRate").toDouble ();
		info.Technology_ = TechIdToString (face.property ("Technology").toInt ());
		info.Temperature_ = 0;

		emit batteryInfoUpdated (info);
	}
}
}
