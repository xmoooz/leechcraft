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

#include <functional>
#include <memory>
#include <QGraphicsPixmapItem>
#include <QPointer>
#include "interfaces/monocle/idocument.h"

template<typename T>
class QFutureWatcher;

namespace LeechCraft
{
namespace Monocle
{
	class PagesLayoutManager;
	class ArbitraryRotationWidget;

	class PageGraphicsItem : public QObject
						   , public QGraphicsPixmapItem
	{
		Q_OBJECT

		bool IsRenderingEnabled_ = true;

		IDocument_ptr Doc_;
		const int PageNum_;

		qreal XScale_ = 1;
		qreal YScale_ = 1;

		bool Invalid_ = true;

		std::function<void (int, QPointF)> ReleaseHandler_;

		PagesLayoutManager *LayoutManager_ = nullptr;

		QPointer<ArbitraryRotationWidget> ArbWidget_;
	public:
		typedef std::function<void (QRectF)> RectSetter_f;
	private:
		struct RectInfo
		{
			QRectF DocRect_;
			RectSetter_f Setter_;
		};
		QMap<QGraphicsItem*, RectInfo> Item2RectInfo_;
	public:
		PageGraphicsItem (IDocument_ptr, int, QGraphicsItem* = nullptr);
		~PageGraphicsItem ();

		void SetLayoutManager (PagesLayoutManager*);

		void SetReleaseHandler (std::function<void (int, QPointF)>);

		void SetScale (double, double);
		int GetPageNum () const;

		QRectF MapFromDoc (const QRectF&) const;
		QRectF MapToDoc (const QRectF&) const;

		void RegisterChildRect (QGraphicsItem*, const QRectF&, RectSetter_f);
		void UnregisterChildRect (QGraphicsItem*);

		void ClearPixmap ();
		void UpdatePixmap ();

		bool IsDisplayed () const;

		void SetRenderingEnabled (bool);

		QRectF boundingRect () const;
		QPainterPath shape () const;
	protected:
		void paint (QPainter*, const QStyleOptionGraphicsItem*, QWidget*);
		void mousePressEvent (QGraphicsSceneMouseEvent*);
		void mouseReleaseEvent (QGraphicsSceneMouseEvent*);
		void contextMenuEvent (QGraphicsSceneContextMenuEvent*);
	private:
		bool ShouldRender () const;
		QPixmap GetEmptyPixmap (bool fill) const;
	private slots:
		void rotateCCW ();
		void rotateCW ();
		void requestRotation (double);

		void updateRotation (double, int);
	signals:
		void rotateRequested (double);
	};
}
}
