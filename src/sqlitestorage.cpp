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
#include "sqlitestorage.h"
#include "sqliteformat.h"
#include "logging_p.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>
using namespace KCalendarCore;

#include <QFileSystemWatcher>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUuid>

#include <iostream>
using namespace std;

#ifdef Q_OS_UNIX
#include "semaphore_p.h"
#else
#include <QSystemSemaphore>
#endif

using namespace mKCal;

static const QString gChanged(QLatin1String(".changed"));

static const char *createStatements[] =
{
    CREATE_METADATA,
    CREATE_CALENDARS,
    CREATE_COMPONENTS,
    CREATE_RDATES,
    CREATE_CUSTOMPROPERTIES,
    CREATE_RECURSIVE,
    CREATE_ALARM,
    CREATE_ATTENDEE,
    CREATE_ATTACHMENTS,
    CREATE_CALENDARPROPERTIES,
    /* Create index on frequently used columns */
    INDEX_CALENDAR,
    INDEX_COMPONENT,
    INDEX_COMPONENT_UID,
    INDEX_COMPONENT_NOTEBOOK,
    INDEX_RDATES,
    INDEX_CUSTOMPROPERTIES,
    INDEX_RECURSIVE,
    INDEX_ALARM,
    INDEX_ATTENDEE,
    INDEX_ATTACHMENTS,
    INDEX_CALENDARPROPERTIES,
    "PRAGMA foreign_keys = ON",
    "PRAGMA user_version = 2"
};

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::SqliteStorage::Private
{
public:
    Private(const ExtendedCalendar::Ptr &calendar, SqliteStorage *storage,
            const QString &databaseName
           )
        : mCalendar(calendar),
          mStorage(storage),
          mDatabaseName(databaseName),
#ifdef Q_OS_UNIX
          mSem(databaseName),
#else
          mSem(databaseName, 1, QSystemSemaphore::Open),
#endif
          mChanged(databaseName + gChanged),
          mWatcher(0),
          mFormat(0),
          mIsLoading(false),
          mIsSaved(false)
    {}
    ~Private()
    {
    }

    ExtendedCalendar::Ptr mCalendar;
    SqliteStorage *mStorage;
    QString mDatabaseName;
#ifdef Q_OS_UNIX
    ProcessMutex mSem;
#else
    QSystemSemaphore mSem;
#endif

    QFile mChanged;
    QFileSystemWatcher *mWatcher;
    int mSavedTransactionId;
    sqlite3 *mDatabase = nullptr;
    SqliteFormat *mFormat = nullptr;
    QHash<QString, Incidence::Ptr> mIncidencesToInsert;
    QHash<QString, Incidence::Ptr> mIncidencesToUpdate;
    QHash<QString, Incidence::Ptr> mIncidencesToDelete;
    bool mIsLoading;
    bool mIsSaved;

    bool addIncidence(const Incidence::Ptr &incidence, const QString &notebookUid);
    bool loadRecurringIncidences();
    bool saveNotebook(const Notebook::Ptr &nb, DBOperation dbop);
    int loadIncidences(sqlite3_stmt *stmt1);
    int loadIncidencesBySeries(sqlite3_stmt *stmt1, QStringList *identifiers = nullptr, int limit = 0);
    bool saveIncidences(QHash<QString, Incidence::Ptr> &list, DBOperation dbop,
                        Incidence::List *savedIncidences);
};
//@endcond

SqliteStorage::SqliteStorage(const ExtendedCalendar::Ptr &cal, const QString &databaseName,
                             bool validateNotebooks)
    : ExtendedStorage(cal, validateNotebooks),
      d(new Private(cal, this, databaseName))
{
}

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

SqliteStorage::SqliteStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks)
    : SqliteStorage(cal, defaultLocation(), validateNotebooks)
{
}

SqliteStorage::~SqliteStorage()
{
    close();
    delete d;
}

QString SqliteStorage::databaseName() const
{
    return d->mDatabaseName;
}

bool SqliteStorage::open()
{
    int rv;
    char *errmsg = NULL;
    const char *query = NULL;

    if (d->mDatabase) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    bool fileExisted = QFile::exists(d->mDatabaseName);
    rv = sqlite3_open(d->mDatabaseName.toUtf8(), &d->mDatabase);
    if (rv) {
        qCWarning(lcMkcal) << "sqlite3_open error:" << rv << "on database" << d->mDatabaseName;
        qCWarning(lcMkcal) << sqlite3_errmsg(d->mDatabase);
        goto error;
    }
    qCDebug(lcMkcal) << "database" << d->mDatabaseName << "opened";

    // Set one and half second busy timeout for waiting for internal sqlite locks
    sqlite3_busy_timeout(d->mDatabase, 1500);

    {
        sqlite3_stmt *dbVersion = nullptr;
        SL3_prepare_v2(d->mDatabase, "PRAGMA user_version", -1, &dbVersion, nullptr);
        SL3_step(dbVersion);
        int version = 0;
        if (rv == SQLITE_ROW) {
            version = sqlite3_column_int(dbVersion, 0);
        }
        sqlite3_finalize(dbVersion);

        if (version == 0 && fileExisted) {
            qCWarning(lcMkcal) << "Migrating mkcal database to version 1";
            query = BEGIN_TRANSACTION;
            SL3_exec(d->mDatabase);
            query = "DROP INDEX IF EXISTS IDX_ATTENDEE"; // recreate on new format
            SL3_exec(d->mDatabase);
            // insert normal attendee for every organizer
            query = "INSERT INTO ATTENDEE(ComponentId, Email, Name, IsOrganizer, Role, PartStat, Rsvp, DelegatedTo, DelegatedFrom) "
                    "              SELECT ComponentId, Email, Name, 0, Role, PartStat, Rsvp, DelegatedTo, DelegatedFrom "
                    "              FROM ATTENDEE WHERE isOrganizer=1";
            SL3_exec(d->mDatabase);
            query = "PRAGMA user_version = 1";
            SL3_exec(d->mDatabase);
            query = COMMIT_TRANSACTION;
            SL3_exec(d->mDatabase);

            version = 1;
        }
        if (version == 1) {
            qCWarning(lcMkcal) << "Migrating mkcal database to version 2";
            query = BEGIN_TRANSACTION;
            SL3_exec(d->mDatabase);
            query = "ALTER TABLE Components ADD COLUMN thisAndFuture INTEGER";
            SL3_try_exec(d->mDatabase); // Ignore error if any, consider that column already exists.
            query = "PRAGMA user_version = 2";
            SL3_exec(d->mDatabase);
            query = COMMIT_TRANSACTION;
            SL3_exec(d->mDatabase);

            version = 2;
        }
    }

    for (unsigned int i = 0; i < (sizeof(createStatements)/sizeof(createStatements[0])); i++) {
         query = createStatements[i];
         SL3_exec(d->mDatabase);
    }

    d->mFormat = new SqliteFormat(d->mDatabase);
    d->mFormat->selectMetadata(&d->mSavedTransactionId);

    if (!d->mChanged.open(QIODevice::Append)) {
        qCWarning(lcMkcal) << "cannot open changed file for" << d->mDatabaseName;
        goto error;
    }
    d->mWatcher = new QFileSystemWatcher();
    d->mWatcher->addPath(d->mChanged.fileName());
    connect(d->mWatcher, &QFileSystemWatcher::fileChanged,
            this, &SqliteStorage::fileChanged);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        goto error;
    }

    if (!loadNotebooks()) {
        qCWarning(lcMkcal) << "cannot load notebooks from calendar";
        goto error;
    }

    if (notebooks().isEmpty() || !defaultNotebook()) {
        qCDebug(lcMkcal) << "Storage has no default notebook, adding one";
        Notebook::Ptr defaultNb(new Notebook(QString::fromLatin1("Default"),
                                             QString(),
                                             QString::fromLatin1("#0000FF")));
        if (!setDefaultNotebook(defaultNb)) {
            qCWarning(lcMkcal) << "Unable to add a default notebook.";
            close();
            return false;
        }
    }

    return true;

error:
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    close();
    return false;
}

bool SqliteStorage::load()
{
    if (!d->mDatabase) {
        return false;
    }

    int rv = 0;
    int count = -1;
    d->mIsLoading = true;

    const char *query1 = NULL;
    int qsize1 = 0;

    sqlite3_stmt *stmt1 = NULL;

    query1 = SELECT_COMPONENTS_ALL;
    qsize1 = sizeof(SELECT_COMPONENTS_ALL);

    SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, NULL);

    count = d->loadIncidences(stmt1);

error:
    d->mIsLoading = false;

    setIsRecurrenceLoaded(count >= 0);
    if (count >= 0) {
        addLoadedRange(QDate(), QDate());
    }

    return count >= 0;
}

bool SqliteStorage::load(const QString &uid)
{
    if (!d->mDatabase) {
        return false;
    }

    // Don't reload an existing incidence from DB.
    // Either the calendar is already in sync with
    // the calendar or the database has been externally
    // modified and in that case, the calendar has been emptied.
    if (calendar()->incidence(uid)) {
        return true;
    }

    int rv = 0;
    int count = -1;
    d->mIsLoading = true;

    const char *query1 = NULL;
    int qsize1 = 0;

    sqlite3_stmt *stmt1 = NULL;
    int index = 1;
    QByteArray u;

    if (!uid.isEmpty()) {
        query1 = SELECT_COMPONENTS_BY_UID;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_UID);

        SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, NULL);
        u = uid.toUtf8();
        SL3_bind_text(stmt1, index, u.constData(), u.length(), SQLITE_STATIC);

        count = d->loadIncidences(stmt1);
    }
error:
    d->mIsLoading = false;

    return count >= 0;
}

bool SqliteStorage::load(const QDate &start, const QDate &end)
{
    if (!d->mDatabase) {
        return false;
    }

    // We have no way to know if a recurring incidence
    // is happening within [start, end[, so load them all.
    if ((start.isValid() || end.isValid())
        && !d->loadRecurringIncidences()) {
        return false;
    }

    int rv = 0;
    int count = -1;
    QDateTime loadStart;
    QDateTime loadEnd;

    d->mIsLoading = true;

    if (getLoadDates(start, end, &loadStart, &loadEnd)) {
        const char *query1 = NULL;
        int qsize1 = 0;

        sqlite3_stmt *stmt1 = NULL;
        int index = 1;
        qint64 secsStart;
        qint64 secsEnd;

        // Incidences to insert
        if (loadStart.isValid() && loadEnd.isValid()) {
            query1 = SELECT_COMPONENTS_BY_DATE_BOTH;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_DATE_BOTH);
            SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, NULL);
            secsStart = d->mFormat->toOriginTime(loadStart);
            secsEnd = d->mFormat->toOriginTime(loadEnd);
            SL3_bind_int64(stmt1, index, secsEnd);
            SL3_bind_int64(stmt1, index, secsStart);
            SL3_bind_int64(stmt1, index, secsStart);
        } else if (loadStart.isValid()) {
            query1 = SELECT_COMPONENTS_BY_DATE_START;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_DATE_START);
            SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, NULL);
            secsStart = d->mFormat->toOriginTime(loadStart);
            SL3_bind_int64(stmt1, index, secsStart);
            SL3_bind_int64(stmt1, index, secsStart);
        } else if (loadEnd.isValid()) {
            query1 = SELECT_COMPONENTS_BY_DATE_END;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_DATE_END);
            SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, NULL);
            secsEnd = d->mFormat->toOriginTime(loadEnd);
            SL3_bind_int64(stmt1, index, secsEnd);
        } else {
            query1 = SELECT_COMPONENTS_ALL;
            qsize1 = sizeof(SELECT_COMPONENTS_ALL);
            SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, NULL);
        }
        count = d->loadIncidences(stmt1);

        if (count >= 0) {
            addLoadedRange(loadStart.date(), loadEnd.date());
        }
        if (loadStart.isNull() && loadEnd.isNull()) {
            setIsRecurrenceLoaded(count >= 0);
        }
    } else {
        count = 0;
    }
error:
    d->mIsLoading = false;

    return count >= 0;
}

bool SqliteStorage::loadNotebookIncidences(const QString &notebookUid)
{
    if (!d->mDatabase) {
        return false;
    }

    int rv = 0;
    int count = -1;
    d->mIsLoading = true;

    const char *query1 = NULL;
    int qsize1 = 0;

    sqlite3_stmt *stmt1 = NULL;
    int index = 1;
    QByteArray u;

    if (!notebookUid.isEmpty()) {
        query1 = SELECT_COMPONENTS_BY_NOTEBOOKUID;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_NOTEBOOKUID);

        SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, NULL);
        u = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, u.constData(), u.length(), SQLITE_STATIC);

        count = d->loadIncidences(stmt1);
    }
error:
    d->mIsLoading = false;

    return count >= 0;
}

bool SqliteStorage::Private::loadRecurringIncidences()
{
    if (!mDatabase) {
        return false;
    }

    if (mStorage->isRecurrenceLoaded()) {
        return true;
    }

    int rv = 0;
    int count = 0;
    mIsLoading = true;

    const char *query1 = NULL;
    int qsize1 = 0;

    sqlite3_stmt *stmt1 = NULL;

    query1 = SELECT_COMPONENTS_BY_RECURSIVE;
    qsize1 = sizeof(SELECT_COMPONENTS_BY_RECURSIVE);

    SL3_prepare_v2(mDatabase, query1, qsize1, &stmt1, NULL);

    count = loadIncidences(stmt1);

error:
    mIsLoading = false;

    mStorage->setIsRecurrenceLoaded(count >= 0);

    return count >= 0;
}

bool SqliteStorage::search(const QString &key, QStringList *identifiers, int limit)
{
    if (!d->mDatabase || key.isEmpty())
        return false;

    d->mIsLoading = true;
    const char *query1 = SEARCH_COMPONENTS;
    int qsize1 = sizeof(SEARCH_COMPONENTS);
    const QByteArray s('%' + key.toUtf8().replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + '%');
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index = 1;
    Incidence::Ptr incidence;
    QString nbook;
    int count = -1;

    qCDebug(lcMkcal) << "Searching DB for" << s;
    SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, nullptr);
    SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_STATIC);
    SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_STATIC);
    SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_STATIC);

    count = d->loadIncidencesBySeries(stmt1, identifiers, limit);

error:
    d->mIsLoading = false;

    return count >= 0;
}

//@cond PRIVATE
bool SqliteStorage::Private::addIncidence(const Incidence::Ptr &incidence, const QString &notebookUid)
{
    bool added = true;
    bool hasNotebook = mCalendar->hasValidNotebook(notebookUid);
    const QString key = incidence->instanceIdentifier();
    if (mIncidencesToInsert.contains(key) ||
        mIncidencesToUpdate.contains(key) ||
        mIncidencesToDelete.contains(key) ||
        (mStorage->validateNotebooks() && !hasNotebook)) {
        qCWarning(lcMkcal) << "not loading" << incidence->uid() << notebookUid
                           << (!hasNotebook ? "(invalidated notebook)" : "(local changes)");
        added = false;
    } else {
        Incidence::Ptr old(mCalendar->incidence(incidence->uid(), incidence->recurrenceId()));
        if (old) {
            if (incidence->revision() > old->revision()) {
                mCalendar->deleteIncidence(old);   // move old to deleted
                // and replace it with the new one.
            } else {
                added = false;
            }
        }
    }
    if (added && !mCalendar->addIncidence(incidence, notebookUid)) {
        added = false;
        qCWarning(lcMkcal) << "cannot add incidence" << incidence->uid() << "to notebook" << notebookUid;
    }

    return added;
}

int SqliteStorage::Private::loadIncidences(sqlite3_stmt *stmt1)
{
    int count = 0;
    Incidence::Ptr incidence;
    QString notebookUid;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return -1;
    }

    while ((incidence = mFormat->selectComponents(stmt1, notebookUid))) {
        if (addIncidence(incidence, notebookUid)) {
            // qCDebug(lcMkcal) << "updating incidence" << incidence->uid()
            //                  << incidence->dtStart() << endDateTime
            //                  << "in calendar";
            count += 1;
        }
    }

    sqlite3_finalize(stmt1);

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    mStorage->emitStorageFinished(false, "load completed");

    return count;
}

int SqliteStorage::Private::loadIncidencesBySeries(sqlite3_stmt *stmt1, QStringList *identifiers, int limit)
{
    int count = 0;
    Incidence::Ptr incidence;
    QString notebookUid;
    QSet<QString> recurringUids;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return -1;
    }

    while ((incidence = mFormat->selectComponents(stmt1, notebookUid))
           && (limit <= 0 || count < limit)) {
        if (addIncidence(incidence, notebookUid)) {
            if (incidence->recurs() || incidence->hasRecurrenceId()) {
                recurringUids.insert(incidence->uid());
            } else {
                // Apply limit on load on non recurring events only.
                count += 1;
            }
        }
        if (identifiers) {
            identifiers->append(incidence->instanceIdentifier());
        }
    }

    sqlite3_finalize(stmt1);

    if (recurringUids.count() > 0) {
        // Additionally load any exception or parent to ensure calendar
        // consistency.
        int rv = 0;
        sqlite3_stmt *loadByUid = NULL;
        const char *query1 = NULL;
        int qsize1 = 0;
        query1 = SELECT_COMPONENTS_BY_UID;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_UID);
        SL3_prepare_v2(mDatabase, query1, qsize1, &loadByUid, NULL);

        for (const QString &uid : const_cast<const QSet<QString>&>(recurringUids)) {
            int index = 1;
            QByteArray u = uid.toUtf8();
            SL3_reset(loadByUid);
            SL3_bind_text(loadByUid, index, u.constData(), u.length(), SQLITE_STATIC);
            while ((incidence = mFormat->selectComponents(stmt1, notebookUid))) {
                addIncidence(incidence, notebookUid);
            }
        }

    error:
        sqlite3_finalize(loadByUid);
    }

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    mStorage->emitStorageFinished(false, "load completed");

    return count;
}
//@endcond

bool SqliteStorage::purgeDeletedIncidences(const KCalendarCore::Incidence::List &list,
                                           const QString &notebookUid)
{
    if (!d->mDatabase) {
        return false;
    }

    if (notebookUid.isEmpty()) {
        qCWarning(lcMkcal) << "Deprecated call to purgeDeletedIncidences() with an empty notebook uid,";
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    int rv = 0;
    unsigned int error = 1;

    char *errmsg = NULL;
    const char *query = NULL;

    query = BEGIN_TRANSACTION;
    SL3_exec(d->mDatabase);

    error = 0;
    for (const KCalendarCore::Incidence::Ptr &incidence: list) {
        if (!d->mFormat->purgeDeletedComponents(*incidence, notebookUid)) {
            error += 1;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(d->mDatabase);

 error:
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    return error == 0;
}

bool SqliteStorage::save()
{
    return save(ExtendedStorage::MarkDeleted);
}

bool SqliteStorage::save(ExtendedStorage::DeleteAction deleteAction)
{
    d->mIsSaved = false;

    if (!d->mDatabase) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    int errors = 0;

    // Incidences to insert
    Incidence::List added;
    if (!d->mIncidencesToInsert.isEmpty()
        && !d->saveIncidences(d->mIncidencesToInsert, DBInsert, &added)) {
        errors++;
    }

    // Incidences to update
    Incidence::List modified;
    if (!d->mIncidencesToUpdate.isEmpty()
        && !d->saveIncidences(d->mIncidencesToUpdate, DBUpdate, &modified)) {
        errors++;
    }

    // Incidences to delete
    Incidence::List deleted;
    if (!d->mIncidencesToDelete.isEmpty()) {
        DBOperation dbop = deleteAction == ExtendedStorage::PurgeDeleted ? DBDelete : DBMarkDeleted;
        if (!d->saveIncidences(d->mIncidencesToDelete, dbop, &deleted)) {
            errors++;
        }
    }

    if (d->mIsSaved)
        d->mFormat->incrementTransactionId(&d->mSavedTransactionId);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    if (d->mIsSaved) {
        emitStorageUpdated(added, modified, deleted);
        d->mChanged.resize(0);   // make a change to create signal
    }

    if (errors == 0) {
        emitStorageFinished(false, "save completed");
    } else {
        emitStorageFinished(true, "errors saving incidences");
    }

    return errors == 0;
}

//@cond PRIVATE
bool SqliteStorage::Private::saveIncidences(QHash<QString, Incidence::Ptr> &list, DBOperation dbop, Incidence::List *savedIncidences)
{
    int rv = 0;
    int errors = 0;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";
    QHash<QString, Incidence::Ptr>::const_iterator it;
    char *errmsg = NULL;
    const char *query = NULL;

    query = BEGIN_TRANSACTION;
    SL3_exec(mDatabase);

    for (it = list.constBegin(); it != list.constEnd(); ++it) {
        QString notebookUid = mCalendar->notebook(*it);
        if (dbop == DBInsert || dbop == DBUpdate) {
            const Notebook::Ptr notebook = mStorage->notebook(notebookUid);
            // Notice : we allow to save/delete incidences in a read-only
            // notebook. The read-only flag is a hint only. This allows
            // to update a marked as read-only notebook to reflect external
            // changes.
            if ((notebook && notebook->isRunTimeOnly()) ||
                (!notebook && mStorage->validateNotebooks())) {
                qCWarning(lcMkcal) << "invalid notebook - not saving incidence" << (*it)->uid();
                continue;
            }
         }
         (*savedIncidences) << *it;

        qCDebug(lcMkcal) << operation << "incidence" << (*it)->uid() << "notebook" << notebookUid;
        if (!mFormat->modifyComponents(**it, notebookUid, dbop)) {
            qCWarning(lcMkcal) << sqlite3_errmsg(mDatabase) << "for incidence" << (*it)->uid();
            errors++;
        }
    }

    list.clear();
    // TODO What if there were errors? Options: 1) rollback 2) best effort.

    query = COMMIT_TRANSACTION;
    SL3_exec(mDatabase);

    if (!savedIncidences->isEmpty())
        mIsSaved = true;

    return errors == 0;

error:
    return false;
}
//@endcond

bool SqliteStorage::close()
{
    if (d->mDatabase) {
        if (d->mWatcher) {
            d->mWatcher->removePaths(d->mWatcher->files());
            // This should work, as storage should be closed before
            // application terminates now. If not, deadlock occurs.
            delete d->mWatcher;
            d->mWatcher = NULL;
        }
        d->mChanged.close();
        delete d->mFormat;
        d->mFormat = 0;
        sqlite3_close(d->mDatabase);
        d->mDatabase = 0;
    }
    return ExtendedStorage::close();
}

void SqliteStorage::calendarModified(bool modified, Calendar *calendar)
{
    Q_UNUSED(calendar);
    qCDebug(lcMkcal) << "calendarModified called:" << modified;
}

void SqliteStorage::calendarIncidenceAdded(const Incidence::Ptr &incidence)
{
    if (d->mIsLoading) {
        return;
    }

    const QString key = incidence->instanceIdentifier();
    if (d->mIncidencesToDelete.remove(key) > 0) {
        qCDebug(lcMkcal) << "removing incidence from deleted" << key;
        calendarIncidenceChanged(incidence);
    } else if (!d->mIncidencesToInsert.contains(key)) {
        qCDebug(lcMkcal) << "appending incidence" << key << "for database insert";
        d->mIncidencesToInsert.insert(key, incidence);
    }
}

void SqliteStorage::calendarIncidenceChanged(const Incidence::Ptr &incidence)
{
    const QString key = incidence->instanceIdentifier();
    if (!d->mIncidencesToUpdate.contains(key) &&
        !d->mIncidencesToInsert.contains(key) &&
        !d->mIsLoading) {
        qCDebug(lcMkcal) << "appending incidence" << key << "for database update";
        d->mIncidencesToUpdate.insert(key, incidence);
    }
}

void SqliteStorage::calendarIncidenceDeleted(const Incidence::Ptr &incidence, const KCalendarCore::Calendar *calendar)
{
    Q_UNUSED(calendar);

    const QString key = incidence->instanceIdentifier();
    if (d->mIncidencesToInsert.contains(key) && !d->mIsLoading) {
        qCDebug(lcMkcal) << "removing incidence from inserted" << key;
        d->mIncidencesToInsert.remove(key);
    } else {
        if (!d->mIncidencesToDelete.contains(key) && !d->mIsLoading) {
            qCDebug(lcMkcal) << "appending incidence" << key << "for database delete";
            d->mIncidencesToDelete.insert(key, incidence);
        }
    }
}

void SqliteStorage::calendarIncidenceAdditionCanceled(const Incidence::Ptr &incidence)
{
    const QString key = incidence->instanceIdentifier();
    if (d->mIncidencesToInsert.contains(key) && !d->mIsLoading) {
        qCDebug(lcMkcal) << "duplicate - removing incidence from inserted" << key;
        d->mIncidencesToInsert.remove(key);
    }
}

bool SqliteStorage::insertedIncidences(Incidence::List *list, const QDateTime &after,
                                       const QString &notebookUid)
{
    if (d->mDatabase && list && after.isValid()) {
        const char *query1 = NULL;
        int qsize1 = 0;
        int rv = 0;
        sqlite3_stmt *stmt1 = NULL;
        int index = 1;
        QByteArray n;
        sqlite3_int64 secs;
        Incidence::Ptr incidence;
        QString nbook;
        bool success = false;

        if (!notebookUid.isEmpty()) {
            query1 = SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK);
        } else {
            query1 = SELECT_COMPONENTS_BY_CREATED;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_CREATED);
        }

        qCDebug(lcMkcal) << "incidences inserted since" << after;
        if (!d->mSem.acquire()) {
            qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
            return false;
        }

        SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, nullptr);
        secs = d->mFormat->toOriginTime(after);
        SL3_bind_int64(stmt1, index, secs);
        if (!notebookUid.isEmpty()) {
            n = notebookUid.toUtf8();
            SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
        }
        while ((incidence = d->mFormat->selectComponents(stmt1, nbook))) {
            list->append(incidence);
        }
        success = true;

    error:
        sqlite3_finalize(stmt1);
        if (!d->mSem.release()) {
            qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        }
        return success;
    }
    return false;
}

bool SqliteStorage::modifiedIncidences(Incidence::List *list, const QDateTime &after,
                                       const QString &notebookUid)
{
    if (d->mDatabase && list && after.isValid()) {
        const char *query1 = NULL;
        int qsize1 = 0;
        int rv = 0;
        sqlite3_stmt *stmt1 = NULL;
        int index = 1;
        QByteArray n;
        sqlite3_int64 secs;
        Incidence::Ptr incidence;
        QString nbook;
        bool success = false;

        if (!notebookUid.isEmpty()) {
            query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK);
        } else {
            query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_LAST_MODIFIED);
        }

        qCDebug(lcMkcal) << "incidences updated since" << after;
        if (!d->mSem.acquire()) {
            qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
            return false;
        }

        SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, nullptr);
        secs = d->mFormat->toOriginTime(after);
        SL3_bind_int64(stmt1, index, secs);
        SL3_bind_int64(stmt1, index, secs);
        if (!notebookUid.isEmpty()) {
            n = notebookUid.toUtf8();
            SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
        }
        while ((incidence = d->mFormat->selectComponents(stmt1, nbook))) {
            list->append(incidence);
        }
        success = true;

    error:
        sqlite3_finalize(stmt1);
        if (!d->mSem.release()) {
            qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        }
        return success;
    }
    return false;
}

bool SqliteStorage::deletedIncidences(Incidence::List *list, const QDateTime &after,
                                      const QString &notebookUid)
{
    if (d->mDatabase && list) {
        const char *query1 = NULL;
        int qsize1 = 0;
        int rv = 0;
        sqlite3_stmt *stmt1 = NULL;
        int index = 1;
        QByteArray n;
        sqlite3_int64 secs;
        Incidence::Ptr incidence;
        QString nbook;
        bool success = false;

        if (!notebookUid.isEmpty()) {
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

        qCDebug(lcMkcal) << "incidences deleted since" << after;
        if (!d->mSem.acquire()) {
            qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
            return false;
        }

        SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, nullptr);
        if (after.isValid()) {
            secs = d->mFormat->toOriginTime(after);
            SL3_bind_int64(stmt1, index, secs);
            SL3_bind_int64(stmt1, index, secs);
        }
        if (!notebookUid.isEmpty()) {
            n = notebookUid.toUtf8();
            SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
        }
        while ((incidence = d->mFormat->selectComponents(stmt1, nbook))) {
            list->append(incidence);
        }
        success = true;

    error:
        sqlite3_finalize(stmt1);
        if (!d->mSem.release()) {
            qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        }
        return success;
    }
    return false;
}

bool SqliteStorage::allIncidences(Incidence::List *list, const QString &notebookUid)
{
    if (d->mDatabase && list) {
        const char *query1 = NULL;
        int qsize1 = 0;
        int rv = 0;
        sqlite3_stmt *stmt1 = NULL;
        int index = 1;
        QByteArray n;
        Incidence::Ptr incidence;
        QString nbook;
        bool success = false;

        if (!notebookUid.isEmpty()) {
            query1 = SELECT_COMPONENTS_BY_NOTEBOOKUID;
            qsize1 = sizeof(SELECT_COMPONENTS_BY_NOTEBOOKUID);
        } else {
            query1 = SELECT_COMPONENTS_ALL;
            qsize1 = sizeof(SELECT_COMPONENTS_ALL);
        }

        qCDebug(lcMkcal) << "all incidences";
        if (!d->mSem.acquire()) {
            qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
            return false;
        }

        SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt1, nullptr);
        if (!notebookUid.isEmpty()) {
            n = notebookUid.toUtf8();
            SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
        }
        while ((incidence = d->mFormat->selectComponents(stmt1, nbook))) {
            list->append(incidence);
        }
        success = true;

    error:
        sqlite3_finalize(stmt1);
        if (!d->mSem.release()) {
            qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        }
        return success;
    }
    return false;
}

QDateTime SqliteStorage::incidenceDeletedDate(const Incidence::Ptr &incidence)
{
    int index;
    QByteArray u;
    int rv = 0;
    sqlite3_int64 date;
    QDateTime deletionDate;

    if (!d->mDatabase) {
        return deletionDate;
    }

    const char *query = SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED;
    int qsize = sizeof(SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED);
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;

    SL3_prepare_v2(d->mDatabase, query, qsize, &stmt, &tail);
    index = 1;
    u = incidence->uid().toUtf8();
    SL3_bind_text(stmt, index, u.constData(), u.length(), SQLITE_STATIC);
    if (incidence->hasRecurrenceId()) {
        qint64 secsRecurId;
        if (incidence->recurrenceId().timeSpec() == Qt::LocalTime) {
            secsRecurId = d->mFormat->toLocalOriginTime(incidence->recurrenceId());
        } else {
            secsRecurId = d->mFormat->toOriginTime(incidence->recurrenceId());
        }
        SL3_bind_int64(stmt, index, secsRecurId);
    } else {
        SL3_bind_int64(stmt, index, 0);
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return deletionDate;
    }

    SL3_step(stmt);
    if ((rv == SQLITE_ROW) || (rv == SQLITE_OK)) {
        date = sqlite3_column_int64(stmt, 1);
        deletionDate = d->mFormat->fromOriginTime(date);
    }

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    return deletionDate;
}

bool SqliteStorage::loadNotebooks()
{
    const char *query = SELECT_CALENDARS_ALL;
    int qsize = sizeof(SELECT_CALENDARS_ALL);

    int rv = 0;
    sqlite3_stmt *stmt = NULL;
    bool isDefault;

    Notebook::Ptr nb;

    if (!d->mDatabase) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    d->mIsLoading = true;

    SL3_prepare_v2(d->mDatabase, query, qsize, &stmt, nullptr);

    while ((nb = d->mFormat->selectCalendars(stmt, &isDefault))) {
        qCDebug(lcMkcal) << "loaded notebook" << nb->uid() << nb->name() << "from database";
        if (isDefault && !setDefaultNotebook(nb)) {
            qCWarning(lcMkcal) << "cannot add default notebook" << nb->uid() << nb->name() << "to storage";
            nb = Notebook::Ptr();
        } else if (!isDefault && !addNotebook(nb)) {
            qCWarning(lcMkcal) << "cannot add notebook" << nb->uid() << nb->name() << "to storage";
            nb = Notebook::Ptr();
        }
    }

    sqlite3_finalize(stmt);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    d->mIsLoading = false;
    return true;

error:
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    d->mIsLoading = false;
    return false;
}

bool SqliteStorage::insertNotebook(const Notebook::Ptr &nb)
{
    return d->saveNotebook(nb, DBInsert);
}

bool SqliteStorage::modifyNotebook(const Notebook::Ptr &nb)
{
    return d->saveNotebook(nb, DBUpdate);
}

bool SqliteStorage::eraseNotebook(const Notebook::Ptr &nb)
{
    return d->saveNotebook(nb, DBDelete);
}

//@cond PRIVATE
bool SqliteStorage::Private::saveNotebook(const Notebook::Ptr &nb, DBOperation dbop)
{
    int rv = 0;
    bool success = mIsLoading; // true if we are currently loading
    const char *query = NULL;
    int qsize = 0;
    sqlite3_stmt *stmt = NULL;
    const char *tail = NULL;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";

    if (!mDatabase) {
        return false;
    }

    if (!mIsLoading) {
        // Execute database operation.
        if (dbop == DBInsert) {
            query = INSERT_CALENDARS;
            qsize = sizeof(INSERT_CALENDARS);
            nb->setCreationDate(QDateTime::currentDateTimeUtc());
        } else if (dbop == DBUpdate) {
            query = UPDATE_CALENDARS;
            qsize = sizeof(UPDATE_CALENDARS);
        } else if (dbop == DBDelete) {
            query = DELETE_CALENDARS;
            qsize = sizeof(DELETE_CALENDARS);
        } else {
            return false;
        }

        if (!mSem.acquire()) {
            qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
            return false;
        }

        SL3_prepare_v2(mDatabase, query, qsize, &stmt, &tail);

        if ((success = mFormat->modifyCalendars(*nb, dbop, stmt, nb == mStorage->defaultNotebook()))) {
            qCDebug(lcMkcal) << operation << "notebook" << nb->uid() << nb->name() << "in database";
        }

        sqlite3_finalize(stmt);

        if (success) {
            // Don't save the incremented transactionId at the moment,
            // let it be seen as an external modification.
            // Todo: add a method for observers on notebook changes.
            if (!mFormat->incrementTransactionId(nullptr))
                mSavedTransactionId = -1;
        }

        if (!mSem.release()) {
            qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
        }

        if (success) {
            mChanged.resize(0);   // make a change to create signal
        }
    }
    return success;

error:
    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }
    return false;
}
//@endcond

void SqliteStorage::fileChanged(const QString &path)
{
    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return;
    }
    int transactionId;
    if (!d->mFormat->selectMetadata(&transactionId))
        transactionId = d->mSavedTransactionId - 1; // Ensure reload on error
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    if (transactionId != d->mSavedTransactionId) {
        d->mSavedTransactionId = transactionId;
        emitStorageModified(path);
        qCDebug(lcMkcal) << path << "has been modified";
    }
}

void SqliteStorage::virtual_hook(int id, void *data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);
    Q_ASSERT(false);
}
