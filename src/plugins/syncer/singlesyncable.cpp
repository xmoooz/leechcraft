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

#include "singlesyncable.h"
#include <sstream>
#include <boost/archive/text_oarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/variant.hpp>
#include <QTimer>
#include <QTcpSocket>
#include <QSettings>
#include <QCoreApplication>
#include <laretz/ops/item.h>
#include <laretz/ops/operation.h>
#include <interfaces/isyncable.h>

namespace LeechCraft
{
namespace Syncer
{
	SingleSyncable::SingleSyncable (const QByteArray& id, ISyncProxy *proxy, QObject *parent)
	: QObject (parent)
	, ID_ (id)
	, Proxy_ (proxy)
	, Socket_ (new QTcpSocket (this))
	{
		connect (Socket_,
				SIGNAL (connected ()),
				this,
				SLOT (handleSocketConnected ()));

		QTimer::singleShot (1000,
				this,
				SLOT (startSync ()));
	}

	std::shared_ptr<QSettings> SingleSyncable::GetSettings ()
	{
		std::shared_ptr<QSettings> settings (new QSettings (QCoreApplication::organizationName (),
					QCoreApplication::applicationName () + "_Syncer_State"),
				[] (QSettings *settings) -> void
				{
					settings->endGroup ();
					delete settings;
				});
		settings->beginGroup (ID_);
		return settings;
	}

	void SingleSyncable::startSync ()
	{
		if (Socket_->isValid ())
			return;

		Socket_->connectToHost ("127.0.0.1", 54093);
	}

	void SingleSyncable::handleSocketConnected ()
	{
		QByteArray data;

		QDataStream out (&data, QIODevice::WriteOnly);
		out << "Login: d34df00d\n";
		out << "Password: shitfuck\n";
		out << "\n";

		std::vector<Laretz::Operation> ops;
		ops.push_back ({ Laretz::OpType::List });

		std::ostringstream ostr;
		boost::archive::text_oarchive oars (ostr);
		oars << ops;

		out << ostr.str ().c_str ();

		Socket_->write (data);
	}
}
}
