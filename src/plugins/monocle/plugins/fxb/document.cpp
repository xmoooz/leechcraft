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

#include "document.h"
#include <QFile>
#include <QDomDocument>
#include <QTextDocument>
#include <QApplication>
#include <QPalette>
#include <QtDebug>
#include <util/sll/either.h>
#include "fb2converter.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace Monocle
{
namespace FXB
{
	Document::Document (const QString& filename, QObject *plugin)
	: DocURL_ (QUrl::fromLocalFile (filename))
	, Plugin_ (plugin)
	{
		SetSettings ();

		QFile file (filename);
		if (!file.open (QIODevice::ReadOnly))
		{
			qWarning () << Q_FUNC_INFO
					<< "unable to open file"
					<< file.fileName ()
					<< file.errorString ();
			return;
		}

		QDomDocument doc;
		if (!doc.setContent (file.readAll (), true))
		{
			qWarning () << Q_FUNC_INFO
					<< "malformed XML in"
					<< filename;
			return;
		}

		FB2Converter::Config cfg {};
		cfg.DefaultFont_ = XmlSettingsManager::Instance ().property ("DefaultFont").value<QFont> ();
		cfg.PageSize_ = {
				XmlSettingsManager::Instance ().property ("PageWidth").toInt (),
				XmlSettingsManager::Instance ().property ("PageHeight").toInt ()
			};
		cfg.Margins_ = {
				XmlSettingsManager::Instance ().property ("LeftMargin").toInt (),
				XmlSettingsManager::Instance ().property ("TopMargin").toInt (),
				XmlSettingsManager::Instance ().property ("RightMargin").toInt (),
				XmlSettingsManager::Instance ().property ("BottomMargin").toInt ()
			};
		cfg.BackgroundColor_ = qApp->palette ().color (QPalette::Base);
		cfg.LinkColor_ = qApp->palette ().color (QPalette::Link);

		FB2Converter conv { this, doc, cfg };
		Util::Visit (conv.GetResult (),
				[this] (const FB2Converter::ConvertedDocument& result)
				{
					SetDocument (result.Doc_, result.Links_);
					Info_ = result.Info_;
					TOC_ = result.TOC_;
				},
				Util::Visitor
				{
					[] (auto) {}
				});
	}

	QObject* Document::GetBackendPlugin () const
	{
		return Plugin_;
	}

	QObject* Document::GetQObject ()
	{
		return this;
	}

	DocumentInfo Document::GetDocumentInfo () const
	{
		return Info_;
	}

	QUrl Document::GetDocURL () const
	{
		return DocURL_;
	}

	TOCEntryLevel_t Document::GetTOC ()
	{
		return TOC_;
	}

	void Document::RequestNavigation (int page)
	{
		emit navigateRequested ({}, { page, { 0, 0.4 } });
	}

	void Document::SetSettings ()
	{
		auto setRenderHint = [this] (const QByteArray& option, QPainter::RenderHint hint)
		{
			SetRenderHint (hint, XmlSettingsManager::Instance ().property (option).toBool ());
		};

		setRenderHint ("EnableAntialiasing", QPainter::Antialiasing);
		setRenderHint ("EnableTextAntialiasing", QPainter::TextAntialiasing);
		setRenderHint ("EnableSmoothPixmapTransform", QPainter::SmoothPixmapTransform);
	}
}
}
}
