/**********************************************************************
 * LeechCraft - modular cross-platform feature rich internet client.
 * Copyright (C) 2006-2013  Georg Rudoy
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

#include "palettefixerfilter.h"
#include <QWidget>
#include <QEvent>
#include <QApplication>

namespace LeechCraft
{
namespace LMP
{
	PaletteFixerFilter::PaletteFixerFilter (QWidget *parent)
	: QObject (parent)
	, W_ (parent)
	, IsChanging_ (false)
	{
		UpdatePalette (W_->palette ());
		parent->installEventFilter (this);
	}

	void PaletteFixerFilter::UpdatePalette (QPalette pal)
	{
		pal.setColor (QPalette::Base, pal.color (QPalette::Window));
		pal.setColor (QPalette::AlternateBase, pal.color (QPalette::Window));
		pal.setColor (QPalette::Text, pal.color (QPalette::WindowText));
		IsChanging_ = true;
		W_->setPalette (pal);
		IsChanging_ = false;
	}

	bool PaletteFixerFilter::eventFilter (QObject*, QEvent *e)
	{
		if (IsChanging_)
			return false;

		QPalette palette;
		if (e->type () == QEvent::ApplicationPaletteChange ||
				e->type () == QEvent::PaletteChange)
			palette = qApp->palette ();
		else
			return false;

		UpdatePalette (palette);

		return false;
	}
}
}
