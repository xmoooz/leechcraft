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

#pragma once

#include <QObject>
#include <QSet>
#include <QFutureInterface>
#include <interfaces/media/idiscographyprovider.h>

class QNetworkAccessManager;

namespace LeechCraft
{
namespace Util
{
	class QueueManager;
}

namespace MusicZombie
{
	class PendingDisco : public QObject
	{
		const QString Artist_;
		const QString ReleaseName_;
		const QSet<QString> Hints_;

		Util::QueueManager *Queue_;

		QNetworkAccessManager *NAM_;

		using QueryResult_t = Media::IDiscographyProvider::Result_t;
		QFutureInterface<QueryResult_t> Promise_;

		using Artist2Releases_t = QHash<QString, QSet<QString>>;
	public:
		PendingDisco (Util::QueueManager*, const QString&, const QString&,
				const QStringList&, QNetworkAccessManager*, QObject* = nullptr);

		QFuture<Media::IDiscographyProvider::Result_t> GetFuture ();
	private:
		void RequestArtist (bool);

		void HandleData (const QByteArray&, bool);
		void HandleDataNoHints (const Artist2Releases_t&);
		void HandleDataWithHints (Artist2Releases_t&);

		void HandleGotID (const QString&);

		void HandleLookupFinished (const QByteArray&);
	};
}
}
