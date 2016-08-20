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

#include "servermanager.h"
#include <cstring>
#include <QStringList>
#include <QDir>
#include <QtDebug>
#include <Wt/WServer>
#include <util/xsd/addressesmodelmanager.h>
#include <util/sys/paths.h>
#include "aggregatorapp.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
namespace WebAccess
{
	namespace
	{
		class ArgcGenerator
		{
			QStringList Parms_;
		public:
			inline ArgcGenerator ()
			: Parms_ ("/usr/local/lib/leechcraft")
			{
			}

			inline void AddParm (const QString& name, const QString& parm)
			{
				Parms_ << name << parm;
			}

			inline int GetArgc () const
			{
				return Parms_.size ();
			}

			inline char** GetArgv () const
			{
				char **result = new char* [GetArgc () + 1];
				int i = 0;
				for (const auto& parm : Parms_)
				{
					result [i] = new char [parm.size () + 1];
					std::strcpy (result [i], parm.toLatin1 ());
					++i;
				}
				result [i] = 0;

				return result;
			}
		};
	}

	ServerManager::ServerManager (IProxyObject *proxy,
			ICoreProxy_ptr coreProxy,
			Util::AddressesModelManager *manager)
	: Server_ { new Wt::WServer }
	, AddrMgr_ { manager }
	{
		Server_->addEntryPoint (Wt::Application,
				[proxy, coreProxy] (const Wt::WEnvironment& we)
					{ return new AggregatorApp (proxy, coreProxy, we); });

		connect (AddrMgr_,
				SIGNAL (addressesChanged ()),
				this,
				SLOT (reconfigureServer ()));
		reconfigureServer ();
	}

	void ServerManager::reconfigureServer ()
	{
		const auto& addresses = AddrMgr_->GetAddresses ();
		qDebug () << Q_FUNC_INFO << "reconfiguring server at" << addresses;

		if (Server_->isRunning ())
		{
			try
			{
				qDebug () << Q_FUNC_INFO << "stopping the server...";
				Server_->stop ();
			}
			catch (const std::exception& e)
			{
				qWarning () << Q_FUNC_INFO
						<< e.what ();
			}
		}

		if (addresses.isEmpty ())
			return;

		ArgcGenerator gen;
		gen.AddParm ("--docroot", "/usr/share/Wt;/favicon.ico,/resources,/style");

		const auto& addr = addresses.value (0);
		gen.AddParm ("--http-address", addr.first);
		gen.AddParm ("--http-port", addr.second);
		Server_->setServerConfiguration (gen.GetArgc (), gen.GetArgv ());

		const auto& logPath = Util::CreateIfNotExists ("aggregator/webaccess")
				.filePath ("wt.log");
		Server_->logger ().setFile (logPath.toStdString ());

		try
		{
			Server_->start ();
		}
		catch (const std::exception& e)
		{
			qWarning () << Q_FUNC_INFO
					<< e.what ();
		}
	}
}
}
}
