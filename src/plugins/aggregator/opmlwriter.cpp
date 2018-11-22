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

#include "opmlwriter.h"
#include <QByteArray>
#include <QStringList>
#include <QDomDocument>
#include <QDomElement>
#include <QtDebug>
#include <util/util.h>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/itagsmanager.h>
#include "core.h"
#include "storagebackendmanager.h"

namespace LeechCraft
{
namespace Aggregator
{
	QString OPMLWriter::Write (const channels_shorts_t& channels,
			const QString& title,
			const QString& owner,
			const QString& ownerEmail) const
	{
		QDomDocument doc;
		QDomElement root = doc.createElement ("opml");
		doc.appendChild (root);
		WriteHead (root, doc, title, owner, ownerEmail);
		WriteBody (root, doc, channels);

		return doc.toString ();
	}

	void OPMLWriter::WriteHead (QDomElement& root,
			QDomDocument& doc,
			const QString& title,
			const QString& owner,
			const QString& ownerEmail) const
	{
		QDomElement head = doc.createElement ("head");
		QDomElement text = doc.createElement ("text");
		head.appendChild (text);
		root.appendChild (head);

		if (!title.isEmpty ())
		{
			QDomText t = doc.createTextNode (title);
			text.appendChild (t);
		}
		if (!owner.isEmpty ())
		{
			QDomElement elem = doc.createElement ("owner");
			QDomText t = doc.createTextNode (owner);
			elem.appendChild (t);
			head.appendChild (elem);
		}
		if (!ownerEmail.isEmpty ())
		{
			QDomElement elem = doc.createElement ("ownerEmail");
			QDomText t = doc.createTextNode (ownerEmail);
			elem.appendChild (t);
			head.appendChild (elem);
		}
	}

	namespace
	{
		QString TagGetter (const QDomElement& elem)
		{
			return elem.attribute ("text");
		}

		void TagSetter (QDomElement& result, const QString& tag)
		{
			result.setAttribute ("text", tag);
			result.setAttribute ("isOpen", "true");
		}
	}

	void OPMLWriter::WriteBody (QDomElement& root,
			QDomDocument& doc,
			const channels_shorts_t& channels) const
	{
		const auto& sb = StorageBackendManager::Instance ().MakeStorageBackendForThread ();

		auto body = doc.createElement ("body");
		for (const auto& cs : channels)
		{
			auto tags = Core::Instance ().GetProxy ()->GetTagsManager ()->GetTags (cs.Tags_);
			tags.sort ();

			auto inserter = Util::GetElementForTags (tags,
					body, doc, "outline",
					&TagGetter,
					&TagSetter);
			auto item = doc.createElement ("outline");
			item.setAttribute ("title", cs.Title_);
			item.setAttribute ("xmlUrl", sb->GetFeed (cs.FeedID_).URL_);
			item.setAttribute ("htmlUrl", cs.Link_);
			inserter.appendChild (item);
		}

		root.appendChild (body);
	}
}
}
