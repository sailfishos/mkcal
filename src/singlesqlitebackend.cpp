/*
  This file is part of the mkcal library.

  Copyright (c) 2023 Damien Caliste <dcaliste@free.fr>

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

#include "singlesqlitebackend_p.h"
#include "sqliteformat.h"
#include "logging_p.h"

#include <QFileSystemWatcher>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_UNIX
#include "semaphore_p.h"
#else
#include <QSystemSemaphore>
#endif

using namespace KCalendarCore;

using namespace mKCal;

static const QString gChanged(QLatin1String(".changed"));

//@cond PRIVATE
class SingleSqliteBackend::Private
{
public:
    Private(const QString &databaseName)
        : mDatabaseName(databaseName)
#ifdef Q_OS_UNIX
        , mSem(databaseName)
#else
        , mSem(databaseName, 1, QSystemSemaphore::Open)
#endif
        , mChanged(databaseName + gChanged)
    {
    }

    QString mDatabaseName;
    SqliteFormat *mFormat = nullptr;
#ifdef Q_OS_UNIX
    ProcessMutex mSem;
#else
    QSystemSemaphore mSem;
#endif
    int mSavedTransactionId = -1;
    QFile mChanged;
    QFileSystemWatcher *mWatcher = nullptr;

    bool mSaving = false;
    QHash<QString, QStringList> mAdded;
    QHash<QString, QStringList> mModified;
    QHash<QString, QStringList> mDeleted;

    bool loadIncidences(QHash<QString, Incidence::List> *list,
                        sqlite3_stmt *stmt1);
    bool loadIncidencesBySeries(QHash<QString, Incidence::List> *list,
                                QHash<QString, QStringList> *identifiers,
                                sqlite3_stmt *stmt1, int limit);
    bool loadNotebooks(Notebook::List *list, Notebook::Ptr *defaultNb,
                       sqlite3_stmt *stmt);
    bool saveNotebook(const Notebook &nb, DBOperation dbop,
                      sqlite3_stmt *stmt, bool isDefault);
};

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
//@endcond

SingleSqliteBackend::SingleSqliteBackend(const QString &databaseName)
    : QObject()
    , d(new Private(databaseName.isEmpty() ? defaultLocation() : databaseName))
{
}

SingleSqliteBackend::~SingleSqliteBackend()
{
    close();
    delete d;
}

QString SingleSqliteBackend::databaseName() const
{
    return d->mDatabaseName;
}

bool SingleSqliteBackend::open()
{
    if (d->mFormat) {
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    d->mFormat = new SqliteFormat(d->mDatabaseName);
    if (!d->mFormat->database()) {
        goto error;
    }
    d->mFormat->selectMetadata(&d->mSavedTransactionId);

    if (!d->mChanged.open(QIODevice::Append)) {
        qCWarning(lcMkcal) << "cannot open changed file for" << d->mDatabaseName;
        goto error;
    }
    d->mWatcher = new QFileSystemWatcher();
    d->mWatcher->addPath(d->mChanged.fileName());
    connect(d->mWatcher, &QFileSystemWatcher::fileChanged,
            this, &SingleSqliteBackend::fileChanged);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    return true;

 error:
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    close();
    return false;
}

bool SingleSqliteBackend::close()
{
    if (d->mWatcher) {
        d->mWatcher->removePaths(d->mWatcher->files());
        // This should work, as storage should be closed before
        // application terminates now. If not, deadlock occurs.
        delete d->mWatcher;
        d->mWatcher = nullptr;
    }
    delete d->mFormat;
    d->mFormat = nullptr;
    d->mChanged.close();
    return true;
}

void SingleSqliteBackend::fileChanged(const QString &path)
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
        qCDebug(lcMkcal) << path << "has been modified";
        emit modified();
    }
}

bool SingleSqliteBackend::Private::loadIncidences(QHash<QString, Incidence::List> *list,
                                                  sqlite3_stmt *stmt1)
{
    Incidence::Ptr incidence;
    QString notebookUid;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    while ((incidence = mFormat->selectComponents(stmt1, notebookUid))) {
        QHash<QString, Incidence::List>::Iterator it = list->find(notebookUid);
        if (it == list->end()) {
            list->insert(notebookUid, Incidence::List() << incidence);
        } else {
            it->append(incidence);
        }
    }

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;
}

bool SingleSqliteBackend::incidences(Incidence::List *list,
                                     const QString &notebookUid,
                                     const QString &uid)
{
    if (!d->mFormat || !list) {
        return false;
    }

    if (notebookUid.isEmpty()) {
        qCWarning(lcMkcal) << "notebook uid must be specified.";
        return false;
    }

    int rv = 0;
    bool success = false;

    const char *query1 = NULL;
    int qsize1 = 0;
    int index = 1;

    sqlite3_stmt *stmt1 = NULL;
    QByteArray u, n;
    QHash<QString, Incidence::List> hash;

    if (!uid.isEmpty()) {
        query1 = SELECT_COMPONENTS_BY_NOTEBOOKUID_AND_UID;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_NOTEBOOKUID_AND_UID);

        SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, NULL);
        n = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
        u = uid.toUtf8();
        SL3_bind_text(stmt1, index, u.constData(), u.length(), SQLITE_STATIC);
    } else {
        query1 = SELECT_COMPONENTS_BY_NOTEBOOKUID;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_NOTEBOOKUID);

        SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, NULL);
        n = notebookUid.toUtf8();
        SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);
    }

    success = d->loadIncidences(&hash, stmt1);
    if (success) {
        list->append(hash.value(notebookUid));
    }

error:
    sqlite3_finalize(stmt1);
    return success;
}

bool SingleSqliteBackend::incidences(QHash<QString, Incidence::List> *list,
                                     const QDateTime &start, const QDateTime &end,
                                     bool loadAllRecurringIncidences)
{
    if (!d->mFormat || !list || (!start.isValid() && !end.isValid())) {
        return false;
    }

    int rv = 0;
    bool success = false;

    const char *query1 = NULL;
    int qsize1 = 0;

    sqlite3_stmt *stmt1 = NULL;
    int index = 1;
    qint64 secsStart;
    qint64 secsEnd;

    // We have no way to know if a recurring incidence
    // is happening within [start, end[, so load them all.
    if (loadAllRecurringIncidences) {
        SL3_prepare_v2(d->mFormat->database(), SELECT_COMPONENTS_BY_RECURSIVE,
                       sizeof(SELECT_COMPONENTS_BY_RECURSIVE), &stmt1, NULL);

        success = d->loadIncidences(list, stmt1);
        if (!success)
            goto error;
    }

    // Load non recurring incidences based on dates.
    if (start.isValid() && end.isValid()) {
        query1 = SELECT_COMPONENTS_BY_DATE_BOTH;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_DATE_BOTH);
        SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, NULL);
        secsStart = d->mFormat->toOriginTime(start);
        secsEnd = d->mFormat->toOriginTime(end);
        SL3_bind_int64(stmt1, index, secsEnd);
        SL3_bind_int64(stmt1, index, secsStart);
        SL3_bind_int64(stmt1, index, secsStart);
    } else if (start.isValid()) {
        query1 = SELECT_COMPONENTS_BY_DATE_START;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_DATE_START);
        SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, NULL);
        secsStart = d->mFormat->toOriginTime(start);
        SL3_bind_int64(stmt1, index, secsStart);
        SL3_bind_int64(stmt1, index, secsStart);
    } else if (end.isValid()) {
        query1 = SELECT_COMPONENTS_BY_DATE_END;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_DATE_END);
        SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, NULL);
        secsEnd = d->mFormat->toOriginTime(end);
        SL3_bind_int64(stmt1, index, secsEnd);
    }

    success = d->loadIncidences(list, stmt1);

 error:
    sqlite3_finalize(stmt1);
    return success;
}

bool SingleSqliteBackend::incidences(QHash<QString, Incidence::List> *list,
                                     const QString &uid)
{
    int rv = 0;
    bool success = false;
 
    const char *query1 = NULL;
    int qsize1 = 0;

    sqlite3_stmt *stmt1 = NULL;
    int index = 1;
    const QByteArray u = uid.toUtf8();

    if (!uid.isEmpty()) {
        query1 = SELECT_COMPONENTS_BY_UID;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_UID);

        SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, NULL);
        SL3_bind_text(stmt1, index, u.constData(), u.length(), SQLITE_STATIC);
    } else {
        query1 = SELECT_COMPONENTS_ALL;
        qsize1 = sizeof(SELECT_COMPONENTS_ALL);

        SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, NULL);
    }
    
    success = d->loadIncidences(list, stmt1);

error:
    sqlite3_finalize(stmt1);
    return success;
}

bool SingleSqliteBackend::deletedIncidences(Incidence::List *list,
                                            const QString &notebookUid)
{
    if (!d->mFormat || !list || notebookUid.isEmpty()) {
        return false;
    }

    const char *query1 = NULL;
    int qsize1 = 0;
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index = 1;
    const QByteArray n = notebookUid.toUtf8();
    QHash<QString, Incidence::List> hash;
    bool success = false;

    query1 = SELECT_COMPONENTS_ALL_DELETED_BY_NOTEBOOK;
    qsize1 = sizeof(SELECT_COMPONENTS_ALL_DELETED_BY_NOTEBOOK);

    qCDebug(lcMkcal) << "incidences deleted";

    SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, nullptr);
    SL3_bind_text(stmt1, index, n.constData(), n.length(), SQLITE_STATIC);

    success = d->loadIncidences(&hash, stmt1);
    if (success) {
        list->append(hash.value(notebookUid));
    }

 error:
    sqlite3_finalize(stmt1);
    return success;
}

static void addId(QHash<QString, QStringList> *hash,
                  const QString &key, const QString &id)
{
    QHash<QString, QStringList>::Iterator it = hash->find(key);
    if (it == hash->end()) {
        hash->insert(key, QStringList() << id);
    } else {
        it->append(id);
    }
}

bool SingleSqliteBackend::Private::loadIncidencesBySeries(QHash<QString, Incidence::List> *list,
                                                          QHash<QString, QStringList> *identifiers,
                                                          sqlite3_stmt *stmt1, int limit)
{
    int count = 0;
    Incidence::Ptr incidence;
    QString notebookUid;
    QSet<QPair<QString, QString>> recurringUids;

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    while ((incidence = mFormat->selectComponents(stmt1, notebookUid))
           && (limit <= 0 || count < limit)) {
        if (incidence->recurs() || incidence->hasRecurrenceId()) {
            recurringUids.insert(QPair<QString, QString>(notebookUid, incidence->uid()));
        } else {
            QHash<QString, Incidence::List>::Iterator it = list->find(notebookUid);
            if (it == list->end()) {
                list->insert(notebookUid, Incidence::List() << incidence);
            } else {
                it->append(incidence);
            }
            // Apply limit on load on non recurring events only.
            count += 1;
        }
        if (identifiers) {
            addId(identifiers, notebookUid, incidence->instanceIdentifier());
        }
    }

    if (recurringUids.count() > 0) {
        // Additionally load any exception or parent to ensure calendar
        // consistency.
        int rv = 0;
        sqlite3_stmt *loadByUid = NULL;
        const char *query1 = NULL;
        int qsize1 = 0;
        query1 = SELECT_COMPONENTS_BY_NOTEBOOKUID_AND_UID;
        qsize1 = sizeof(SELECT_COMPONENTS_BY_NOTEBOOKUID_AND_UID);
        SL3_prepare_v2(mFormat->database(), query1, qsize1, &loadByUid, NULL);

        for (const QPair<QString, QString> &it : const_cast<const QSet<QPair<QString, QString>>&>(recurringUids)) {
            int index = 1;
            const QByteArray nbuid = it.first.toUtf8();
            const QByteArray uid = it.second.toUtf8();
            SL3_reset(loadByUid);
            SL3_bind_text(loadByUid, index, nbuid.constData(), nbuid.length(), SQLITE_STATIC);
            SL3_bind_text(loadByUid, index, uid.constData(), uid.length(), SQLITE_STATIC);
            QHash<QString, Incidence::List>::Iterator incidences = list->find(it.first);
            while ((incidence = mFormat->selectComponents(loadByUid, notebookUid))) {
                if (incidences == list->end()) {
                    list->insert(notebookUid, Incidence::List() << incidence);
                } else {
                    incidences->append(incidence);
                }
            }
        }

    error:
        sqlite3_finalize(loadByUid);
    }

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;
}

bool SingleSqliteBackend::search(QHash<QString, Incidence::List> *list,
                                 QHash<QString, QStringList> *identifiers,
                                 const QString &key, int limit)
{
    if (!d->mFormat || key.isEmpty())
        return false;

    const char *query1 = SEARCH_COMPONENTS;
    int qsize1 = sizeof(SEARCH_COMPONENTS);
    const QByteArray s('%' + key.toUtf8().replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + '%');
    int rv = 0;
    sqlite3_stmt *stmt1 = NULL;
    int index = 1;
    Incidence::Ptr incidence;
    QString nbook;
    bool success = false;

    qCDebug(lcMkcal) << "Searching DB for" << s;
    SL3_prepare_v2(d->mFormat->database(), query1, qsize1, &stmt1, nullptr);
    SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_STATIC);
    SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_STATIC);
    SL3_bind_text(stmt1, index, s.constData(), s.length(), SQLITE_STATIC);

    success = d->loadIncidencesBySeries(list, identifiers, stmt1, limit);

error:
    sqlite3_finalize(stmt1);
    return success;
}

bool SingleSqliteBackend::deferSaving()
{
    int rv = 0;
    char *errmsg = NULL;

    if (!d->mFormat) {
        return false;
    }

    if (d->mSaving) {
        qCWarning(lcMkcal) << "already saving. Call commit() first.";
        return false;
    }

    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return false;
    }

    const char *query = BEGIN_TRANSACTION;
    SL3_exec(d->mFormat->database());

    d->mSaving = true;

    return true;

error:
    return false;
}

bool SingleSqliteBackend::addIncidence(const QString &notebookUid,
                                       const Incidence &incidence)
{
    bool oneshot = !d->mSaving;
    bool success = true;

    if (oneshot && !deferSaving()) {
        return false;
    }
   
    if (d->mFormat->modifyComponents(incidence, notebookUid, DBInsert)) {
        addId(&d->mAdded, notebookUid, incidence.instanceIdentifier());
    } else {
        success = false;
        goto error;
    }

 error:
    if (oneshot && !commit()) {
        return false;
    }
    return success;
}

bool SingleSqliteBackend::modifyIncidence(const QString &notebookUid,
                                          const Incidence &incidence)
{
    bool oneshot = !d->mSaving;
    bool success = true;

    if (oneshot && !deferSaving()) {
        return false;
    }
   
    if (d->mFormat->modifyComponents(incidence, notebookUid, DBUpdate)) {
        addId(&d->mModified, notebookUid, incidence.instanceIdentifier());
    } else {
        success = false;
        goto error;
    }

 error:
    if (oneshot && !commit()) {
        return false;
    }
    return success;
}

bool SingleSqliteBackend::deleteIncidence(const QString &notebookUid,
                                          const Incidence &incidence)
{
    bool oneshot = !d->mSaving;
    bool success = true;

    if (oneshot && !deferSaving()) {
        return false;
    }
   
    if (d->mFormat->modifyComponents(incidence, notebookUid, DBMarkDeleted)) {
        addId(&d->mDeleted, notebookUid, incidence.instanceIdentifier());
    } else {
        success = false;
        goto error;
    }

 error:
    if (oneshot && !commit()) {
        return false;
    }
    return success;
}

bool SingleSqliteBackend::purgeIncidence(const QString &notebookUid,
                                         const Incidence &incidence)
{
    bool oneshot = !d->mSaving;
    bool success = true;

    if (oneshot && !deferSaving()) {
        return false;
    }
   
    if (d->mFormat->modifyComponents(incidence, notebookUid, DBDelete)) {
        addId(&d->mDeleted, notebookUid, incidence.instanceIdentifier());
    } else {
        success = false;
        goto error;
    }

 error:
    if (oneshot && !commit()) {
        return false;
    }
    return success;
}

bool SingleSqliteBackend::commit()
{
    int rv = 0;
    char *errmsg = NULL;

    if (!d->mFormat) {
        return false;
    }

    if (!d->mSaving) {
        qCWarning(lcMkcal) << "nothing to commit. Call deferSaving() first.";
        return false;
    }

    d->mSaving = false;

    const char *query = COMMIT_TRANSACTION;
    SL3_exec(d->mFormat->database());

    if (d->mAdded.count() > 0 || d->mModified.count() > 0 || d->mDeleted.count() > 0)
        d->mFormat->incrementTransactionId(&d->mSavedTransactionId);

    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    if (!d->mAdded.isEmpty() || !d->mModified.isEmpty() || !d->mDeleted.isEmpty()) {
        emit updated(d->mAdded, d->mModified, d->mDeleted);
        d->mChanged.resize(0);   // make a change to create signal
    }
    d->mAdded.clear();
    d->mModified.clear();
    d->mDeleted.clear();

    return true;

error:
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }

    d->mAdded.clear();
    d->mModified.clear();
    d->mDeleted.clear();

    return false;
}

bool SingleSqliteBackend::purgeDeletedIncidences(const QString &notebookUid,
                                                 const Incidence::List &list)
{
    if (!d->mFormat) {
        return false;
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
    SL3_exec(d->mFormat->database());

    error = 0;
    for (const Incidence::Ptr &incidence: list) {
        if (!d->mFormat->purgeDeletedComponents(*incidence, notebookUid)) {
            error += 1;
        }
    }

    query = COMMIT_TRANSACTION;
    SL3_exec(d->mFormat->database());

 error:
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
    return error == 0;
}

bool SingleSqliteBackend::Private::loadNotebooks(Notebook::List *list, Notebook::Ptr *defaultNb, sqlite3_stmt *stmt)
{
    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    bool isDefault;
    Notebook::Ptr nb;
    while ((nb = mFormat->selectCalendars(stmt, &isDefault))) {
        qCDebug(lcMkcal) << "loaded notebook" << nb->uid() << nb->name() << "from database";
        list->append(nb);
        if (isDefault && defaultNb) {
            *defaultNb = nb;
        }
    }

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    return true;
}

bool SingleSqliteBackend::notebooks(Notebook::List *list, Notebook::Ptr *defaultNb)
{
    int rv = 0;
    sqlite3_stmt *stmt = NULL;
    bool success = false;

    if (!d->mFormat || !list) {
        return false;
    }

    const char *query = SELECT_CALENDARS_ALL;
    int qsize = sizeof(SELECT_CALENDARS_ALL);
    SL3_prepare_v2(d->mFormat->database(), query, qsize, &stmt, nullptr);

    success = d->loadNotebooks(list, defaultNb, stmt);

error:
    sqlite3_finalize(stmt);
    return success;
}

bool SingleSqliteBackend::Private::saveNotebook(const Notebook &nb, DBOperation dbop,
                                                sqlite3_stmt *stmt, bool isDefault)
{
    bool success;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";

    if (!mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
        return false;
    }

    if ((success = mFormat->modifyCalendars(nb, dbop, stmt, isDefault))) {
        qCDebug(lcMkcal) << operation << "notebook" << nb.uid() << nb.name() << "in database";
        if (dbop == DBDelete && !mFormat->purgeAllComponents(nb.uid())) {
            qCWarning(lcMkcal) << "cannot purge all incidences from" << nb.uid();
        }

        mFormat->incrementTransactionId(&mSavedTransactionId);
    }

    if (!mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
    }

    if (success) {
        mChanged.resize(0); // make a change to create signal
    }

    return success;
}

bool SingleSqliteBackend::addNotebook(const Notebook &notebook, bool isDefault)
{
    if (!d->mFormat) {
        return false;
    }

    bool success = false;
    sqlite3_stmt *stmt = nullptr;

    int rv = 0;
    SL3_prepare_v2(d->mFormat->database(), INSERT_CALENDARS,
                   sizeof(INSERT_CALENDARS), &stmt, nullptr);

    success = d->saveNotebook(notebook, DBInsert, stmt, isDefault);

 error:
    sqlite3_finalize(stmt);
    return success;
}

bool SingleSqliteBackend::updateNotebook(const Notebook &notebook, bool isDefault)
{
    if (!d->mFormat) {
        return false;
    }

    bool success = false;
    sqlite3_stmt *stmt = nullptr;

    int rv = 0;
    SL3_prepare_v2(d->mFormat->database(), UPDATE_CALENDARS,
                   sizeof(UPDATE_CALENDARS), &stmt, nullptr);

    success = d->saveNotebook(notebook, DBUpdate, stmt, isDefault);

 error:
    sqlite3_finalize(stmt);
    return success;
}

bool SingleSqliteBackend::deleteNotebook(const Notebook &notebook)
{
    if (!d->mFormat) {
        return false;
    }

    bool success = false;
    sqlite3_stmt *stmt = nullptr;

    int rv = 0;
    SL3_prepare_v2(d->mFormat->database(), DELETE_CALENDARS,
                   sizeof(DELETE_CALENDARS), &stmt, nullptr);

    success = d->saveNotebook(notebook, DBDelete, stmt, false);

 error:
    sqlite3_finalize(stmt);
    return success;
}

SqliteFormat* SingleSqliteBackend::acquireDb()
{
    if (!d->mSem.acquire()) {
        qCWarning(lcMkcal) << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
        return nullptr;
    }

    return d->mFormat;
}

void SingleSqliteBackend::releaseDb()
{
    if (!d->mSem.release()) {
        qCWarning(lcMkcal) << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
}
