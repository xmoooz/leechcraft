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

#include "checker.h"
#include <algorithm>
#include <cmath>
#include <QtDebug>
#include <util/sll/visitor.h>
#include <util/sll/curry.h>
#include <util/threads/futures.h>
#include <interfaces/core/ipluginsmanager.h>
#include <interfaces/lmp/ilmpproxy.h>
#include <interfaces/lmp/ilocalcollection.h>
#include "checkmodel.h"

namespace LeechCraft
{
namespace LMP
{
namespace BrainSlugz
{
	Checker::Checker (CheckModel *model,
			const QList<Media::ReleaseInfo::Type>& types,
			const ICoreProxy_ptr& coreProxy,
			QObject *parent)
	: QObject { parent }
	, Model_ { model }
	, Provider_ { coreProxy->GetPluginsManager ()->
				GetAllCastableTo<Media::IDiscographyProvider*> ().value (0) }
	, Types_ { types }
	, Artists_ { Model_->GetSelectedArtists () }
	{
		if (!Provider_)
		{
			qWarning () << Q_FUNC_INFO
					<< "no providers :(";
			deleteLater ();
			return;
		}

		rotateQueue ();
	}

	int Checker::GetRemainingCount () const
	{
		return Artists_.size ();
	}

	void Checker::HandleReady ()
	{
		emit finished ();
	}

	namespace
	{
		void CleanupPunctuation (QString& name)
		{
			for (auto c : { '(', ')', ',', '.', ':' })
				name.remove (c);
			for (auto str : { QString::fromUtf8 ("—") })
				name.remove (str);
			name = name.trimmed ().simplified ();
		}

		void CleanupAlbumName (QString& name)
		{
			name.remove ("EP");
			name.remove (" the ", Qt::CaseInsensitive);
			if (name.startsWith ("the ", Qt::CaseInsensitive))
				name = name.mid (4);
			name = name.trimmed ().simplified ();
		}

		const int MaxYearDiff = 4;

		bool IsSameRelease (const Collection::Album_ptr& albumPtr, const Media::ReleaseInfo& release)
		{
			auto name1 = albumPtr->Name_.toLower ();
			auto name2 = release.Name_.toLower ();

			CleanupPunctuation (name1);
			CleanupPunctuation (name2);

			if (name1 == name2)
				return true;

			CleanupAlbumName (name1);
			CleanupAlbumName (name2);

			if (albumPtr->Year_ == release.Year_ &&
					(name1.contains (name2) || name2.contains (name1)))
				return true;

			return std::abs (static_cast<double> (albumPtr->Year_ - release.Year_)) <= MaxYearDiff &&
					name1 == name2;
		}
	}

	void Checker::HandleDiscoReady (const Collection::Artist& artist, QList<Media::ReleaseInfo> releases)
	{
		releases.erase (std::remove_if (releases.begin (), releases.end (),
					[this] (const Media::ReleaseInfo& info)
					{
						return !Types_.contains (info.Type_);
					}),
				releases.end ());

		bool foundSome = false;

		for (const auto& albumPtr : artist.Albums_)
		{
			const auto pos = std::find_if (releases.begin (), releases.end (), Util::Curry (IsSameRelease) (albumPtr));
			if (pos == releases.end ())
				continue;

			releases.erase (pos);
			foundSome = true;
		}

		if (!foundSome)
		{
			qWarning () << Q_FUNC_INFO
					<< "we probably found something different for"
					<< artist.Name_;
			for (const auto& release : releases)
				qWarning () << "\t" << release.Year_ << release.Name_;
			Model_->MarkNoNews (artist);
		}
		else if (releases.isEmpty ())
			Model_->MarkNoNews (artist);
		else
			Model_->SetMissingReleases (releases, artist);
	}

	void Checker::rotateQueue ()
	{
		emit progress (Artists_.size ());

		if (Artists_.isEmpty ())
		{
			HandleReady ();
			return;
		}

		auto artist = Artists_.takeFirst ();
		Util::Sequence (this, Provider_->GetDiscography (artist.Name_, {})) >>
				Util::Visitor
				{
					[=] (const QString& error)
					{
						qWarning () << Q_FUNC_INFO
								<< artist.Name_
								<< error;
						Model_->MarkNoNews (artist);
					},
					[=] (const auto& result) { HandleDiscoReady (artist, result); }
				}.Finally ([this] { rotateQueue (); });
	}
}
}
}
