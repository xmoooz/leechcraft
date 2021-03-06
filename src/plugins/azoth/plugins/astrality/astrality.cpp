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

#include "astrality.h"
#include <QIcon>
#include <QtDebug>
#include <Types>
#include <Debug>
#include <ConnectionManager>
#include <PendingStringList>
#include <util/util.h>
#include <interfaces/azoth/iprotocol.h>
#include <interfaces/azoth/iaccount.h>
#include "cmwrapper.h"
#include "accountwrapper.h"
#include "protowrapper.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Astrality
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Proxy_ = proxy;

		Util::InstallTranslator ("azoth_astrality");
		Tp::registerTypes ();
		Tp::enableDebug (false);
		Tp::enableWarnings (false);
	}

	void Plugin::SecondInit ()
	{
		connect (Tp::ConnectionManager::listNames (),
				SIGNAL (finished (Tp::PendingOperation*)),
				this,
				SLOT (handleListNames (Tp::PendingOperation*)));
	}

	void Plugin::Release ()
	{
		Q_FOREACH (CMWrapper *cmWrapper, Wrappers_)
			Q_FOREACH (QObject *protocol, cmWrapper->GetProtocols ())
				qobject_cast<ProtoWrapper*> (protocol)->Release ();

		qDeleteAll (Wrappers_);
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.Azoth.Astrality";
	}

	QString Plugin::GetName () const
	{
		return "Azoth Astrality";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Support for protocols provided by Telepathy.");
	}

	QIcon Plugin::GetIcon () const
	{
		static QIcon icon ("lcicons:/azoth/astrality/resources/images/astrality.svg");
		return icon;
	}

	QSet<QByteArray> Plugin::GetPluginClasses () const
	{
		QSet<QByteArray> classes;
		classes << "org.LeechCraft.Plugins.Azoth.Plugins.IProtocolPlugin";
		return classes;
	}

	QObject* Plugin::GetQObject ()
	{
		return this;
	}

	QList<QObject*> Plugin::GetProtocols () const
	{
		QList<QObject*> result;
		Q_FOREACH (auto cmw, Wrappers_)
			result << cmw->GetProtocols ();
		return result;
	}

	void Plugin::initPlugin (QObject *proxy)
	{
	}

	void Plugin::handleListNames (Tp::PendingOperation *op)
	{
		auto psl = qobject_cast<Tp::PendingStringList*> (op);
		qDebug () << Q_FUNC_INFO << psl->result ();

		Q_FOREACH (const QString& cmName, psl->result ())
		{
			auto cmw = new CMWrapper (cmName, Proxy_, this);
			Wrappers_ << cmw;

			connect (cmw,
					SIGNAL (gotProtoWrappers (QList<QObject*>)),
					this,
					SIGNAL (gotNewProtocols (QList<QObject*>)));
			connect (cmw,
					SIGNAL (gotProtoWrappers (QList<QObject*>)),
					this,
					SLOT (handleProtoWrappers (QList<QObject*>)));
		}
	}

	void Plugin::handleProtoWrappers (const QList<QObject*>& wrappers)
	{
		Q_FOREACH (QObject *obj, wrappers)
		{
			connect (obj,
					SIGNAL (gotEntity (LeechCraft::Entity)),
					this,
					SIGNAL (gotEntity (LeechCraft::Entity)));
		}
	}
}
}
}

LC_EXPORT_PLUGIN (leechcraft_azoth_astrality, LeechCraft::Azoth::Astrality::Plugin);
