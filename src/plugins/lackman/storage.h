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

#ifndef PLUGINS_LACKMAN_STORAGE_H
#define PLUGINS_LACKMAN_STORAGE_H
#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include "repoinfo.h"

class QUrl;

namespace LeechCraft
{
namespace LackMan
{
	class RepoInfo;
	struct PackageInfo;

	class Storage : public QObject
	{
		Q_OBJECT

		QSqlDatabase DB_;

		QSqlQuery QueryCountPackages_;
		QSqlQuery QueryFindRepo_;
		QSqlQuery QueryAddRepo_;
		QSqlQuery QueryGetRepo_;
		QSqlQuery QueryRemoveRepo_;
		QSqlQuery QueryAddRepoComponent_;
		QSqlQuery QueryGetRepoComponents_;
		QSqlQuery QueryFindComponent_;
		QSqlQuery QueryFindPackage_;
		QSqlQuery QueryGetPackageVersions_;
		QSqlQuery QueryFindInstalledPackage_;
		QSqlQuery QueryAddPackage_;
		QSqlQuery QueryGetPackage_;
		QSqlQuery QueryRemovePackage_;
		QSqlQuery QueryAddPackageSize_;
		QSqlQuery QueryGetPackageSize_;
		QSqlQuery QueryRemovePackageSize_;
		QSqlQuery QueryAddPackageArchiver_;
		QSqlQuery QueryGetPackageArchiver_;
		QSqlQuery QueryRemovePackageArchiver_;
		QSqlQuery QueryHasLocation_;
		QSqlQuery QueryAddLocation_;
		QSqlQuery QueryRemovePackageFromLocation_;
		QSqlQuery QueryClearTags_;
		QSqlQuery QueryAddTag_;
		QSqlQuery QueryClearPackageInfos_;
		QSqlQuery QueryAddPackageInfo_;
		QSqlQuery QueryClearImages_;
		QSqlQuery QueryAddImage_;
		QSqlQuery QueryClearDeps_;
		QSqlQuery QueryAddDep_;
		QSqlQuery QueryGetPackagesInComponent_;
		QSqlQuery QueryGetListPackageInfos_;
		QSqlQuery QueryGetSingleListPackageInfo_;
		QSqlQuery QueryGetPackageTags_;
		QSqlQuery QueryGetInstalledPackages_;
		QSqlQuery QueryGetImages_;
		QSqlQuery QueryGetDependencies_;
		QSqlQuery QueryGetFulfillerCandidates_;
		QSqlQuery QueryGetPackageLocations_;
		QSqlQuery QueryAddToInstalled_;
		QSqlQuery QueryRemoveFromInstalled_;
	public:
		Storage (QObject* = 0);

		int CountPackages (const QUrl& repoUrl);

		QSet<int> GetInstalledPackagesIDs ();
		InstalledDependencyInfoList GetInstalledPackages ();

		int FindRepo (const QUrl& repoUrl);
		int AddRepo (const RepoInfo& ri);
		void RemoveRepo (int);
		RepoInfo GetRepo (int);

		QStringList GetComponents (int repoId);
		int FindComponent (int repoId, const QString& component);
		int AddComponent (int repoId, const QString& component, bool = true);
		void RemoveComponent (int repoId, const QString& component);

		int FindPackage (const QString& name, const QString& version);
		QStringList GetPackageVersions (const QString& name);

		int FindInstalledPackage (int packageId);
		PackageShortInfo GetPackage (int packageId);
		qint64 GetPackageSize (int packageId);
		void RemovePackage (int packageId);
		void AddPackages (const PackageInfo&);

		QMap<int, QList<QString>> GetPackageLocations (int);
		QList<int> GetPackagesInComponent (int);
		QMap<QString, QList<ListPackageInfo>> GetListPackageInfos ();
		QList<Image> GetImages (const QString&);
		ListPackageInfo GetSingleListPackageInfo (int);
		DependencyList GetDependencies (int);
		QList<ListPackageInfo> GetFulfillers (const Dependency&);

		QStringList GetPackageTags (int);
		QStringList GetAllTags ();

		bool HasLocation (int packageId, int componentId);
		void AddLocation (int packageId, int componentId);
		void RemoveLocation (int packageId, int componentId);

		void AddToInstalled (int);
		void RemoveFromInstalled (int);
	private:
		void InitTables ();
		void InitQueries ();
	signals:
		void packageRemoved (int);
	};
}
}

#endif
