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

#include "namauth.h"
#include <QIcon>
#include <QMessageBox>
#include <interfaces/core/icoreproxy.h>
#include <util/util.h>
#include <util/sll/visitor.h>
#include <util/threads/futures.h>
#include <util/db/consistencychecker.h>
#include "namhandler.h"
#include "sqlstoragebackend.h"

namespace LeechCraft
{
namespace NamAuth
{
	void Plugin::Init (ICoreProxy_ptr proxy)
	{
		Util::InstallTranslator ("namauth");

		const auto checker = Util::ConsistencyChecker::Create (SQLStorageBackend::GetDBPath (), GetName ());
		Util::Sequence (this, checker->StartCheck ()) >>
				[=] (const auto& result)
				{
					Util::Visit (result,
							[=] (Util::ConsistencyChecker::Succeeded) { InitStorage (proxy); },
							[=] (Util::ConsistencyChecker::Failed failed)
							{
								Util::Sequence (this, failed->DumpReinit ()) >>
										[=] (const auto& result)
										{
											Util::Visit (result,
													[=] (Util::ConsistencyChecker::DumpError err)
													{
														QMessageBox::critical (nullptr,
																tr ("LeechCraft"),
																tr ("Unable to recover the HTTP passwords database: %1.")
																		.arg (err.Error_));

														const auto& path = SQLStorageBackend::GetDBPath ();
														QFile::copy (path, path + ".old");
														QFile::remove (path);

														InitStorage (proxy);
													},
													[=] (Util::ConsistencyChecker::DumpFinished)
													{
														InitStorage (proxy);
													});
										};
							});
				};
	}

	void Plugin::SecondInit ()
	{
	}

	QByteArray Plugin::GetUniqueID () const
	{
		return "org.LeechCraft.NamAuth";
	}

	void Plugin::Release ()
	{
	}

	QString Plugin::GetName () const
	{
		return "NamAuth";
	}

	QString Plugin::GetInfo () const
	{
		return tr ("Provides basic support for HTTP-level authentication.");
	}

	QIcon Plugin::GetIcon () const
	{
		return {};
	}

	void Plugin::InitStorage (const ICoreProxy_ptr& proxy)
	{
		const auto sb = new SQLStorageBackend;
		new NamHandler { sb, proxy->GetNetworkAccessManager () };
	}
}
}

LC_EXPORT_PLUGIN (leechcraft_namauth, LeechCraft::NamAuth::Plugin);
