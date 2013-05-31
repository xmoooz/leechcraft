/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2010-2012  Oleg Linkin
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

#include "fileswatcher_inotify.h"
#include <stdexcept>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <QtDebug>
#include <QStringList>
#include <QTimer>
#include "utils.h"
#include <plugins/qrosp/third-party/qmetaobjectbuilder.h>

namespace LeechCraft
{
namespace NetStoreManager
{
	FilesWatcherInotify::FilesWatcherInotify (QObject *parent)
	: FilesWatcherBase (parent)
	, INotifyDescriptor_ (inotify_init ())
	, WatchMask_ (IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY |IN_MOVED_FROM | IN_MOVED_TO )
	, WaitMSecs_ (50)
	, Timer_ (new QTimer (this))
	{
		if (INotifyDescriptor_ < 0)
			throw std::runtime_error ("inotify_init failed. Synchronization will not work.");

		EventSize_ = sizeof (struct inotify_event);
		BufferLength_ = 1024 * (EventSize_ + 16);

		connect (Timer_,
				SIGNAL (timeout ()),
				this,
				SLOT (checkNotifications ()));
	}

	void FilesWatcherInotify::HandleNotification (int descriptor)
	{
		char buffer [BufferLength_];
		ssize_t length = read (descriptor, buffer, BufferLength_);
		if (length < 0)
		{
			qDebug () << "read error";
			return;
		}

		int i = 0;

		QList<inotify_event*> eventsBuffer;
		while (i < length)
		{
			struct inotify_event *event = reinterpret_cast<inotify_event*> (&buffer [i]);

			const QString path = WatchedPathes2Descriptors_.right.at (event->wd);
			const QString fullPath = path + "/" + QString (event->name);

			i += EventSize_ + event->len;

// 			if (!(event->mask & IN_ISDIR) &&
// 					IsInExceptionList (fullPath))
// 				continue;

			if (event->mask & IN_CREATE)
			{
				if (event->mask & IN_ISDIR)
					AddPathWithNotify (fullPath);
				else
					emit fileWasCreated (fullPath);
			}
			else if (event->mask & IN_DELETE)
			{
				if (!(event->mask & IN_ISDIR))
					emit fileWasRemoved (fullPath);
			}
			else if (event->mask & IN_MODIFY)
			{
				if (!(event->mask & IN_ISDIR))
					emit fileWasUpdated (fullPath);
			}
			else if (event->mask & IN_MOVED_FROM)
			{
				eventsBuffer << event;
			}
			else if (event->mask & IN_MOVED_TO)
			{
				if (eventsBuffer.isEmpty ())
					AddPathWithNotify (fullPath);
				else
					for (auto it = eventsBuffer.begin (); it < eventsBuffer.end ();)
					{
						if ((*it)->cookie == event->cookie &&
								(*it)->wd == event->wd)
						{
							QString oldPath = path + "/" + QString ((*it)->name);
							if ((*it)->mask & IN_ISDIR)
							{
								qDebug () << oldPath << fullPath;
								RemoveWatchingPath ((*it)->wd);
								addPath (fullPath);
							}
							emit entryWasRenamed (oldPath, fullPath);
							it = eventsBuffer.erase (it);
							break;
						}
						else if ((*it)->cookie == event->cookie)
						{
							const QString oldPrePath = WatchedPathes2Descriptors_.right.at ((*it)->wd);
							emit entryWasMoved (oldPrePath + "/" + QString ((*it)->name),
									fullPath);
							it = eventsBuffer.erase (it);
							break;
						}
					}
			}
			else if (event->mask & IN_DELETE_SELF)
				emit dirWasRemoved (path);

			if (event->mask & IN_IGNORED)
				RemoveWatchingPath (event->wd);
		}

		for (auto it = eventsBuffer.begin (); it < eventsBuffer.end ();)
		{
			const QString path = WatchedPathes2Descriptors_.right.at ((*it)->wd);
			const QString fullPath = path + "/" + QString ((*it)->name);

			if ((*it)->mask & IN_ISDIR)
				emit dirWasRemoved (fullPath);
			else
				emit fileWasRemoved (fullPath);

			it = eventsBuffer.erase (it);
		}
	}

	void FilesWatcherInotify::AddPathWithNotify (const QString& path)
	{
		if (!QFileInfo (path).isDir ())
			return;

		addPath (path);
		emit dirWasCreated (path);
		const auto paths = Utils::ScanDir (QDir::AllEntries | QDir::NoDotAndDotDot,
				path,
				true);
		for (const auto& p : paths)
			if (QFileInfo (p).isDir ())
			{
				addPath (p);
				emit dirWasCreated (p);
			}
			else
				emit fileWasCreated (p);
	}

	bool FilesWatcherInotify::IsInExceptionList (const QString& path) const
	{
		if (ExceptionMasks_.isEmpty ())
			return false;

		for (const auto& mask : ExceptionMasks_)
		{
			QRegExp rx (mask, Qt::CaseInsensitive, QRegExp::WildcardUnix);
			if (rx.exactMatch (path))
			{
				qDebug () << "entry with name"
						<< QFileInfo (path).fileName ()
						<< "was ignored by "
						<< mask;
				return true;
			}
		}

		return false;
	}

	void FilesWatcherInotify::RemoveWatchingPath (int descriptor)
	{
		inotify_rm_watch (INotifyDescriptor_, descriptor);
		WatchedPathes2Descriptors_.right.erase (descriptor);
	}

	void FilesWatcherInotify::updatePaths (const QStringList& paths)
	{
		for (const auto& path : paths)
			if (!WatchedPathes2Descriptors_.left.count (path))
				addPath (path);

		for (auto it = WatchedPathes2Descriptors_.left.begin ();
				it != WatchedPathes2Descriptors_.left.end ();)
		{
			if (!paths.contains (it->first))
				it = WatchedPathes2Descriptors_.left.erase (it);
			else
				++it;
		}
	}

	void FilesWatcherInotify::checkNotifications ()
	{
		struct pollfd pfd = { INotifyDescriptor_, POLLIN, 0 };
		int res = poll (&pfd, 1, WaitMSecs_);

		if (res < 0)
			qDebug () << "error";
		else if (!res)
		{
		}
		else
			HandleNotification (INotifyDescriptor_);
	}

	void FilesWatcherInotify::addPath (const QString& path)
	{
		const int fd = inotify_add_watch (INotifyDescriptor_, path.toUtf8 (), WatchMask_);
		WatchedPathes2Descriptors_.insert ({ path, fd });

		if (!Timer_->isActive ())
			Timer_->start (1000);
	}

	void FilesWatcherInotify::release ()
	{
		for (auto map : WatchedPathes2Descriptors_.left)
			inotify_rm_watch (INotifyDescriptor_, map.second);

		WatchedPathes2Descriptors_.clear ();
		close (INotifyDescriptor_);
	}

	void FilesWatcherInotify::updateExceptions (QStringList masks)
	{
		ExceptionMasks_ = masks;
		ExceptionMasks_.removeAll ("");
		ExceptionMasks_.removeDuplicates ();

		for (const auto& pair : WatchedPathes2Descriptors_.left)
			if (IsInExceptionList (pair.first))
				RemoveWatchingPath (pair.second);
	}
}
}
