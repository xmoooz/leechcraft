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

#include "downloadhelpers.h"
#include <QFuture>
#include <QFile>
#include <QtDebug>
#include <util/sll/either.h>
#include <util/sll/visitor.h>
#include <util/sys/paths.h>
#include <util/xpc/util.h>
#include <util/threads/futures.h>
#include <interfaces/core/ientitymanager.h>

namespace LeechCraft::Util
{
	std::optional<QFuture<TempResultType_t>> DownloadAsTemporary (IEntityManager *iem,
			const QUrl& url, DownloadParams params)
	{
		const auto& path = Util::GetTemporaryName ();
		auto e = Util::MakeEntity (url,
				path,
				DoNotSaveInHistory |
					Internal |
					DoNotNotifyUser |
					NotPersistent);
		e.Mime_ = std::move (params.Mime_);
		e.Additional_ = std::move (params.Additional_);

		auto res = iem->DelegateEntity (e);
		if (!res)
		{
			qWarning () << Q_FUNC_INFO
					<< "delegation failed for"
					<< url;
			return {};
		}

		return Util::Sequence (params.Context_, res.DownloadResult_) >>
				Util::Visitor
				{
					[] (const IDownload::Error& err) { return Util::MakeReadyFuture (TempResultType_t::Left (err)); },
					[path] (IDownload::Success)
					{
						QFile file { path };
						auto removeGuard = Util::MakeScopeGuard ([&file] { file.remove (); });
						if (!file.open (QIODevice::ReadOnly))
						{
							qWarning () << Q_FUNC_INFO
									<< "unable to open downloaded file"
									<< file.errorString ();
							return Util::MakeReadyFuture (TempResultType_t::Left ({
									IDownload::Error::Type::LocalError,
									"unable to open file"
							}));
						}

						return Util::MakeReadyFuture (TempResultType_t::Right (file.readAll ()));
					}
				};
	}

}