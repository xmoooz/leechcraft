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

#pragma once

#include <QX11Info>
#include <QList>
#include <QString>
#include <QHash>
#include <X11/X.h>

class QIcon;

namespace LeechCraft
{
namespace Krigstask
{
	class XWrapper : public QObject
	{
		Q_OBJECT

		Display *Display_;
		Window AppWin_;

		QHash<QString, Atom> Atoms_;

		XWrapper ();
	public:
		enum WinStateFlag
		{
			NoFlag = 0x0,
			Modal = 0x001,
			Sticky = 0x002,
			MaximizedVert = 0x004,
			MaximizedHorz = 0x008,
			Shaded = 0x010,
			SkipTaskbar = 0x020,
			SkipPager = 0x040,
			Hidden = 0x080,
			Fullscreen = 0x100,
			OnTop = 0x200,
			OnBottom = 0x400,
			Attention = 0x800
		};
		Q_DECLARE_FLAGS (WinStateFlags, WinStateFlag)

		static XWrapper& Instance ();

		bool Filter (void*);

		QList<Window> GetWindows ();
		QString GetWindowTitle (Window);
		QIcon GetWindowIcon (Window);

		WinStateFlags GetWindowState (Window);

		Window GetActiveApp ();

		bool ShouldShow (Window);
	private:
		template<typename T>
		void HandlePropNotify (T);

		Window GetActiveWindow ();

		Atom GetAtom (const QString&);
		bool GetWinProp (Window, Atom, ulong*, uchar**, Atom = static_cast<Atom> (AnyPropertyType)) const;
		bool GetRootWinProp (Atom, ulong*, uchar**, Atom = static_cast<Atom> (AnyPropertyType)) const;
		QList<Atom> GetWindowType (Window);
	signals:
		void windowListChanged ();
		void activeWindowChanged ();
		void desktopChanged ();
	};
}
}

Q_DECLARE_OPERATORS_FOR_FLAGS (LeechCraft::Krigstask::XWrapper::WinStateFlags)
