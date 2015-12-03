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

#include "itemsfinder.h"
#include <QDir>
#include <QTimer>
#include <QtDebug>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <util/sll/prelude.h>
#include <util/threads/futures.h>
#include "interfaces/core/iiconthememanager.h"
#include "xdg.h"
#include "item.h"

namespace LeechCraft
{
namespace Util
{
namespace XDG
{
	ItemsFinder::ItemsFinder (ICoreProxy_ptr proxy,
			const QList<Type>& types, QObject *parent)
	: QObject { parent }
	, Proxy_ { proxy }
	, Types_ { types }
	{
		QTimer::singleShot (1000, this, SLOT (update ()));
	}

	bool ItemsFinder::IsReady () const
	{
		return IsReady_;
	}

	Cat2Items_t ItemsFinder::GetItems () const
	{
		return Items_;
	}

	Item_ptr ItemsFinder::FindItem (const QString& id) const
	{
		for (const auto& list : Items_)
		{
			const auto pos = std::find_if (list.begin (), list.end (),
					[&id] (const Item_ptr& item) { return item->GetPermanentID () == id; });
			if (pos != list.end ())
				return *pos;
		}

		return {};
	}

	namespace
	{
		QStringList ScanDir (const QString& path)
		{
			const auto& infos = QDir (path).entryInfoList ({ "*.desktop" },
						QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot);

			return Util::ConcatMap (infos,
					[] (const QFileInfo& info)
					{
						return info.isDir () ?
								ScanDir (info.absoluteFilePath ()) :
								QStringList { info.absoluteFilePath () };
					});
		}

		QIcon GetIconDevice (ICoreProxy_ptr proxy, QString name)
		{
			if (name.isEmpty ())
				return QIcon ();

			if (name.endsWith (".png") || name.endsWith (".svg"))
				name.chop (4);

			auto result = proxy->GetIconThemeManager ()->GetIcon (name);
			if (!result.isNull ())
				return result;

			result = GetAppIcon (name);
			if (!result.isNull ())
				return result;

			qDebug () << Q_FUNC_INFO << name << "not found";

			return result;
		}

		void FixIcons (const Cat2Items_t& items, ICoreProxy_ptr proxy)
		{
			for (const auto& list : items)
				for (const auto& item : list)
					if (item->GetIcon ().isNull ())
						item->SetIcon (GetIconDevice (proxy, item->GetIconName ()));
		}

		Cat2Items_t FindAndParse (const QList<Type>& types)
		{
			Cat2Items_t result;

			QStringList paths;
			for (const auto& dir : ToPaths (types))
				paths << ScanDir (dir);

			for (const auto& path : paths)
			{
				Item_ptr item;
				try
				{
					item = Item::FromDesktopFile (path);
				}
				catch (const std::exception& e)
				{
					qWarning () << Q_FUNC_INFO
							<< "error parsing"
							<< path
							<< e.what ();
					continue;
				}

				if (!item->IsValid ())
				{
					qWarning () << Q_FUNC_INFO
							<< "invalid item"
							<< path;
					continue;
				}

				for (const auto& cat : item->GetCategories ())
					if (!cat.startsWith ("X-"))
						result [cat] << item;
			}

			return result;
		}
	}

	void ItemsFinder::update ()
	{
		if (IsScanning_)
			return;

		IsScanning_ = true;

		Util::Sequence (this, QtConcurrent::run (FindAndParse, Types_)) >>
				[this] (const Cat2Items_t& result)
				{
					IsScanning_ = false;
					IsReady_ = true;

					if (result == Items_)
						return;

					Items_ = std::move (result);
					FixIcons (Items_, Proxy_);

					emit itemsListChanged ();
				};
	}
}
}
}
