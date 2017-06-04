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

#include "visualhandler.h"
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/ientitymanager.h>
#include "core.h"

namespace LeechCraft
{
namespace AdvancedNotifications
{
	VisualHandler::VisualHandler ()
	{
	}

	NotificationMethod VisualHandler::GetHandlerMethod () const
	{
		return NMVisual;
	}

	void VisualHandler::Handle (const Entity& orig, const NotificationRule&)
	{
		if (orig.Additional_ ["org.LC.AdvNotifications.EventCategory"].toString () == "org.LC.AdvNotifications.Cancel")
			return;

		const QString& evId = orig.Additional_ ["org.LC.AdvNotifications.EventID"].toString ();
		if (ActiveEvents_.contains (evId))
			return;

		Entity e = orig;

		if (e.Additional_ ["Text"].toString ().isEmpty ())
		{
			if (!e.Additional_ ["org.LC.AdvNotifications.FullText"].toString ().isEmpty ())
				e.Additional_ ["Text"] = e.Additional_ ["org.LC.AdvNotifications.FullText"];
			else if (!e.Additional_ ["org.LC.AdvNotifications.ExtendedText"].toString ().isEmpty ())
				e.Additional_ ["Text"] = e.Additional_ ["org.LC.AdvNotifications.ExtendedText"];
			else
			{
				qWarning () << Q_FUNC_INFO
						<< "refusing to rehandle entity with empty text:"
						<< e.Entity_
						<< e.Additional_;
				return;
			}
		}

		for (auto i = e.Additional_.begin (); i !=  e.Additional_.end (); )
		{
			const auto& key = i.key ();
			if (key.startsWith ("org.LC.AdvNotifications."))
				i = e.Additional_.erase (i);
			else
				++i;
		}

		if (e.Mime_.endsWith ("+advanced"))
			e.Mime_.remove ("+advanced");

		QObject_ptr probeObj (new QObject ());
		ActiveEvents_ << evId;
		probeObj->setProperty ("EventID", evId);
		connect (probeObj.get (),
				SIGNAL (destroyed ()),
				this,
				SLOT (handleProbeDestroyed ()));
		QVariant probe = QVariant::fromValue<QObject_ptr> (probeObj);
		e.Additional_ ["RemovalProbe"] = probe;

		Core::Instance ().GetProxy ()->GetEntityManager ()->HandleEntity (e);
	}

	void VisualHandler::handleProbeDestroyed ()
	{
		const QString& evId = sender ()->property ("EventID").toString ();
		ActiveEvents_.remove (evId);
	}
}
}
