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

#include <QFileDialog>
#include <interfaces/core/icoreproxy.h>
#include <interfaces/core/itagsmanager.h>
#include "addmultipletorrents.h"
#include "xmlsettingsmanager.h"

namespace LeechCraft
{
namespace BitTorrent
{
	AddMultipleTorrents::AddMultipleTorrents (QWidget *parent)
	: QDialog (parent)
	{
		setupUi (this);
		OpenDirectory_->setText (XmlSettingsManager::Instance ()->
				property ("LastTorrentDirectory").toString ());
		SaveDirectory_->setText (XmlSettingsManager::Instance ()->
				property ("LastSaveDirectory").toString ());
	}

	QString AddMultipleTorrents::GetOpenDirectory () const
	{
		return OpenDirectory_->text ();
	}

	QString AddMultipleTorrents::GetSaveDirectory () const
	{
		return SaveDirectory_->text ();
	}

	Core::AddType AddMultipleTorrents::GetAddType () const
	{
		switch (AddTypeBox_->currentIndex ())
		{
			case 0:
				return Core::Started;
			case 1:
				return Core::Paused;
			default:
				return Core::Started;
		}
	}

	Util::TagsLineEdit* AddMultipleTorrents::GetEdit ()
	{
		return TagsEdit_;
	}

	QStringList AddMultipleTorrents::GetTags () const
	{
		auto tm = Core::Instance ()->GetProxy ()->GetTagsManager ();
		QStringList result;
		Q_FOREACH (const auto& tag, tm->Split (TagsEdit_->text ()))
			result << tm->GetID (tag);
		return result;
	}

	void AddMultipleTorrents::on_BrowseOpen__released ()
	{
		QString dir = QFileDialog::getExistingDirectory (this,
				tr ("Select directory with torrents"),
				OpenDirectory_->text ());
		if (dir.isEmpty ())
			return;

		XmlSettingsManager::Instance ()->setProperty ("LastTorrentDirectory", dir);
		OpenDirectory_->setText (dir);
	}

	void AddMultipleTorrents::on_BrowseSave__released ()
	{
		QString dir = QFileDialog::getExistingDirectory (this,
				tr ("Select save directory"),
				SaveDirectory_->text ());
		if (dir.isEmpty ())
			return;

		XmlSettingsManager::Instance ()->setProperty ("LastSaveDirectory", dir);
		SaveDirectory_->setText (dir);
	}
}
}
