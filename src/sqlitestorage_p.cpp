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
/**
  @file
  This file is part of the API for handling calendar data and
  defines the SqliteStorage class.

  @brief
  This class provides a calendar storage as an sqlite database.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
*/
#include "sqlitestorage_p.h"
#include "sqliteformat.h"
#include "logging_p.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>
using namespace KCalendarCore;

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

using namespace mKCal;

static const QString gChanged(QLatin1String(".changed"));

// QDir::isReadable() doesn't support group permissions, only user permissions.
static bool directoryIsRW(const QString &dirPath)
{
    QFileInfo databaseDirInfo(dirPath);
    return (databaseDirInfo.permission(QFile::ReadGroup | QFile::WriteGroup)
            || databaseDirInfo.permission(QFile::ReadUser  | QFile::WriteUser));
}

static QString defaultLocation()
{
    // Environment variable is taking precedence.
    QString dbFile = QLatin1String(qgetenv("SQLITESTORAGEDB"));
    if (dbFile.isEmpty()) {
        // Otherwise, use a central storage location by default
        const QString privilegedDataDir = QString("%1/.local/share/system/privileged/").arg(QDir::homePath());

        QDir databaseDir(privilegedDataDir);
        if (databaseDir.exists() && directoryIsRW(privilegedDataDir)) {
            databaseDir = privilegedDataDir + QLatin1String("Calendar/mkcal/");
        } else {
            databaseDir = QString("%1/.local/share/system/Calendar/mkcal/").arg(QDir::homePath());
        }

        if (!databaseDir.exists() && !databaseDir.mkpath(QString::fromLatin1("."))) {
            qCWarning(lcMkcal) << "Unable to create calendar database directory:" << databaseDir.path();
        }

        dbFile = databaseDir.absoluteFilePath(QLatin1String("db"));
    }

    return dbFile;
}

SqliteStorageImpl::SqliteStorageImpl(const QTimeZone &timeZone,
                                     const QString &databaseName)
    : mTimeZone(timeZone),
      mDatabaseName(databaseName.isEmpty() ? defaultLocation() : databaseName),
#ifdef Q_OS_UNIX
      mSem(mDatabaseName),
#else
      mSem(mDatabaseName, 1, QSystemSemaphore::Open),
#endif
      mChanged(mDatabaseName + gChanged)
{
}

bool SqliteStorageImpl::open()
{
    int rv;

    if (mDatabase) {
        return false;
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    rv = sqlite3_open(mDatabaseName.toUtf8(), &mDatabase);
    if (rv) {
        qCWarning(lcMkcal) << "sqlite3_open error:" << rv << "on database" << mDatabaseName;
        qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase);
        goto error;
    }
    qCDebug(lcMkcal) << "database" << mDatabaseName << "opened";

    mFormat = new SqliteFormat(mDatabase, mTimeZone);
    if (!mFormat->init()) {
        goto error;
    }
    mFormat->selectMetadata(&mSavedTransactionId);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    if (!mChanged.open(QIODevice::Append)) {
        qCWarning(lcMkcal) << "cannot open changed file for" << mDatabaseName;
        goto error;
    }
    mWatcher = new QFileSystemWatcher();
    mWatcher->addPath(mChanged.fileName());

    if (!loadTimezone()) {
        qCWarning(lcMkcal) << "cannot load timezones from calendar";
        return false;
    }

    return true;

error:
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    close();
    return false;
}

bool SqliteStorageImpl::close()
{
    if (mDatabase) {
        if (mWatcher) {
            mWatcher->removePaths(mWatcher->files());
            // This should work, as storage should be closed before
            // application terminates now. If not, deadlock occurs.
            delete mWatcher;
            mWatcher = NULL;
        }
        mChanged.close();
        delete mFormat;
        mFormat = 0;
        sqlite3_close(mDatabase);
        mDatabase = 0;
    }
    return true;
}

static bool isContaining(const QMultiHash<QString, Incidence*> &list, const Incidence *incidence)
{
    QMultiHash<QString, Incidence*>::ConstIterator it = list.find(incidence->uid());
    for (; it != list.constEnd(); ++it) {
        if ((*it)->recurrenceId() == incidence->recurrenceId()) {
            return true;
        }
    }
    return false;
}

bool SqliteStorageImpl::loadIncidences(QMultiHash<QString, Incidence*> *incidences,
                                       const DBLoadOperation &dbop)
{
    sqlite3_stmt* stmt1 = mFormat->loadOperationToSQL(dbop);
    if (!stmt1) {
        return false;
    }

    if (!mSem.acquire()) {
        sqlite3_finalize(stmt1);
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    QString notebookUid;
    Incidence* incidence;
    bool wasEmpty = incidences->count() == 0;
    while ((incidence = mFormat->selectComponents(stmt1, notebookUid))) {
        if (wasEmpty || !isContaining(*incidences, incidence)) {
            incidences->insert(notebookUid, incidence);
        } else {
            delete incidence;
        }
    }

    sqlite3_finalize(stmt1);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;
}

int SqliteStorageImpl::loadIncidences(QMultiHash<QString, Incidence*> *incidences,
                                       const DBLoadDateLimited &dbop,
                                       int limit, QDateTime *last,
                                       bool useDate,
                                       bool ignoreEnd)
{
    sqlite3_stmt* stmt1 = mFormat->loadOperationToSQL(dbop);
    if (!stmt1) {
        return -1;
    }

    if (!mSem.acquire()) {
        sqlite3_finalize(stmt1);
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return -1;
    }

    int count = 0;
    QDateTime previous, date;
    QString notebookUid;
    Incidence* incidence;
    while ((incidence = mFormat->selectComponents(stmt1, notebookUid))) {
        const QDateTime endDateTime(incidence->dateTime(Incidence::RoleEnd));
        if (useDate && endDateTime.isValid()
            && (!ignoreEnd || incidence->type() != Incidence::TypeEvent)) {
            date = endDateTime;
        } else if (useDate && incidence->dtStart().isValid()) {
            date = incidence->dtStart();
        } else {
            date = incidence->created();
        }
        if (previous != date) {
            if (!previous.isValid() || limit <= 0 || incidences->count() <= limit) {
                // If we don't have previous date, or we're within limits,
                // we can just set the 'previous' and move onward
                previous = date;
            } else {
                // Move back to old date
                date = previous;
                break;
            }
        }
        incidences->insert(notebookUid, incidence);
        count += 1;
    }
    if (last) {
        *last = date;
    }

    sqlite3_finalize(stmt1);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return count;
}

Person::List SqliteStorageImpl::loadContacts()
{
    return mFormat->selectContacts();
}

bool SqliteStorageImpl::save(const Calendar::Ptr &calendar, const ExtendedStorage &storage,
                             const QMultiHash<QString, Incidence::Ptr> &additions,
                             const QMultiHash<QString, Incidence::Ptr> &modifications,
                             const QMultiHash<QString, Incidence::Ptr> &deletions,
                             Incidence::List *added,
                             Incidence::List *modified,
                             Incidence::List *deleted,
                             ExtendedStorage::DeleteAction deleteAction)
{
    mIsSaved = false;

    if (!mDatabase) {
        return false;
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    mTimeZone = calendar->timeZone();
    if (!saveTimezone()) {
        qCWarning(lcMkcal) << "saving timezones failed";
    }

    int errors = 0;

    // Incidences to insert
    if (!additions.isEmpty()
        && !saveIncidences(calendar, storage, additions, DBInsert, added)) {
        errors++;
    }

    // Incidences to update
    if (!modifications.isEmpty()
        && !saveIncidences(calendar, storage, modifications, DBUpdate, modified)) {
        errors++;
    }

    // Incidences to delete / mark as deleted
    if (!deletions.isEmpty() && deleteAction == ExtendedStorage::PurgeDeleted
        && !saveIncidences(calendar, storage, deletions, DBDelete, deleted)) {
        errors++;
    }
    else if (!deletions.isEmpty() && deleteAction == ExtendedStorage::MarkDeleted
             && !saveIncidences(calendar, storage, deletions, DBMarkDeleted, deleted)) {
        errors++;
    }

    if (mIsSaved)
        mFormat->incrementTransactionId(&mSavedTransactionId);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    if (mIsSaved) {
        mChanged.resize(0);   // make a change to create signal
    }

    return errors == 0;
}

bool SqliteStorageImpl::save(const MemoryCalendar *calendar,
                             const QStringList &toAdd, const QStringList &toUpdate, const QStringList &toDelete,
                             QStringList *added, QStringList *modified, QStringList *deleted,
                             ExtendedStorage::DeleteAction deleteAction)
{
    mIsSaved = false;

    if (!mDatabase) {
        return false;
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    mTimeZone = calendar->timeZone();
    if (!saveTimezone()) {
        qCWarning(lcMkcal) << "saving timezones failed";
    }

    int errors = 0;

    // Incidences to insert
    if (!toAdd.isEmpty()
        && !saveIncidences(calendar, toAdd, DBInsert, added)) {
        errors++;
    }

    // Incidences to update
    if (!toUpdate.isEmpty()
        && !saveIncidences(calendar, toUpdate, DBUpdate, modified)) {
        errors++;
    }

    // Incidences to delete / mark as deleted
    if (!toDelete.isEmpty() && deleteAction == ExtendedStorage::PurgeDeleted
        && !saveIncidences(calendar, toDelete, DBDelete, deleted)) {
        errors++;
    }
    else if (!toDelete.isEmpty() && deleteAction == ExtendedStorage::MarkDeleted
             && !saveIncidences(calendar, toDelete, DBMarkDeleted, deleted)) {
        errors++;
    }

    if (!added->isEmpty() || !modified->isEmpty() || !deleted->isEmpty())
        mIsSaved = true;

    if (mIsSaved)
        mFormat->incrementTransactionId(&mSavedTransactionId);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    if (mIsSaved) {
        mChanged.resize(0);   // make a change to create signal
    }

    return errors == 0;
}

bool SqliteStorageImpl::saveIncidences(const Calendar::Ptr &calendar, const ExtendedStorage &storage,
                                       const QMultiHash<QString, Incidence::Ptr> &list, DBOperation dbop,
                                       Incidence::List *savedIncidences)
{
    int rv = 0;
    int errors = 0;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";
    QMultiHash<QString, Incidence::Ptr>::ConstIterator it;
    char *errmsg = NULL;
    const char *query = NULL;

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    for (it = list.constBegin(); it != list.constEnd(); ++it) {
        const QString notebookUid = calendar->notebook(*it);
        qCDebug(lcMkcal) << operation << "incidence" << (*it)->uid() << "notebook" << notebookUid;
        if (!mFormat->modifyComponents(**it, notebookUid, dbop)) {
            qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase) << "for incidence" << (*it)->uid();
            errors++;
        } else {
            (*savedIncidences) << *it;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    if (!savedIncidences->isEmpty())
        mIsSaved = true;

    return errors == 0;

error:
    return false;
}

bool SqliteStorageImpl::saveIncidences(const MemoryCalendar *calendar,
                                       const QStringList &list, DBOperation dbop,
                                       QStringList *savedIncidences)
{
    int rv = 0;
    int errors = 0;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";
    QMultiHash<QString, Incidence::Ptr>::ConstIterator it;
    char *errmsg = NULL;
    const char *query = NULL;

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    for (const QString &id : list) {
        Incidence::Ptr incidence = calendar->instance(id);
        if (!incidence) {
            qCWarning(lcMkcal) << "invalid id - not saving incidence" << id;
            continue;
        }
        const QString notebookUid = calendar->notebook(incidence);

        qCDebug(lcMkcal) << operation << "incidence" << incidence->uid() << "notebook" << notebookUid;
        if (mFormat->modifyComponents(*incidence, notebookUid, dbop)) {
            savedIncidences->append(id);
        } else {
            qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase) << "for incidence" << incidence->uid();
            errors++;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    if (!savedIncidences->isEmpty())
        mIsSaved = true;

    return errors == 0;

error:
    return false;
}

sqlite3_stmt* SqliteStorageImpl::selectInsertedIncidences(const QDateTime &after,
                                                          const QString &notebookUid)
{
    const char *query1;
    int qsize1;
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index;
    QByteArray n;
    sqlite3_int64 secs;

    if (!mDatabase) {
        return nullptr;
    }

    qCDebug(lcMkcal) << "incidences inserted"
                     << "since" << after.toString();

    if (!notebookUid.isNull()) {
        query1 = SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK);
    } else {
        query1 = SELECT_COMPONENTS_BY_CREATED;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_CREATED);
    }
    SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, nullptr);

    index = 1;
    secs = (after.isValid()) ? mFormat->toOriginTime(after) : 0;
    SL3_bind_int64(stmt1, index, secs);
    if (!notebookUid.isNull()) {
        n = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_TRANSIENT);
    }

    return stmt1;

error:
    if (stmt1) {
        sqlite3_finalize(stmt1);
    }
    return nullptr;
}

sqlite3_stmt* SqliteStorageImpl::selectModifiedIncidences(const QDateTime &after,
                                                         const QString &notebookUid)
{
    const char *query1;
    int qsize1;
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index;
    QByteArray n;
    sqlite3_int64 secs;

    if (!mDatabase) {
        return nullptr;
    }

    qCDebug(lcMkcal) << "incidences updated"
                     << "since" << after.toString();

    if (!notebookUid.isNull()) {
        query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK);
    } else {
        query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_LAST_MODIFIED);
    }
    SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, nullptr);

    index = 1;
    secs = (after.isValid()) ? mFormat->toOriginTime(after) : 0;
    SL3_bind_int64(stmt1, index, secs);
    secs = (after.isValid()) ? secs : LLONG_MAX;
    SL3_bind_int64(stmt1, index, secs);
    if (!notebookUid.isNull()) {
        n = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_TRANSIENT);
    }

    return stmt1;

error:
    if (stmt1) {
        sqlite3_finalize(stmt1);
    }
    return nullptr;
}

sqlite3_stmt* SqliteStorageImpl::selectDeletedIncidences(const QDateTime &after,
                                                         const QString &notebookUid)
{
    const char *query1;
    int qsize1;
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index;
    QByteArray n;
    sqlite3_int64 secs;

    if (!mDatabase) {
        return nullptr;
    }

    qCDebug(lcMkcal) << "incidences deleted"
                     << "since" << after.toString();

    if (!notebookUid.isNull()) {
        if (after.isValid()) {
            query1 = SELECT_COMPONENTS_BY_DELETED_AND_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_DELETED_AND_NOTEBOOK);
        } else {
            query1 = SELECT_COMPONENTS_ALL_DELETED_BY_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_ALL_DELETED_BY_NOTEBOOK);
        }
    } else {
        if (after.isValid()) {
            query1 = SELECT_COMPONENTS_BY_DELETED;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_DELETED);
        } else {
            query1 = SELECT_COMPONENTS_ALL_DELETED;
            qsize1 = sizeof(SELECT_COMPONENTS_ALL_DELETED);
        }
    }
    SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, nullptr);

    index = 1;
    if (after.isValid()) {
        secs = mFormat->toOriginTime(after);
        SL3_bind_int64(stmt1, index, secs);
        SL3_bind_int64(stmt1, index, secs);
    }
    if (!notebookUid.isNull()) {
        n = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_TRANSIENT);
    }

    return stmt1;

error:
    if (stmt1) {
        sqlite3_finalize(stmt1);
    }
    return nullptr;
}

sqlite3_stmt* SqliteStorageImpl::selectAllIncidences(const QString &notebookUid)
{
    const char *query1;
    int qsize1;
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index;
    QByteArray n;

    if (!mDatabase) {
        return nullptr;
    }

    qCDebug(lcMkcal) << "all incidences";

    if (!notebookUid.isNull()) {
        query1 = SELECT_COMPONENTS_BY_NOTEBOOK;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_NOTEBOOK);
    } else {
        query1 = SELECT_COMPONENTS_ALL;
        qsize1 = sizeof(SELECT_COMPONENTS_ALL);
    }
    SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, nullptr);

    index = 1;
    if (!notebookUid.isNull()) {
        n = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_TRANSIENT);
    }

    return stmt1;

error:
    if (stmt1) {
        sqlite3_finalize(stmt1);
    }
    return nullptr;
}

sqlite3_stmt* SqliteStorageImpl::selectDuplicatedIncidences(const QDateTime &after,
                                                            const QString &notebookUid,
                                                            const QString &summary)
{
    const char *query1;
    int qsize1;
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index;
    QByteArray n;
    QByteArray s;
    sqlite3_int64 secs;

    if (!mDatabase || summary.isNull()) {
        return nullptr;
    }

    qCDebug(lcMkcal) << "duplicated incidences since" << after.toString();

    if (!notebookUid.isNull()) {
        query1 = SELECT_COMPONENTS_BY_DUPLICATE_AND_NOTEBOOK;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_DUPLICATE_AND_NOTEBOOK);
    } else {
        query1 = SELECT_COMPONENTS_BY_DUPLICATE;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_DUPLICATE);
    }
    SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, nullptr);

    index = 1;
    secs = (after.isValid()) ? mFormat->toOriginTime(after) : 0;
    qCDebug(lcMkcal) << "QUERY FROM" << secs;
    SL3_bind_int64(stmt1, index, secs);
    s = summary.toUtf8();
    SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_TRANSIENT);
    if (!notebookUid.isNull()) {
        n = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_TRANSIENT);
    }

    return stmt1;

error:
    if (stmt1) {
        sqlite3_finalize(stmt1);
    }
    return nullptr;
}

bool SqliteStorageImpl::selectIncidences(Incidence::List *list, sqlite3_stmt *stmt)
{
    if (!list || !stmt) {
        return false;
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    QString nbook;
    Incidence* incidence;
    while ((incidence = mFormat->selectComponents(stmt, nbook))) {
        qCDebug(lcMkcal) << "adding incidence" << incidence->uid() << "into list";
        list->append(Incidence::Ptr(incidence));
    }
    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return true;
}

int SqliteStorageImpl::selectCount(const char *query, int qsize)
{
    int rv = 0;
    int count = 0;
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;

    if (!mDatabase) {
        return count;
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return count;
    }

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, &tail);
    SL3_step(stmt);
    if ((rv == SQLITE_ROW) || (rv == SQLITE_OK)) {
        count = sqlite3_column_int(stmt, 0);
    }

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return count;
}

QDateTime SqliteStorageImpl::incidenceDeletedDate(const QString &uid,
                                                  const QDateTime &recurrenceId)
{
    int index;
    QByteArray u;
    int rv = 0;
    sqlite3_int64 date;
    QDateTime deletionDate;

    if (!mDatabase) {
        return deletionDate;
    }

    const char *query = SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED;
    int qsize = sizeof(SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED);
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, &tail);
    index = 1;
    u = uid.toUtf8();
    SL3_bind_text(stmt, index, u.constData(), u.length(), SQLITE_STATIC);
    if (recurrenceId.isValid()) {
        qint64 secsRecurId;
        if (recurrenceId.timeSpec() == Qt::LocalTime) {
            secsRecurId = mFormat->toLocalOriginTime(recurrenceId);
        } else {
            secsRecurId = mFormat->toOriginTime(recurrenceId);
        }
        SL3_bind_int64(stmt, index, secsRecurId);
    } else {
        SL3_bind_int64(stmt, index, 0);
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return deletionDate;
    }

    SL3_step(stmt);
    if ((rv == SQLITE_ROW) || (rv == SQLITE_OK)) {
        date = sqlite3_column_int64(stmt, 1);
        deletionDate = mFormat->fromOriginTime(date);
    }

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return deletionDate;
}

bool SqliteStorageImpl::purgeIncidences(const Incidence::List &list)
{
    int rv = 0;
    unsigned int error = 1;

    char *errmsg = NULL;
    const char *query = NULL;

    if (!mDatabase) {
        return false;
    }

    qCDebug(lcMkcal) << "deleting" << list.count() << "incidences";

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    error = 0;
    for (const KCalendarCore::Incidence::Ptr &incidence: list) {
        if (!mFormat->modifyComponents(*incidence, QString(), DBDelete)) {
            error += 1;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;

 error:
    qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase);
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return false;
}

bool SqliteStorageImpl::purgeDeletedIncidences(const Incidence::List &list)
{
    int rv = 0;
    unsigned int error = 1;

    char *errmsg = NULL;
    const char *query = NULL;

    if (!mDatabase) {
        return false;
    }

    qCDebug(lcMkcal) << "purging" << list.count() << "incidences";

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    error = 0;
    for (const KCalendarCore::Incidence::Ptr &incidence: list) {
        if (!mFormat->purgeDeletedComponents(incidence->uid(), incidence->recurrenceId())) {
            error += 1;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;

 error:
    qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase);
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return false;
}

bool SqliteStorageImpl::purgeDeletedIncidences(const QList<IncidenceId> &list)
{
    int rv = 0;
    unsigned int error = 1;

    char *errmsg = NULL;
    const char *query = NULL;

    if (!mDatabase) {
        return false;
    }

    qCDebug(lcMkcal) << "purging" << list.count() << "incidences";

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    error = 0;
    for (const IncidenceId &id: list) {
        if (!mFormat->purgeDeletedComponents(id.uid, id.recId)) {
            error += 1;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;

 error:
    qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase);
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return false;
}

bool SqliteStorageImpl::saveTimezone()
{
    int rv = 0;
    int index = 1;

    if (!mDatabase) {
        return false;
    }

    const char *query1 = UPDATE_TIMEZONES;
    int qsize1 = sizeof(UPDATE_TIMEZONES);
    sqlite3_stmt *stmt1 = NULL;

    if (mTimeZone.isValid()) {
        MemoryCalendar::Ptr temp(new MemoryCalendar(mTimeZone));
        ICalFormat ical;
        QByteArray data = ical.toString(temp, QString()).toUtf8();

        // Semaphore is already locked here.
        SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, NULL);
        SL3_bind_text(stmt1, index, data, data.length(), SQLITE_STATIC);
        SL3_step(stmt1);
        mIsSaved = true;
        qCDebug(lcMkcal) << "updated timezones in database";
        sqlite3_finalize(stmt1);
    }

    return true;

 error:
    sqlite3_finalize(stmt1);
    qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase);

    return false;
}

bool SqliteStorageImpl::loadTimezone()
{
    int rv = 0;
    bool success = false;

    if (!mDatabase) {
        return false;
    }

    const char *query = SELECT_TIMEZONES;
    int qsize = sizeof(SELECT_TIMEZONES);
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, &tail);

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    SL3_step(stmt);
    if (rv == SQLITE_ROW) {
        QString zoneData = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 1));
        if (!zoneData.isEmpty()) {
            MemoryCalendar::Ptr temp(new MemoryCalendar(mTimeZone));
            ICalFormat ical;
            if (ical.fromString(temp, zoneData)) {
                qCDebug(lcMkcal) << "loaded timezones from database";
                mTimeZone = temp->timeZone();
            } else {
                qCWarning(lcMkcal) << "failed to load timezones from database";
            }
        }
    }
    // Return true in any case, unless there was an sql error.
    success = true;

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return success;
}

bool SqliteStorageImpl::loadNotebooks(QList<Notebook*> *notebooks,
                                      Notebook **defaultNb)
{
    const char *query = SELECT_CALENDARS_ALL;
    int qsize = sizeof(SELECT_CALENDARS_ALL);

    int rv = 0;
    sqlite3_stmt *stmt = NULL;
    bool isDefault;

    Notebook *nb;

    if (!mDatabase || !defaultNb || !notebooks || !notebooks->isEmpty()) {
        return false;
    }

    *defaultNb = nullptr;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, nullptr);

    while ((nb = mFormat->selectCalendars(stmt, &isDefault))) {
        qCDebug(lcMkcal) << "loaded notebook" << nb->uid() << nb->name() << "from database";
        if (isDefault) {
            *defaultNb = nb;
        }
        notebooks->append(nb);
    }

    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    if (notebooks->isEmpty()) {
        qCDebug(lcMkcal) << "Storage is empty, initializing";
        *defaultNb = new Notebook(QString::fromLatin1("Default"),
                                  QString(),
                                  QString::fromLatin1("#0000FF"));
        if (modifyNotebook(**defaultNb, DBInsert, true)) {
            notebooks->append(*defaultNb);
        } else {
            qCWarning(lcMkcal) << "Unable to add a default notebook.";
            delete *defaultNb;
            *defaultNb = nullptr;
            return false;
        }
    }

    return true;

error:
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return false;
}

bool SqliteStorageImpl::loadNotebook(Notebook **notebook, const QString &notebookUid)
{
    const char *query = SELECT_CALENDARS_BY_UID;
    int qsize = sizeof(SELECT_CALENDARS_BY_UID);

    int rv = 0;
    int index = 1;
    sqlite3_stmt *stmt = NULL;
    bool isDefault;
    QByteArray uid = notebookUid.toUtf8();

    if (!mDatabase || !notebook) {
        return false;
    }
    *notebook = nullptr;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, nullptr);
    SL3_bind_text(stmt, index, uid.constData(), uid.length(), SQLITE_STATIC);

    *notebook = mFormat->selectCalendars(stmt, &isDefault);

    sqlite3_finalize(stmt);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;

error:
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return false;
}

bool SqliteStorageImpl::modifyNotebook(const Notebook &nb, DBOperation dbop, bool isDefault)
{
    int rv = 0;
    bool success = true;
    const char *query = NULL;
    int qsize = 0;
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";

    if (!mDatabase) {
        return false;
    }

    Incidence::List deleted, all;
    // Execute database operation.
    if (dbop == DBInsert) {
        query = INSERT_CALENDARS;
        qsize = sizeof(INSERT_CALENDARS);
    } else if (dbop == DBUpdate) {
        query = UPDATE_CALENDARS;
        qsize = sizeof(UPDATE_CALENDARS);
    } else if (dbop == DBDelete) {
        query = DELETE_CALENDARS;
        qsize = sizeof(DELETE_CALENDARS);
        selectIncidences(&deleted, selectDeletedIncidences(QDateTime(), nb.uid()));
        selectIncidences(&all, selectAllIncidences(nb.uid()));
    } else {
        return false;
    }

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, &tail);

    if ((success = mFormat->modifyCalendars(nb, dbop, stmt, isDefault))) {
        qCDebug(lcMkcal) << operation << "notebook" << nb.uid() << nb.name() << "in database";
    }

    sqlite3_finalize(stmt);

    if (success) {
        mFormat->incrementTransactionId(&mSavedTransactionId);
    }

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    if (success) {
        if (!deleted.isEmpty() && !purgeDeletedIncidences(deleted)) {
            qCWarning(lcMkcal) << "error when purging deleted incidences from notebook" << nb.uid();
        }
        if (!all.isEmpty() && !purgeIncidences(all)) {
            qCWarning(lcMkcal) << "error when deleting incidences from notebook" << nb.uid();
        }
        mChanged.resize(0);   // make a change to create signal
    }
    return success;

error:
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return false;
}

bool SqliteStorageImpl::fileChanged()
{
    if (!mDatabase) {
        return false;
    }
    
    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }
    int transactionId;
    if (!mFormat->selectMetadata(&transactionId))
        transactionId = mSavedTransactionId - 1; // Ensure reload on error
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    if (transactionId != mSavedTransactionId) {
        mSavedTransactionId = transactionId;
        if (!loadTimezone()) {
            qCWarning(lcMkcal) << "loading timezones failed";
        }
        return true;
    } else {
        return false;
    }
}
