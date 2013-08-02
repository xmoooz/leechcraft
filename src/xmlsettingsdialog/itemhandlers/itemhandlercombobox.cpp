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

#include "itemhandlercombobox.h"
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QtDebug>
#include "../scripter.h"
#include "../itemhandlerfactory.h"

namespace LeechCraft
{
	ItemHandlerCombobox::ItemHandlerCombobox (ItemHandlerFactory *factory)
	: Factory_ (factory)
	{
	}

	ItemHandlerCombobox::~ItemHandlerCombobox ()
	{
	}

	bool ItemHandlerCombobox::CanHandle (const QDomElement& element) const
	{
		return element.attribute ("type") == "combobox";
	}

	void ItemHandlerCombobox::Handle (const QDomElement& item, QWidget *pwidget)
	{
		QGridLayout *lay = qobject_cast<QGridLayout*> (pwidget->layout ());

		QHBoxLayout *hboxLay = new QHBoxLayout;
		QComboBox *box = new QComboBox (XSD_);

		hboxLay->addWidget (box);

		XSD_->SetTooltip (box, item);
		box->setObjectName (item.attribute ("property"));
		box->setSizeAdjustPolicy (QComboBox::AdjustToContents);
		if (item.hasAttribute ("maxVisibleItems"))
			box->setMaxVisibleItems (item.attribute ("maxVisibleItems").toInt ());

		bool mayHaveDataSource = item.hasAttribute ("mayHaveDataSource") &&
				item.attribute ("mayHaveDataSource").toLower () == "true";
		if (mayHaveDataSource)
		{
			const QString& prop = item.attribute ("property");
			Factory_->RegisterDatasourceSetter (prop,
					[this] (const QString& str, QAbstractItemModel *m, Util::XmlSettingsDialog *xsd)
						{ SetDataSource (str, m, xsd); });
			Propname2Combobox_ [prop] = box;
			Propname2Item_ [prop] = item;
		}

		hboxLay->addStretch ();

		if (item.hasAttribute ("moreThisStuff"))
		{
			QPushButton *moreButt = new QPushButton (tr ("More stuff..."));
			hboxLay->addWidget (moreButt);

			moreButt->setObjectName (item.attribute ("moreThisStuff"));
			connect (moreButt,
					SIGNAL (released ()),
					XSD_,
					SLOT (handleMoreThisStuffRequested ()));
		}

		QDomElement option = item.firstChildElement ("option");
		while (!option.isNull ())
		{
			const auto& images = XSD_->GetImages (option);
			if (!images.isEmpty ())
			{
				const QIcon icon (QPixmap::fromImage (images.at (0)));
				box->addItem (icon,
						XSD_->GetLabel (option),
						option.attribute ("name"));
			}
			else
				box->addItem (XSD_->GetLabel (option),
						option.attribute ("name"));

			auto setColor = [&option, box] (const QString& attr, Qt::ItemDataRole role) -> void
			{
				if (option.hasAttribute (attr))
				{
					const QColor color (option.attribute (attr));
					box->setItemData (box->count () - 1, color, role);
				}
			};
			setColor ("color", Qt::ForegroundRole);
			setColor ("bgcolor", Qt::BackgroundRole);

			option = option.nextSiblingElement ("option");
		}

		connect (box,
				SIGNAL (currentIndexChanged (int)),
				this,
				SLOT (updatePreferences ()));

		QDomElement scriptContainer = item.firstChildElement ("scripts");
		if (!scriptContainer.isNull ())
		{
			Scripter scripter (scriptContainer);

			QStringList fromScript = scripter.GetOptions ();
			Q_FOREACH (const QString& elm, scripter.GetOptions ())
				box->addItem (scripter.HumanReadableOption (elm),
						elm);
		}

		int pos = box->findData (XSD_->GetValue (item));
		if (pos != -1)
			box->setCurrentIndex (pos);
		else if (!mayHaveDataSource)
			qWarning () << Q_FUNC_INFO
				<< box
				<< XSD_->GetValue (item)
				<< "not found (and this item may not have a datasource)";

		QLabel *label = new QLabel (XSD_->GetLabel (item));
		label->setWordWrap (false);

		box->setProperty ("ItemHandler", QVariant::fromValue<QObject*> (this));
		box->setProperty ("SearchTerms", label->text ());

		int row = lay->rowCount ();
		lay->addWidget (label, row, 0, Qt::AlignRight);
		lay->addLayout (hboxLay, row, 1);
	}

	void ItemHandlerCombobox::SetValue (QWidget *widget, const QVariant& value) const
	{
		QComboBox *combobox = qobject_cast<QComboBox*> (widget);
		if (!combobox)
		{
			qWarning () << Q_FUNC_INFO
				<< "not a QComboBox"
				<< widget;
			return;
		}

		int pos = combobox->findData (value);
		if (pos == -1)
		{
			QString text = value.toString ();
			if (!text.isNull ())
				pos = combobox->findText (text);
		}

		if (pos != -1)
			combobox->setCurrentIndex (pos);
		else
			qWarning () << Q_FUNC_INFO
				<< combobox
				<< value
				<< "not found";
	}

	QVariant ItemHandlerCombobox::GetObjectValue (QObject *object) const
	{
		QComboBox *combobox = qobject_cast<QComboBox*> (object);
		if (!combobox)
		{
			qWarning () << Q_FUNC_INFO
				<< "not a QComboBox"
				<< object;
			return QVariant ();
		}
		QVariant result = combobox->itemData (combobox->currentIndex ());
		if (result.isNull ())
			result = combobox->currentText ();
		return result;
	}

	void ItemHandlerCombobox::SetDataSource (const QString& prop, QAbstractItemModel *model, Util::XmlSettingsDialog *xsd)
	{
		QComboBox *box = Propname2Combobox_ [prop];
		if (!box)
		{
			qWarning () << Q_FUNC_INFO
					<< "combobox for property"
					<< prop
					<< "not found";
			return;
		}

		box->setModel (model);

		const QVariant& data = xsd->GetValue (Propname2Item_ [prop]);
		int pos = box->findData (data);
		if (pos == -1)
		{
			QString text = data.toString ();
			if (!text.isNull ())
				pos = box->findText (text);
		}

		if (pos != -1)
			box->setCurrentIndex (pos);
		else
			qWarning () << Q_FUNC_INFO
				<< box
				<< data
				<< "not found";

		ChangedProperties_.remove (prop);
	}
}
