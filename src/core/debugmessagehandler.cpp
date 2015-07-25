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

#include "debugmessagehandler.h"
#include <memory>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdlib>

#ifdef _GNU_SOURCE
#include <execinfo.h>
#endif

#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QDateTime>
#include <QDir>

QMutex G_DbgMutex;
uint Counter = 0;

namespace
{
	QString GetFilename (QtMsgType type)
	{
		switch (type)
		{
		case QtDebugMsg:
			return "debug.log";
		case QtWarningMsg:
			return "warning.log";
		case QtCriticalMsg:
			return "critical.log";
		case QtFatalMsg:
			return "fatal.log";
		}

		return "unknown.log";
	}

	std::string GetColorCode (QtMsgType type)
	{
		switch (type)
		{
		case QtDebugMsg:
			return "\x1b[32m";
		case QtWarningMsg:
			return "\x1b[33m";
		case QtCriticalMsg:
			return "\x1b[31m";
		case QtFatalMsg:
			return "\x1b[35m";
		}

		return {};
	}

	std::shared_ptr<std::ostream> GetOstream (QtMsgType type, DebugHandler::DebugWriteFlags flags)
	{
		if (flags & DebugHandler::DWFNoFileLog)
		{
			auto& stream = type == QtDebugMsg ? std::cout : std::cerr;

			stream << GetColorCode (type);
			switch (type)
			{
			case QtDebugMsg:
				stream << "[DBG] ";
				break;
			case QtWarningMsg:
				stream << "[WRN] ";
				break;
			case QtCriticalMsg:
				stream << "[CRT] ";
				break;
			case QtFatalMsg:
				stream << "[FTL] (yay, really `faster than light`!) ";
				break;
			}
			stream << "\x1b[0m";

			return { &stream, [] (std::ostream*) {} };
		}

		const QString name = QDir::homePath () + "/.leechcraft/" + GetFilename (type);

		auto ostr = std::make_shared<std::ofstream> ();
		ostr->open (QDir::toNativeSeparators (name).toStdString (), std::ios::app);
		return ostr;
	}

	void PrintBacktrace (const std::shared_ptr<std::ostream>& ostr)
	{
		const int maxSize = 100;
		void *array [maxSize];
		size_t size = backtrace (array, maxSize);
		char **strings = backtrace_symbols (array, size);

		*ostr << "Backtrace of " << size << " frames:" << std::endl;

		for (size_t i = 0; i < size; ++i)
			*ostr << i << "\t" << strings [i] << std::endl;

		std::free (strings);
	}
}

namespace DebugHandler
{
	void Write (QtMsgType type, const char *message, DebugWriteFlags flags)
	{
#if !defined (Q_OS_WIN32)
		if (!strcmp (message, "QPixmap::handle(): Pixmap is not an X11 class pixmap") ||
				strstr (message, ": Painter not active"))
			return;
#endif
#if defined (Q_OS_WIN32)
		if (!strcmp (message, "QObject::startTimer: QTimer can only be used with threads started with QThread"))
			return;
#endif

		QMutexLocker locker { &G_DbgMutex };

		const auto& ostr = GetOstream (type, flags);
		*ostr << "["
				<< QDateTime::currentDateTime ().toString ("dd.MM.yyyy HH:mm:ss.zzz").toStdString ()
				<< "] ["
				<< QThread::currentThread ()
				<< "] ["
				<< std::setfill ('0')
				<< std::setw (3)
				<< Counter++
				<< "] "
				<< message
				<< std::endl;

#ifdef _GNU_SOURCE
		if (type != QtDebugMsg && (flags & DWFBacktrace))
			PrintBacktrace (ostr);
#endif
	}
}
