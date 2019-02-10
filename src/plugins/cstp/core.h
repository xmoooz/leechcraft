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

#include <vector>
#include <memory>
#include <QAbstractItemModel>
#include <QStringList>
#include <QNetworkProxy>
#include <QNetworkAccessManager>
#include <QSet>
#include <QUrl>
#include <util/sll/eitherfwd.h>
#include <interfaces/iinfo.h>
#include <interfaces/structures.h>
#include <interfaces/idownload.h>

class QFile;
class QToolBar;

struct EntityTestHandleResult;

namespace LeechCraft
{
namespace CSTP
{
	class Task;

	class Core : public QAbstractItemModel
	{
		Q_OBJECT

		const QStringList Headers_;

		struct TaskDescr
		{
			std::shared_ptr<Task> Task_;
			std::shared_ptr<QFile> File_;
			QString Comment_;
			bool ErrorFlag_;
			LeechCraft::TaskParameters Parameters_;
			QStringList Tags_;
		};
		typedef std::vector<TaskDescr> tasks_t;
		tasks_t ActiveTasks_;
		bool SaveScheduled_ = false;
		QNetworkAccessManager *NetworkAccessManager_ = nullptr;
		QToolBar *Toolbar_ = nullptr;
		QSet<QNetworkReply*> FinishedReplies_;
		QModelIndex Selected_;
		ICoreProxy_ptr CoreProxy_;

		explicit Core ();
	public:
		enum
		{
			HURL
			, HState
			, HProgress
			, HSpeed
			, HRemaining
			, HDownloading
		};

		enum class FileExistsBehaviour
		{
			Remove,
			Abort,
			Continue
		};

		static Core& Instance ();
		void Release ();
		void SetCoreProxy (ICoreProxy_ptr);
		ICoreProxy_ptr GetCoreProxy () const;
		void SetToolbar (QToolBar*);
		void ItemSelected (const QModelIndex&);

		QFuture<IDownload::Result> AddTask (const Entity&);
		qint64 GetTotalDownloadSpeed () const;
		EntityTestHandleResult CouldDownload (const LeechCraft::Entity&);
		QAbstractItemModel* GetRepresentationModel ();
		QNetworkAccessManager* GetNetworkAccessManager () const;
		bool HasFinishedReply (QNetworkReply*) const;
		void RemoveFinishedReply (QNetworkReply*);

		int columnCount (const QModelIndex& = QModelIndex ()) const override;
		QVariant data (const QModelIndex&, int = Qt::DisplayRole) const override;
		Qt::ItemFlags flags (const QModelIndex&) const override;
		bool hasChildren (const QModelIndex&) const override;
		QVariant headerData (int, Qt::Orientation, int = Qt::DisplayRole) const override;
		QModelIndex index (int, int, const QModelIndex& = QModelIndex()) const override;
		QModelIndex parent (const QModelIndex&) const override;
		int rowCount (const QModelIndex& = QModelIndex ()) const override;
	public slots:
		void removeTriggered (int = -1);
		void removeAllTriggered ();
		void startTriggered (int = -1);
		void stopTriggered (int = -1);
		void startAllTriggered ();
		void stopAllTriggered ();
	private slots:
		void done (bool);
		void updateInterface ();
		void writeSettings ();
		void finishedReply (QNetworkReply*);
	private:
		QFuture<IDownload::Result> AddTask (const QUrl&,
				const QString&,
				const QString&,
				const QString&,
				const QStringList&,
				const QVariantMap&,
				LeechCraft::TaskParameters = LeechCraft::NoParameters);
		QFuture<IDownload::Result> AddTask (QNetworkReply*,
				const QString&,
				const QString&,
				const QString&,
				const QStringList&,
				LeechCraft::TaskParameters = LeechCraft::NoParameters);
		QFuture<IDownload::Result> AddTask (TaskDescr&);
		void ReadSettings ();
		void ScheduleSave ();
		tasks_t::const_iterator FindTask (QObject*) const;
		tasks_t::iterator FindTask (QObject*);
		void Remove (tasks_t::iterator);
		void AddToHistory (tasks_t::const_iterator);
		tasks_t::const_reference TaskAt (int) const;
		tasks_t::reference TaskAt (int);
	signals:
		void error (const QString&);
		void fileExists (FileExistsBehaviour*);
	};
}
}
