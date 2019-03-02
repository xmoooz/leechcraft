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

#include <optional>
#include <boost/variant.hpp>
#include <QObject>
#include <QCache>
#include "interfaces/azoth/ihaveavatars.h"

template<typename>
class QFuture;

namespace LeechCraft
{
namespace Azoth
{
	class ICLEntry;
	class AvatarsStorageThread;

	using MaybeImage = std::optional<QImage>;
	using MaybeByteArray = std::optional<QByteArray>;

	class AvatarsStorage : public QObject
	{
		AvatarsStorageThread * const StorageThread_;

		using CacheKey_t = QPair<QString, IHaveAvatars::Size>;
		using CacheValue_t = boost::variant<QByteArray, QImage>;
		QCache<CacheKey_t, CacheValue_t> Cache_;
	public:
		explicit AvatarsStorage (QObject* = nullptr);

		QFuture<void> SetAvatar (const QString&, IHaveAvatars::Size, const QImage&);
		QFuture<void> SetAvatar (const QString&, IHaveAvatars::Size, const QByteArray&);
		QFuture<MaybeImage> GetAvatar (const ICLEntry*, IHaveAvatars::Size);
		QFuture<MaybeByteArray> GetAvatar (const QString&, IHaveAvatars::Size);

		QFuture<void> DeleteAvatars (const QString&);

		void SetCacheSize (int mibs);
	};
}
}
