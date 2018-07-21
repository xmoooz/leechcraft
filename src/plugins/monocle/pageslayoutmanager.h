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
#include <QVector>
#include "interfaces/monocle/idocument.h"

class QGraphicsScene;

namespace LeechCraft
{
namespace Monocle
{
	enum class LayoutMode;
	enum class ScaleMode;

	class PagesView;
	class SmoothScroller;
	class PageGraphicsItem;

	class PagesLayoutManager : public QObject
	{
		Q_OBJECT

		PagesView * const View_;
		SmoothScroller * const Scroller_;
		QGraphicsScene * const Scene_;

		IDocument_ptr CurrentDoc_;

		QList<PageGraphicsItem*> Pages_;
		QVector<double> PageRotations_;

		LayoutMode LayMode_;

		ScaleMode ScaleMode_;
		double FixedScale_ = 1;

		bool RelayoutScheduled_ = false;

		double HorMargin_ = 0;
		double VertMargin_ = 0;

		double Rotation_ = 0;
	public:
		PagesLayoutManager (PagesView*, SmoothScroller*, QObject* = nullptr);

		void HandleDoc (IDocument_ptr, const QList<PageGraphicsItem*>&);
		const QList<PageGraphicsItem*>& GetPages () const;

		LayoutMode GetLayoutMode () const;
		void SetLayoutMode (LayoutMode);
		int GetLayoutModeCount () const;

		QPoint GetViewportCenter () const;

		int GetCurrentPage () const;
		void SetCurrentPage (int, bool);

		void SetScaleMode (ScaleMode);
		ScaleMode GetScaleMode () const;
		void SetFixedScale (double);
		double GetCurrentScale () const;

		void SetRotation (double);
		void AddRotation (double);
		double GetRotation () const;

		void SetRotation (double, int);
		void AddRotation (double, int);
		double GetRotation (int) const;

		void SetMargins (double horizontal, double vertical);

		void Relayout ();
	private:
		QSizeF GetRotatedSize (int page) const;
	public slots:
		void scheduleSetRotation (double);

		void scheduleRelayout ();
		void handleRelayout ();
	private slots:
		void handlePageSizeChanged (int);
	signals:
		void scheduledRelayoutFinished ();
		void rotationUpdated (double);
		void rotationUpdated (double, int);
		void layoutModeChanged ();
	};
}
}
