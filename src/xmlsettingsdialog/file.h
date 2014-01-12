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

#ifndef XMLSETTINGSDIALOG_FILE_H
#define XMLSETTINGSDIALOG_FILE_H
#include <memory>
#include <QScriptEngine>
#include <QFile>
#include "bytearray.h"

namespace LeechCraft
{
	class File : public QObject
	{
		Q_OBJECT

		std::auto_ptr<QFile> Imp_;
	public:
		File (QObject* = 0);
		File (const File&);
		virtual ~File ();
	public slots:
		bool atEnd () const;
		qint64 bytesAvailable () const;
		qint64 bytesToWrite () const;
		bool canReadLine () const;
		void close ();
		bool copy (const QString&);
		QFile::FileError error () const;
		QString errorString () const;
		bool exists () const;
		QString fileName () const;
		bool flush ();
		int handle () const;
		bool isOpen () const;
		bool isReadable () const;
		bool isSequential () const;
		bool isTextModeEnabled () const;
		bool isWritable () const;
		bool link (const QString&);
		bool open (QIODevice::OpenMode = QIODevice::ReadOnly);
		QFile::OpenMode openMode () const;
		ByteArray peek (qint64);
		QFile::Permissions permissions () const;
		qint64 pos () const;
		bool putChar (char);
		ByteArray read (qint64);
		ByteArray readAll ();
		ByteArray readLine (qint64);
		bool remove ();
		bool rename (const QString&);
		bool reset ();
		bool resize (qint64);
		bool seek (qint64);
		void setFileName (const QString&);
		bool setPermissions (QFile::Permissions);
		void setTextModeEnabled (bool);
		qint64 size () const;
		QString symLinkTarget () const;
		void unsetError ();
		bool waitForBytesWritten (int);
		bool waitForReadyRead (int);
		qint64 write (const ByteArray&);
	};
};

Q_DECLARE_METATYPE (LeechCraft::File);
Q_DECLARE_METATYPE (QIODevice::OpenMode);
Q_SCRIPT_DECLARE_QMETAOBJECT (LeechCraft::File, QObject*);

QScriptValue toScriptValue (QScriptEngine*, const QIODevice::OpenMode&);
void fromScriptValue (const QScriptValue&, QIODevice::OpenMode&);

#endif

