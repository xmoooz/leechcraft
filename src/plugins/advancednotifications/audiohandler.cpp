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

#include "audiohandler.h"
#include <util/util.h>
#include <util/resourceloader.h>
#include "core.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace AdvancedNotifications
{
	AudioHandler::AudioHandler ()
	{
	}

	NotificationMethod AudioHandler::GetHandlerMethod () const
	{
		return NMAudio;
	}

	void AudioHandler::Handle (const Entity&, const NotificationRule& rule)
	{
		if (!XmlSettingsManager::Instance ()
				.property ("EnableAudioNots").toBool ())
			return;

		QString fname = rule.GetAudioParams ().Filename_;
		if (fname.isEmpty ())
			return;

		if (!fname.contains ('/'))
		{
			const QString& option = XmlSettingsManager::Instance ()
					.property ("AudioTheme").toString ();
			const QString& base = option + '/' + fname;

			QStringList pathVariants;
			pathVariants << base + ".ogg"
					<< base + ".wav"
					<< base + ".flac"
					<< base + ".mp3";

			fname = Core::Instance ().GetAudioThemeLoader ()->GetPath (pathVariants);
		}

		const auto& now = QDateTime::currentDateTime ();
		if (LastNotify_ [fname].msecsTo (now) < 1000)
			return;

		LastNotify_ [fname] = now;

		const Entity& e = Util::MakeEntity (fname, QString (), Internal);
		Core::Instance ().SendEntity (e);
	}
}
}
