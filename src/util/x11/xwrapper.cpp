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

#include "xwrapper.h"
#include <limits>
#include <type_traits>
#include <QString>
#include <QPixmap>
#include <QIcon>
#include <QApplication>
#include <QWidget>
#include <QDesktopWidget>
#include <QAbstractEventDispatcher>
#include <QtDebug>
#include <QTimer>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

typedef bool (*QX11FilterFunction) (XEvent *event);

extern void qt_installX11EventFilter (QX11FilterFunction func);

namespace LeechCraft
{
namespace Util
{
	const int SourceNormal = 1;
	const int SourcePager = 2;

	const int StateRemove = 0;
	const int StateAdd = 1;
	const int StateToggle = 1;

	namespace
	{
		bool EventFilter (XEvent *msg)
		{
			return XWrapper::Instance ().Filter (msg);
		}
	}

	XWrapper::XWrapper ()
	: Display_ (QX11Info::display ())
	, AppWin_ (QX11Info::appRootWindow ())
	{
		qt_installX11EventFilter (&EventFilter);
		XSelectInput (Display_,
				AppWin_,
				PropertyChangeMask | StructureNotifyMask | SubstructureNotifyMask);
	}

	XWrapper& XWrapper::Instance ()
	{
		static XWrapper w;
		return w;
	}

	Display* XWrapper::GetDisplay () const
	{
		return Display_;
	}

	Window XWrapper::GetRootWindow () const
	{
		return AppWin_;
	}

	bool XWrapper::Filter (XEvent *ev)
	{
		if (ev->type == PropertyNotify)
			HandlePropNotify (&ev->xproperty);

		return false;
	}

	namespace
	{
		template<typename T>
		struct IsDoublePtr : std::false_type {};

		template<typename T>
		struct IsDoublePtr<T**> : std::true_type {};

		template<typename T>
		class Guarded
		{
			T *Data_;
		public:
			Guarded ()
			: Data_ { nullptr }
			{
			}

			~Guarded ()
			{
				if (Data_)
					XFree (Data_);
			}

			T** Get (bool clear = true)
			{
				if (clear && Data_)
					XFree (Data_);
				return &Data_;
			}

			template<typename U>
			U GetAs (bool clear = true)
			{
				if (clear && Data_)
					XFree (Data_);
				return IsDoublePtr<U>::value ?
						reinterpret_cast<U> (&Data_) :
						reinterpret_cast<U> (Data_);
			}

			T operator[] (size_t idx) const
			{
				return Data_ [idx];
			}

			T& operator[] (size_t idx)
			{
				return Data_ [idx];
			}

			operator bool () const
			{
				return Data_ != nullptr;
			}

			bool operator! () const
			{
				return !Data_;
			}
		};
	}

	QList<Window> XWrapper::GetWindows ()
	{
		ulong length = 0;
		Guarded<Window> data;

		QList<Window> result;
		if (GetRootWinProp (GetAtom ("_NET_CLIENT_LIST"), &length, data.GetAs<uchar**> ()))
			for (ulong i = 0; i < length; ++i)
				result << data [i];
		return result;
	}

	QString XWrapper::GetWindowTitle (Window wid)
	{
		QString name;

		ulong length = 0;
		Guarded<uchar> data;

		auto utf8Str = GetAtom ("UTF8_STRING");

		if (GetWinProp (wid, GetAtom ("_NET_WM_VISIBLE_NAME"), &length, data.Get (), utf8Str))
			name = QString::fromUtf8 (data.GetAs<char*> (false));

		if (name.isEmpty ())
			if (GetWinProp (wid, GetAtom ("_NET_WM_NAME"), &length, data.Get (), utf8Str))
				name = QString::fromUtf8 (data.GetAs<char*> (false));

		if (name.isEmpty ())
			if (GetWinProp (wid, GetAtom ("XA_WM_NAME"), &length, data.Get (), XA_STRING))
				name = QString::fromUtf8 (data.GetAs<char*> (false));

		if (name.isEmpty ())
		{
			XFetchName (Display_, wid, data.GetAs<char**> ());
			name = QString (data.GetAs<char*> (false));
		}

		if (name.isEmpty ())
		{
			XTextProperty prop;
			if (XGetWMName (Display_, wid, &prop))
			{
				name = QString::fromUtf8 (reinterpret_cast<char*> (prop.value));
				XFree (prop.value);
			}
		}

		return name;
	}

	QIcon XWrapper::GetWindowIcon (Window wid)
	{
		int fmt = 0;
		ulong type, count, extra;
		Guarded<ulong> data;

		XGetWindowProperty (Display_, wid, GetAtom ("_NET_WM_ICON"),
				0, std::numeric_limits<long>::max (), False, AnyPropertyType,
				&type, &fmt, &count, &extra,
				data.GetAs<uchar**> ());

		if (!data)
			return {};

		QIcon icon;

		auto cur = *data.Get (false);
		auto end = cur + count;
		while (cur < end)
		{
			QImage img (cur [0], cur [1], QImage::Format_ARGB32);
			cur += 2;
			for (int i = 0; i < img.byteCount () / 4; ++i, ++cur)
				reinterpret_cast<uint*> (img.bits ()) [i] = *cur;

			icon.addPixmap (QPixmap::fromImage (img));
		}

		return icon;
	}

	WinStateFlags XWrapper::GetWindowState (Window wid)
	{
		WinStateFlags result;

		ulong length = 0;
		ulong *data = 0;
		if (!GetWinProp (wid, GetAtom ("_NET_WM_STATE"),
				&length, reinterpret_cast<uchar**> (&data), XA_ATOM))
			return result;

		for (ulong i = 0; i < length; ++i)
		{
			const auto curAtom = data [i];

			auto set = [this, &curAtom, &result] (const QString& atom, WinStateFlag flag)
			{
				if (curAtom == GetAtom ("_NET_WM_STATE_" + atom))
					result |= flag;
			};

			set ("MODAL", WinStateFlag::Modal);
			set ("STICKY", WinStateFlag::Sticky);
			set ("MAXIMIZED_VERT", WinStateFlag::MaximizedVert);
			set ("MAXIMIZED_HORZ", WinStateFlag::MaximizedHorz);
			set ("SHADED", WinStateFlag::Shaded);
			set ("SKIP_TASKBAR", WinStateFlag::SkipTaskbar);
			set ("SKIP_PAGER", WinStateFlag::SkipPager);
			set ("HIDDEN", WinStateFlag::Hidden);
			set ("FULLSCREEN", WinStateFlag::Fullscreen);
			set ("ABOVE", WinStateFlag::OnTop);
			set ("BELOW", WinStateFlag::OnBottom);
			set ("DEMANDS_ATTENTION", WinStateFlag::Attention);
		}

		XFree (data);

		return result;
	}

	AllowedActionFlags XWrapper::GetWindowActions (Window wid)
	{
		AllowedActionFlags result;

		ulong length = 0;
		ulong *data = 0;
		if (!GetWinProp (wid, GetAtom ("_NET_WM_ALLOWED_ACTIONS"),
				&length, reinterpret_cast<uchar**> (&data), XA_ATOM))
			return result;

		for (ulong i = 0; i < length; ++i)
		{
			const auto curAtom = data [i];

			auto set = [this, &curAtom, &result] (const QString& atom, AllowedActionFlag flag)
			{
				if (curAtom == GetAtom ("_NET_WM_ACTION_" + atom))
					result |= flag;
			};

			set ("MOVE", AllowedActionFlag::Move);
			set ("RESIZE", AllowedActionFlag::Resize);
			set ("MINIMIZE", AllowedActionFlag::Minimize);
			set ("SHADE", AllowedActionFlag::Shade);
			set ("STICK", AllowedActionFlag::Stick);
			set ("MAXIMIZE_HORZ", AllowedActionFlag::MaximizeHorz);
			set ("MAXIMIZE_VERT", AllowedActionFlag::MaximizeVert);
			set ("FULLSCREEN", AllowedActionFlag::ShowFullscreen);
			set ("CHANGE_DESKTOP", AllowedActionFlag::ChangeDesktop);
			set ("CLOSE", AllowedActionFlag::Close);
			set ("ABOVE", AllowedActionFlag::MoveToTop);
			set ("BELOW", AllowedActionFlag::MoveToBottom);
		}

		XFree (data);

		return result;
	}

	Window XWrapper::GetActiveApp ()
	{
		auto win = GetActiveWindow ();
		if (!win)
			return 0;

		Window transient = None;
		if (!ShouldShow (win) && XGetTransientForHint (Display_, win, &transient))
			return transient;

		return win;
	}

	bool XWrapper::IsLCWindow (Window wid)
	{
		ulong length = 0;
		Guarded<uchar> data;
		if (GetWinProp (wid, GetAtom ("WM_CLASS"), &length, data.Get ()) &&
				QString (data.GetAs<char*> (false)) == "leechcraft")
			return true;

		return false;
	}

	bool XWrapper::ShouldShow (Window wid)
	{
		if (IsLCWindow (wid))
			return false;

		const QList<Atom> ignoreAtoms
		{
			GetAtom ("_NET_WM_WINDOW_TYPE_DESKTOP"),
			GetAtom ("_NET_WM_WINDOW_TYPE_DOCK"),
			GetAtom ("_NET_WM_WINDOW_TYPE_TOOLBAR"),
			GetAtom ("_NET_WM_WINDOW_TYPE_MENU"),
			GetAtom ("_NET_WM_WINDOW_TYPE_SPLASH"),
			GetAtom ("_NET_WM_WINDOW_TYPE_POPUP_MENU")
		};

		for (const auto& type : GetWindowType (wid))
			if (ignoreAtoms.contains (type))
				return false;

		if (GetWindowState (wid) & WinStateFlag::SkipTaskbar)
			return false;

		Window transient = None;
		if (!XGetTransientForHint (Display_, wid, &transient))
			return true;

		if (transient == 0 || transient == wid || transient == AppWin_)
			return true;

		return !GetWindowType (transient).contains (GetAtom ("_NET_WM_WINDOW_TYPE_NORMAL"));
	}

	void XWrapper::Subscribe (Window wid)
	{
		XSelectInput (Display_, wid, PropertyChangeMask);
	}

	void XWrapper::SetStrut (QWidget *widget, Qt::ToolBarArea area)
	{
		const auto wid = widget->effectiveWinId ();

		const auto& winGeom = widget->geometry ();

		switch (area)
		{
		case Qt::BottomToolBarArea:
			SetStrut (wid,
					0, 0, 0, winGeom.height (),
					0, 0,
					0, 0,
					0, 0,
					winGeom.left (), winGeom.right ());
			break;
		}
	}

	void XWrapper::SetStrut (Window wid,
			int left, int right, int top, int bottom,
			int leftStartY, int leftEndY,
			int rightStartY, int rightEndY,
			int topStartX, int topEndX,
			int bottomStartX, int bottomEndX)
	{
		ulong struts[12] = { 0 };

		struts [0] = left;
		struts [1] = right;
		struts [2] = top;
		struts [3] = bottom;

		struts [4] = leftStartY;
		struts [5] = leftEndY;
		struts [6] = rightStartY;
		struts [7] = rightEndY;
		struts [8] = topStartX;
		struts [9] = topEndX;
		struts [10] = bottomStartX;
		struts [11] = bottomEndX;

		XChangeProperty (Display_, wid, GetAtom ("_NET_WM_STRUT_PARTIAL"),
				XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<uchar*> (struts), 12);

		XChangeProperty (Display_, wid, GetAtom ("_NET_WM_STRUT"),
				XA_CARDINAL, 32, PropModeReplace, reinterpret_cast<uchar*> (struts), 4);
	}

	void XWrapper::RaiseWindow (Window wid)
	{
		SendMessage (wid, GetAtom ("_NET_ACTIVE_WINDOW"), SourcePager);
	}

	void XWrapper::MinimizeWindow (Window wid)
	{
		SendMessage (wid, GetAtom ("WM_CHANGE_STATE"), IconicState);
	}

	void XWrapper::MaximizeWindow (Window wid)
	{
		SendMessage (wid, GetAtom ("_NET_WM_STATE"), StateAdd,
				GetAtom ("_NET_WM_STATE_MAXIMIZED_VERT"),
				GetAtom ("_NET_WM_STATE_MAXIMIZED_HORZ"),
				SourcePager);
	}

	void XWrapper::UnmaximizeWindow (Window wid)
	{
		SendMessage (wid, GetAtom ("_NET_WM_STATE"), StateRemove,
				GetAtom ("_NET_WM_STATE_MAXIMIZED_VERT"),
				GetAtom ("_NET_WM_STATE_MAXIMIZED_HORZ"),
				SourcePager);
	}

	void XWrapper::ResizeWindow (Window wid, int width, int height)
	{
		XResizeWindow (Display_, wid, width, height);
	}

	void XWrapper::ShadeWindow (Window wid)
	{
		SendMessage (wid, GetAtom ("_NET_WM_STATE"),
				StateAdd, GetAtom ("_NET_WM_STATE_SHADED"), 0, SourcePager);
	}

	void XWrapper::UnshadeWindow (Window wid)
	{
		SendMessage (wid, GetAtom ("_NET_WM_STATE"),
				StateRemove, GetAtom ("_NET_WM_STATE_SHADED"), 0, SourcePager);
	}

	void XWrapper::MoveWindowTo (Window wid, Layer layer)
	{
		const auto top = layer == Layer::Top ? StateAdd : StateRemove;
		const auto bottom = layer == Layer::Bottom ? StateAdd : StateRemove;

		SendMessage (wid, GetAtom ("_NET_WM_STATE"), top,
				GetAtom ("_NET_WM_STATE_ABOVE"), 0, SourcePager);

		SendMessage (wid, GetAtom ("_NET_WM_STATE"), bottom,
				GetAtom ("_NET_WM_STATE_BELOW"), 0, SourcePager);
	}

	void XWrapper::CloseWindow (Window wid)
	{
		SendMessage (wid, GetAtom ("_NET_CLOSE_WINDOW"), 0, SourcePager);
	}

	template<typename T>
	void XWrapper::HandlePropNotify (T ev)
	{
		if (ev->state == PropertyDelete)
			return;

		const auto wid = ev->window;

		if (wid == AppWin_)
		{
			if (ev->atom == GetAtom ("_NET_CLIENT_LIST"))
				emit windowListChanged ();
			else if (ev->atom == GetAtom ("_NET_ACTIVE_WINDOW"))
				emit activeWindowChanged ();
			else if (ev->atom == GetAtom ("_NET_CURRENT_DESKTOP"))
				emit desktopChanged ();
		}
		else if (ShouldShow (wid))
		{
			if (ev->atom == GetAtom ("_NET_WM_VISIBLE_NAME") ||
					ev->atom == GetAtom ("WM_NAME"))
				emit windowNameChanged (wid);
			else if (ev->atom == GetAtom ("_NET_WM_ICON"))
				emit windowIconChanged (wid);
			else if (ev->atom == GetAtom ("_NET_WM_DESKTOP"))
				emit windowDesktopChanged (wid);
			else if (ev->atom == GetAtom ("_NET_WM_STATE"))
				emit windowStateChanged (wid);
			else if (ev->atom == GetAtom ("_NET_WM_ALLOWED_ACTIONS"))
				emit windowActionsChanged (wid);
		}
	}

	Window XWrapper::GetActiveWindow ()
	{
		ulong length = 0;
		Guarded<ulong> data;

		if (!GetRootWinProp (GetAtom ("_NET_ACTIVE_WINDOW"), &length, data.GetAs<uchar**> (), XA_WINDOW))
			return 0;

		if (!length)
			return 0;

		return data [0];
	}

	int XWrapper::GetDesktopCount ()
	{
		ulong length = 0;
		Guarded<ulong> data;

		if (GetRootWinProp (GetAtom ("_NET_NUMBER_OF_DESKTOPS"), &length, data.GetAs<uchar**> (), XA_CARDINAL))
			return data [0];

		return -1;
	}

	int XWrapper::GetCurrentDesktop ()
	{
		ulong length = 0;
		Guarded<ulong> data;

		if (GetRootWinProp (GetAtom ("_NET_CURRENT_DESKTOP"), &length, data.GetAs<uchar**> (), XA_CARDINAL))
			return data [0];

		return -1;
	}

	void XWrapper::SetCurrentDesktop (int desktop)
	{
		SendMessage (AppWin_, GetAtom ("_NET_CURRENT_DESKTOP"), desktop);
	}

	QStringList XWrapper::GetDesktopNames ()
	{
		ulong length = 0;
		Guarded<uchar> data;

		if (!GetRootWinProp (GetAtom ("_NET_DESKTOP_NAMES"),
				&length, data.GetAs<uchar**> (), GetAtom ("UTF8_STRING")))
			return {};

		if (!data)
			return {};

		QStringList result;
		for (char *pos = data.GetAs<char*> (false), *end = data.GetAs<char*> (false) + length; pos < end; )
		{
			const auto& str = QString::fromUtf8 (pos);
			result << str;
			pos += str.toUtf8 ().size () + 1;
		}
		return result;
	}

	QString XWrapper::GetDesktopName (int desktop, const QString& def)
	{
		return GetDesktopNames ().value (desktop, def);
	}

	int XWrapper::GetWindowDesktop (Window wid)
	{
		ulong length = 0;
		Guarded<ulong> data;
		if (GetWinProp (wid, GetAtom ("_NET_WM_DESKTOP"), &length, data.GetAs<uchar**> (), XA_CARDINAL))
			return data [0];

		if (GetWinProp (wid, GetAtom ("_WIN_WORKSPACE"), &length, data.GetAs<uchar**> (), XA_CARDINAL))
			return data [0];

		return -1;
	}

	void XWrapper::MoveWindowToDesktop (Window wid, int num)
	{
		SendMessage (wid, GetAtom ("_NET_WM_DESKTOP"), num);
	}

	Atom XWrapper::GetAtom (const QString& name)
	{
		if (Atoms_.contains (name))
			return Atoms_ [name];

		auto atom = XInternAtom (Display_, name.toLocal8Bit (), false);
		Atoms_ [name] = atom;
		return atom;
	}

	bool XWrapper::GetWinProp (Window win, Atom property,
			ulong *length, unsigned char **result, Atom req) const
	{
		int fmt = 0;
		ulong type = 0, rest = 0;
		return XGetWindowProperty (Display_, win,
				property, 0, 1024, false, req, &type,
				&fmt, length, &rest, result) == Success;
	}

	bool XWrapper::GetRootWinProp (Atom property,
			ulong *length, uchar **result, Atom req) const
	{
		return GetWinProp (AppWin_, property, length, result, req);
	}

	QList<Atom> XWrapper::GetWindowType (Window wid)
	{
		QList<Atom> result;

		ulong length = 0;
		ulong *data = nullptr;

		if (!GetWinProp (wid, GetAtom ("_NET_WM_WINDOW_TYPE"),
				&length, reinterpret_cast<uchar**> (&data)))
			return result;

		for (ulong i = 0; i < length; ++i)
			result << data [i];

		XFree (data);
		return result;
	}

	bool XWrapper::SendMessage (Window wid, Atom atom, ulong d0, ulong d1, ulong d2, ulong d3, ulong d4)
	{
		XClientMessageEvent msg;
		msg.window = wid;
		msg.type = ClientMessage;
		msg.message_type = atom;
		msg.send_event = true;
		msg.display = Display_;
		msg.format = 32;
		msg.data.l [0] = d0;
		msg.data.l [1] = d1;
		msg.data.l [2] = d2;
		msg.data.l [3] = d3;
		msg.data.l [4] = d4;

		return XSendEvent (Display_, AppWin_, FALSE, SubstructureRedirectMask | SubstructureNotifyMask,
				reinterpret_cast<XEvent*> (&msg)) == Success;
	}

	void XWrapper::initialize ()
	{
	}
}
}
