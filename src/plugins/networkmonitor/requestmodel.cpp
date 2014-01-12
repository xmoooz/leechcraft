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

#include "requestmodel.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTextCodec>
#include <QDateTime>
#include <QtDebug>
#include "headermodel.h"

Q_DECLARE_METATYPE (QNetworkReply*);

using namespace LeechCraft::Plugins::NetworkMonitor;

LeechCraft::Plugins::NetworkMonitor::RequestModel::RequestModel (QObject *parent)
: QStandardItemModel (parent)
, Clear_ (true)
{
	setHorizontalHeaderLabels (QStringList (tr ("Date started"))
			<< tr ("Date finished")
			<< tr ("Type")
			<< tr ("Host"));

	RequestHeadersModel_ = new HeaderModel (this);
	ReplyHeadersModel_ = new HeaderModel (this);
}

HeaderModel* RequestModel::GetRequestHeadersModel () const
{
	return RequestHeadersModel_;
}

HeaderModel* RequestModel::GetReplyHeadersModel () const
{
	return ReplyHeadersModel_;
}

void LeechCraft::Plugins::NetworkMonitor::RequestModel::handleRequest (QNetworkAccessManager::Operation op,
		const QNetworkRequest& req, QNetworkReply *rep)
{
	if (rep->isFinished ())
	{
		qWarning () << Q_FUNC_INFO
			<< "skipping the finished reply"
			<< rep;
		return;
	}
	QList<QStandardItem*> items;
	QString opName;
	switch (op)
	{
		case QNetworkAccessManager::HeadOperation:
			opName = "HEAD";
			break;
		case QNetworkAccessManager::GetOperation:
			opName = "GET";
			break;
		case QNetworkAccessManager::PutOperation:
			opName = "PUT";
			break;
		case QNetworkAccessManager::PostOperation:
			opName = "POST";
			break;
		case QNetworkAccessManager::DeleteOperation:
			opName = "DELETE";
			break;
		case QNetworkAccessManager::UnknownOperation:
			opName = "Unknown";
			break;
#if QT_VERSION >= 0x040700
		case QNetworkAccessManager::CustomOperation:
			opName = "Custom";
			break;
#endif
	}
	items.push_back (new QStandardItem (QDateTime::currentDateTime ().toString ()));
	items.push_back (new QStandardItem (tr ("In progress")));
	items.push_back (new QStandardItem (opName));
	items.push_back (new QStandardItem (req.url ().toString ()));
	items.first ()->setData (QVariant::fromValue<QNetworkReply*> (rep));
	appendRow (items);

	connect (rep,
			SIGNAL (error (QNetworkReply::NetworkError )),
			this,
			SLOT (handleFinished ()));
	connect (rep,
			SIGNAL (finished ()),
			this,
			SLOT (handleFinished ()));
	connect (rep,
			SIGNAL (destroyed (QObject*)),
			this,
			SLOT (handleGonnaDestroy (QObject*)));
}

namespace
{
	template<typename T>
	QMap<QString, QVariant> GetHeaders (const T* object)
	{
		QMap<QString, QVariant> result;
		QList<QByteArray> headers = object->rawHeaderList ();
		QTextCodec *codec = QTextCodec::codecForName ("UTF-8");
		Q_FOREACH (QByteArray header, headers)
			result [codec->toUnicode (header)] = codec->toUnicode (object->rawHeader (header));
		return result;
	}

	template<typename T>
	void FeedHeaders (T object, HeaderModel* model)
	{
		QMap<QString, QVariant> headers = GetHeaders (object);
		Q_FOREACH (QString header, headers.keys ())
			model->AddHeader (header, headers [header].toString ());
	}

	template<>
	void FeedHeaders (QMap<QString, QVariant> headers, HeaderModel* model)
	{
		Q_FOREACH (QString header, headers.keys ())
			model->AddHeader (header, headers [header].toString ());
	}
}

void RequestModel::handleFinished ()
{
	QNetworkReply *reply = qobject_cast<QNetworkReply*> (sender ());
	if (!reply)
	{
		qWarning () << Q_FUNC_INFO
			<< sender ()
			<< "not found";
		return;
	}

	for (int i = 0; i < rowCount (); ++i)
	{
		QStandardItem *ci = item (i);
		if (ci->data ().value<QNetworkReply*> () == reply)
		{
			if (Clear_)
				qDeleteAll (takeRow (i));
			else
			{
				item (i, 1)->setText (QDateTime::currentDateTime ().toString ());
				item (i, 0)->setData (0);
				QNetworkRequest r = reply->request ();
				item (i, 1)->setData (GetHeaders (&r));

				auto headers = GetHeaders (reply);
				headers ["[HTTP response]"] = QString ("%1 (%2)")
						.arg (reply->attribute (QNetworkRequest::HttpStatusCodeAttribute).toInt ())
						.arg (reply->attribute (QNetworkRequest::HttpReasonPhraseAttribute).toString ());
				item (i, 2)->setData (headers);
			}
			break;
		}
	}
}

void RequestModel::setClear (bool clear)
{
	Clear_ = clear;
	if (Clear_)
	{
		for (int i = rowCount () - 1; i >= 0; --i)
			if (!item (i)->data ().value<QNetworkReply*> ())
				qDeleteAll (takeRow (i));
		handleCurrentChanged (QModelIndex ());
	}
}

void RequestModel::handleCurrentChanged (const QModelIndex& newItem)
{
	RequestHeadersModel_->clear ();
	ReplyHeadersModel_->clear ();

	if (!newItem.isValid ())
		return;

	QNetworkReply *reply = item (itemFromIndex (newItem)->row (), 0)->
		data ().value<QNetworkReply*> ();
	if (reply)
	{
		QNetworkRequest r = reply->request ();
		FeedHeaders (&r, RequestHeadersModel_);
		FeedHeaders (reply, ReplyHeadersModel_);
	}
	else
	{
		FeedHeaders (item (itemFromIndex (newItem)->row (), 1)->
				data ().toMap (), RequestHeadersModel_);
		FeedHeaders (item (itemFromIndex (newItem)->row (), 2)->
				data ().toMap (), ReplyHeadersModel_);
	}
}

void RequestModel::handleGonnaDestroy (QObject *obj)
{
	if (!obj && sender ())
		obj = sender ();

	for (int i = 0; i < rowCount (); ++i)
	{
		QStandardItem *ci = item (i);
		if (ci->data ().value<QNetworkReply*> () == obj)
		{
			qDeleteAll (takeRow (i));
			break;
		}
	}
}

