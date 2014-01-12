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

#include "playlistdelegate.h"
#include <QTreeView>
#include <QPainter>
#include <QApplication>
#include <util/util.h>
#include "player.h"
#include "mediainfo.h"
#include "core.h"

namespace LeechCraft
{
namespace LMP
{
	const int Padding = 2;

	PlaylistDelegate::PlaylistDelegate (QTreeView *view, QObject *parent)
	: QStyledItemDelegate (parent)
	, View_ (view)
	{
	}

	void PlaylistDelegate::paint (QPainter *painter,
			const QStyleOptionViewItem& optionOld, const QModelIndex& index) const
	{
		const bool isAlbum = index.data (Player::Role::IsAlbum).toBool ();

		QStyleOptionViewItemV4 option = optionOld;
		auto& pal = option.palette;
		if (!(option.features & QStyleOptionViewItemV4::Alternate))
		{
			QLinearGradient grad (0, 0, option.rect.width (), 0);
			grad.setColorAt (0, pal.color (QPalette::Window).darker (105));
			grad.setColorAt (0.5, pal.color (QPalette::Window).darker (120));
			grad.setColorAt (1, pal.color (QPalette::Window).darker (105));
			option.backgroundBrush = QBrush (grad);
		}

		if (isAlbum)
			PaintAlbum (painter, option, index);
		else
			PaintTrack (painter, option, index);
	}

	QSize PlaylistDelegate::sizeHint (const QStyleOptionViewItem& option, const QModelIndex& index) const
	{
		if (!index.data (Player::Role::IsAlbum).toBool ())
			return QStyledItemDelegate::sizeHint (option, index);

		const auto& info = index.data (Player::Role::Info).value<MediaInfo> ();
		const int width = View_->viewport ()->size ().width () - 4;

		const QFont& font = option.font;
		QFont bold = font;
		bold.setBold (true);
		const auto boldFM = QFontMetrics (bold);
		QFont italic = font;
		italic.setItalic (true);
		const auto italicFM = QFontMetrics (italic);
		QFont boldItalic = bold;
		boldItalic.setItalic (true);
		const auto boldItalicFM = QFontMetrics (boldItalic);

		const int height = Padding * 2 +
				boldFM.boundingRect (info.Album_).height () +
				boldItalicFM.boundingRect (info.Artist_).height () +
				italicFM.boundingRect (info.Genres_.join (" / ")).height () +
				option.fontMetrics.boundingRect (QString::number (info.Year_)).height ();

		return QSize (width, std::max (height, 32));
	}

	void PlaylistDelegate::PaintTrack (QPainter *painter,
			QStyleOptionViewItemV4 option, const QModelIndex& index) const
	{
		const auto& info = index.data (Player::Role::Info).value<MediaInfo> ();

		QStyle *style = option.widget ?
				option.widget->style () :
				QApplication::style ();

		const bool isSubAlbum = index.parent ().isValid ();

		QStyleOptionViewItemV4 bgOpt = option;

		painter->save ();

		if (index.data (Player::Role::IsCurrent).toBool ())
		{
			const QColor& highlight = bgOpt.palette.color (QPalette::Highlight);
			QLinearGradient grad (0, 0, 0, bgOpt.rect.height ());
			grad.setColorAt (0, highlight.lighter (100));
			grad.setColorAt (1, highlight.darker (200));
			bgOpt.backgroundBrush = QBrush (grad);

			QFont font = option.font;
			font.setItalic (true);
			painter->setFont (font);
		}

		const bool drawSelected = index.data (Player::Role::IsCurrent).toBool () ||
				option.state & QStyle::State_Selected;
		if (drawSelected)
			painter->setPen (bgOpt.palette.color (QPalette::HighlightedText));

		style->drawPrimitive (QStyle::PE_PanelItemViewItem, &bgOpt, painter, option.widget);

		const auto& oneShotPosVar = index.data (Player::Role::OneShotPos);
		if (oneShotPosVar.isValid ())
		{
			const auto& text = QString::number (oneShotPosVar.toInt () + 1);
			const auto& textWidth = option.fontMetrics.width (text);

			auto oneShotRect = option.rect;
			oneShotRect.setWidth (std::max (textWidth + 2 * Padding, oneShotRect.height ()));

			painter->save ();
			painter->setRenderHint (QPainter::Antialiasing);
			painter->setRenderHint (QPainter::HighQualityAntialiasing);
			painter->setBrush (option.palette.brush (drawSelected ?
						QPalette::Highlight :
						QPalette::Button));
			painter->setPen (option.palette.color (QPalette::ButtonText));
			painter->drawEllipse (oneShotRect);
			painter->restore ();

			style->drawItemText (painter,
					oneShotRect,
					Qt::AlignCenter,
					option.palette,
					true,
					text);

			option.rect.adjust (oneShotRect.width () + Padding, 0, 0, 0);
		}

		if (index.data (Player::Role::IsStop).toBool ())
		{
			const auto& icon = Core::Instance ().GetProxy ()->GetIcon ("media-playback-stop");
			const auto& px = icon.pixmap (option.rect.size ());
			style->drawItemPixmap (painter, option.rect, Qt::AlignLeft | Qt::AlignVCenter, px);

			option.rect.adjust (px.width () + Padding, 0, 0, 0);
		}

		QString lengthText = Util::MakeTimeFromLong (info.Length_);
		if (lengthText.startsWith ("00:"))
			lengthText = lengthText.mid (3);
		const int width = option.fontMetrics.width (lengthText);
		style->drawItemText (painter, option.rect,
				Qt::AlignRight, option.palette, true, lengthText);

		const auto& itemTextRect = option.rect.adjusted (0, 0, -width, 0);
		QString itemStr;
		if (!isSubAlbum)
			itemStr = index.data ().toString ();
		else
		{
			if (info.TrackNumber_ > 0 && !info.Title_.isEmpty ())
				itemStr = QString::fromUtf8 ("%1 — %2").arg (info.TrackNumber_).arg (info.Title_);
			else if (!info.Title_.isEmpty ())
				itemStr = info.Title_;
			else
				itemStr = QFileInfo (info.LocalPath_).fileName ();
		}
		itemStr = option.fontMetrics.elidedText (itemStr, Qt::ElideRight, itemTextRect.width ());

		style->drawItemText (painter, itemTextRect, 0, option.palette, true, itemStr);
		painter->restore ();
	}

	void PlaylistDelegate::PaintAlbum (QPainter *painter,
			QStyleOptionViewItemV4 option, const QModelIndex& index) const
	{
		const auto& info = index.data (Player::Role::Info).value<MediaInfo> ();

		QStyle *style = option.widget ?
				option.widget->style () :
				QApplication::style ();

		style->drawPrimitive (QStyle::PE_PanelItemViewItem, &option, painter, option.widget);
		const int maxIconHeight = option.rect.height () - Padding * 2;
		QPixmap px = index.data (Player::Role::AlbumArt).value<QPixmap> ();
		px = px.scaled (maxIconHeight, maxIconHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		painter->drawPixmap (option.rect.left () + Padding, option.rect.top () + Padding, px);

		const QFont& font = option.font;
		QFont bold = font;
		bold.setBold (true);
		QFont italic = font;
		italic.setItalic (true);
		QFont boldItalic = bold;
		boldItalic.setItalic (true);

		int x = option.rect.left () + maxIconHeight + 3 * Padding;
		int y = option.rect.top ();
		painter->save ();

		if (option.state & QStyle::State_Selected)
			painter->setPen (option.palette.color (QPalette::HighlightedText));

		auto append = [&x, &y, painter] (const QString& text, const QFont& font)
		{
			painter->setFont (font);
			y += QFontMetrics (font).boundingRect (text).height ();
			painter->drawText (x, y, text);
		};
		append (info.Album_, bold);
		append (info.Artist_, boldItalic);
		append (info.Genres_.join (" / "), italic);
		append (QString::number (info.Year_), font);

		const int length = index.data (Player::Role::AlbumLength).toInt ();
		auto lengthStr = Util::MakeTimeFromLong (length);
		if (lengthStr.startsWith ("00:"))
			lengthStr = lengthStr.mid (3);
		painter->drawText (option.rect.adjusted (Padding, Padding, -Padding, -Padding), Qt::AlignRight, lengthStr);

		painter->restore ();
	}
}
}
