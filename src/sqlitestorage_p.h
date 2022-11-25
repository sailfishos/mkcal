/*
  This file is part of the mkcal library.

  Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
  Copyright (c) 2014-2019 Jolla Ltd.
  Copyright (c) 2019 Open Mobile Platform LLC.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#ifndef MKCAL_SQLITESTORAGE_P_H
#define MKCAL_SQLITESTORAGE_P_H

#include "extendedcalendar.h"
#include "extendedstorage.h"
#include "sqliteformat.h"

#include <KCalendarCore/Incidence>

#ifdef Q_OS_UNIX
#include "semaphore_p.h"
#else
#include <QSystemSemaphore>
#endif

#include <QFile>
#include <QFileSystemWatcher>

namespace mKCal {

struct IncidenceId {
    QString uid;
    QDateTime recId;
};

class MKCAL_HIDE SqliteStorageImpl
{
 public:
    SqliteStorageImpl(const QTimeZone &timeZone, const QString &databaseName);
    ~SqliteStorageImpl()
    {
    }

    QTimeZone mTimeZone;
    QString mDatabaseName;
#ifdef Q_OS_UNIX
    ProcessMutex mSem;
#else
    QSystemSemaphore mSem;
#endif

    QFile mChanged;
    QFileSystemWatcher *mWatcher = nullptr;
    int mSavedTransactionId;
    sqlite3 *mDatabase = nullptr;
    SqliteFormat *mFormat = nullptr;
    bool mIsSaved = false;

    bool open();
    bool close();
    bool loadNotebooks(QList<Notebook*> *notebooks, Notebook **defaultNb);
    bool loadNotebook(Notebook **notebook, const QString &notebookUid);
    bool modifyNotebook(const Notebook &nb, DBOperation dbop, bool isDefault);
    bool loadIncidences(QMultiHash<QString, KCalendarCore::Incidence*> *incidences,
                        const DBLoadOperation &dbop);
    int loadIncidences(QMultiHash<QString, KCalendarCore::Incidence*> *incidences,
                       const DBLoadDateLimited &dbop,
                       int limit, QDateTime *last, bool useDate, bool ignoreEnd);
    bool save(const KCalendarCore::Calendar::Ptr &calendar, const ExtendedStorage &storage,
              const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &additions,
              const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &modifications,
              const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &deletions,
              KCalendarCore::Incidence::List *added,
              KCalendarCore::Incidence::List *modified,
              KCalendarCore::Incidence::List *deleted,
              ExtendedStorage::DeleteAction deleteAction);
    bool saveIncidences(const KCalendarCore::Calendar::Ptr &calendar, const ExtendedStorage &storage,
                        const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &list, DBOperation dbop,
                        KCalendarCore::Incidence::List *savedIncidences);
    bool save(const KCalendarCore::MemoryCalendar *calendar,
              const QStringList &toAdd, const QStringList &toUpdate, const QStringList &toDelete,
              QStringList *added, QStringList *modified, QStringList *deleted,
              ExtendedStorage::DeleteAction deleteAction);
    bool saveIncidences(const KCalendarCore::MemoryCalendar *calendar,
                        const QStringList &list, DBOperation dbop,
                        QStringList *savedIncidences);
    sqlite3_stmt* selectInsertedIncidences(const QDateTime &after,
                                           const QString &notebookUid);
    sqlite3_stmt* selectModifiedIncidences(const QDateTime &after,
                                           const QString &notebookUid);
    sqlite3_stmt* selectDeletedIncidences(const QDateTime &after,
                                          const QString &notebookUid);
    sqlite3_stmt* selectAllIncidences(const QString &notebookUid);
    sqlite3_stmt* selectDuplicatedIncidences(const QDateTime &after,
                                             const QString &notebookUid,
                                             const QString &summary);
    bool selectIncidences(KCalendarCore::Incidence::List *list, sqlite3_stmt *stmt);
    int selectCount(const char *query, int qsize);
    QDateTime incidenceDeletedDate(const QString &uid, const QDateTime &recurrenceId);
    bool purgeIncidences(const KCalendarCore::Incidence::List &list);
    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list);
    bool purgeDeletedIncidences(const QList<IncidenceId> &list);
    KCalendarCore::Person::List loadContacts();
    bool saveTimezone();
    bool loadTimezone();

    bool fileChanged();
};

}

#endif
