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

#include "inbandaccountregthirdpage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QtDebug>
#include "inbandaccountregsecondpage.h"
#include "glooxaccountconfigurationwidget.h"

namespace LeechCraft
{
namespace Azoth
{
namespace Xoox
{
	InBandAccountRegThirdPage::InBandAccountRegThirdPage (InBandAccountRegSecondPage *second, QWidget *parent)
	: QWizardPage (parent)
	, SecondPage_ (second)
	, ConfWidget_ (0)
	, StateLabel_ (new QLabel ())
	, RegState_ (RSIdle)
	{
		setLayout (new QVBoxLayout);
		layout ()->addWidget (StateLabel_);

		connect (SecondPage_,
				SIGNAL (successfulReg ()),
				this,
				SLOT (handleSuccessfulReg ()));
		connect (SecondPage_,
				SIGNAL (regError (QString)),
				this,
				SLOT (handleRegError (QString)));
	}

	void InBandAccountRegThirdPage::SetConfWidget (GlooxAccountConfigurationWidget *w)
	{
		ConfWidget_ = w;
	}

	bool InBandAccountRegThirdPage::isComplete () const
	{
		switch (RegState_)
		{
		case RSError:
		case RSIdle:
		case RSAwaitingResult:
			return false;
		case RSSuccess:
			return true;
		default:
			qWarning () << Q_FUNC_INFO
					<< "unknown state"
					<< RegState_;
			return true;
		}
	}

	void InBandAccountRegThirdPage::initializePage ()
	{
		SecondPage_->Register ();

		StateLabel_->setText (tr ("Awaiting registration result..."));
		SetState (RSAwaitingResult);
	}

	void InBandAccountRegThirdPage::SetState (InBandAccountRegThirdPage::RegState state)
	{
		RegState_ = state;
		emit completeChanged ();
	}

	void InBandAccountRegThirdPage::handleSuccessfulReg ()
	{
		StateLabel_->setText (tr ("Registration completed successfully. "
				"You may now further configure account properties."));
		const QString& jid = SecondPage_->GetJID ();
		ConfWidget_->SetJID (jid);
		ConfWidget_->SetNick (jid.split ('@', QString::SkipEmptyParts).value (0));
		SetState (RSSuccess);
	}

	void InBandAccountRegThirdPage::handleRegError (const QString& msg)
	{
		StateLabel_->setText (tr ("Registration failed: %1.").arg (msg));
		SetState (RSError);
	}
}
}
}
