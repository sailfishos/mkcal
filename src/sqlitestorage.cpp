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
#include "sqlitestorage_p.h"
#include "logging_p.h"

#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/ICalFormat>
using namespace KCalendarCore;

using namespace mKCal;

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class MKCAL_HIDE SqliteStorage::Private: public SqliteStorageImpl
{
public:
    Private(const QTimeZone &timeZone, const QString &databaseName)
        : SqliteStorageImpl(timeZone, databaseName)
    {
    }
    QList<DirectStorageInterface::Observer*> mDirectObservers;
};
//@endcond

SqliteStorage::SqliteStorage(const ExtendedCalendar::Ptr &cal, const QString &databaseName,
                             bool validateNotebooks)
    : ExtendedStorage(cal, validateNotebooks),
      d(new Private(cal->timeZone(), databaseName))
{
}

SqliteStorage::SqliteStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks)
    : SqliteStorage(cal, QString(), validateNotebooks)
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

void SqliteStorage::registerDirectObserver(DirectStorageInterface::Observer *observer)
{
    if (!d->mDirectObservers.contains(observer)) {
        d->mDirectObservers.append(observer);
    }
}

void SqliteStorage::unregisterDirectObserver(DirectStorageInterface::Observer *observer)
{
    d->mDirectObservers.removeAll(observer);
}

bool SqliteStorage::open()
{
    if (!d->open()) {
        return false;
    }
    connect(d->mWatcher, &QFileSystemWatcher::fileChanged,
            this, &SqliteStorage::fileChanged);

    if (!loadNotebooks()) {
        qCWarning(lcMkcal) << "cannot load notebooks from calendar";
        return false;
    }

    return ExtendedStorage::open();
}

bool SqliteStorage::loadIncidences(const DBLoadOperation &dbop)
{
    if (runLoadOperation(dbop)) {
        QMultiHash<QString, Incidence*> incidences;
        bool success = d->loadIncidences(&incidences, dbop);
        incidenceLoaded(DBLoadOperationWrapper(&dbop), success ? 0 : -1, -1, incidences);
        return success;
    } else {
        return true;
    }
}

int SqliteStorage::loadIncidences(const DBLoadDateLimited &dbop,
                                  QDateTime *last, int limit,
                                  bool useDate, bool ignoreEnd)
{
    QMultiHash<QString, Incidence*> incidences;
    int count = d->loadIncidences(&incidences, dbop, limit,
                                  last, useDate, ignoreEnd);
    incidenceLoaded(DBLoadOperationWrapper(&dbop), count, limit, incidences);
    return count;
}

bool SqliteStorage::loadBatch(const QList<DBLoadOperationWrapper> &wrappers)
{
    bool result = true;
    QMultiHash<QString, Incidence*> incidences;
    QList<bool> results;
    for (const DBLoadOperationWrapper &wrapper : wrappers) {
        bool success = d->loadIncidences(&incidences, *wrapper.dbop);
        results << success;
        result = result && success;
    }
    incidenceLoadedByBatch(wrappers, results, incidences);
    return result;
}

Person::List SqliteStorage::loadContacts()
{
    return d->loadContacts();
}

bool SqliteStorage::notifyOpened(const Incidence::Ptr &incidence)
{
    Q_UNUSED(incidence);
    return false;
}

bool SqliteStorage::purgeDeletedIncidences(const Incidence::List &list)
{
    return d->purgeDeletedIncidences(list);
}

bool SqliteStorage::storeIncidences(const QMultiHash<QString, Incidence::Ptr> &additions,
                                    const QMultiHash<QString, Incidence::Ptr> &modifications,
                                    const QMultiHash<QString, Incidence::Ptr> &deletions,
                                    ExtendedStorage::DeleteAction deleteAction)
{
    Incidence::List added, modified, deleted;
    bool success = d->save(calendar(), *this,
                           additions, modifications, deletions,
                           &added, &modified, &deleted, deleteAction);
    const MemoryCalendar::Ptr cal = calendar().staticCast<MemoryCalendar>();
    if (!added.isEmpty()) {
        foreach (DirectStorageInterface::Observer *observer, d->mDirectObservers) {
            observer->storageIncidenceAdded(this, cal.data(), added);
        }
    }
    if (!modified.isEmpty()) {
        foreach (DirectStorageInterface::Observer *observer, d->mDirectObservers) {
            observer->storageIncidenceModified(this, cal.data(), modified);
        }
    }
    if (!deleted.isEmpty()) {
        foreach (DirectStorageInterface::Observer *observer, d->mDirectObservers) {
            observer->storageIncidenceDeleted(this, cal.data(), deleted);
        }
    }
    setUpdated(added, modified, deleted);

    return success;
}

bool SqliteStorage::cancel()
{
    return true;
}

bool SqliteStorage::close()
{
    bool success = ExtendedStorage::close();
    if (success && d->close()) {
        setClosed();
    }
    return success;
}

bool SqliteStorage::insertedIncidences(Incidence::List *list, const QDateTime &after,
                                       const QString &notebookUid)
{
    bool success = d->selectIncidences(list, d->selectInsertedIncidences(after, notebookUid));
    setFinished(!success, success ? "select inserted completed"
                : "error selecting inserted incidences");
    return success;
}

bool SqliteStorage::modifiedIncidences(Incidence::List *list, const QDateTime &after,
                                       const QString &notebookUid)
{
    bool success =  d->selectIncidences(list, d->selectModifiedIncidences(after, notebookUid));
    setFinished(!success, success ? "select modified completed"
                : "error selecting modified incidences");
    return success;
}

bool SqliteStorage::deletedIncidences(Incidence::List *list, const QDateTime &after,
                                      const QString &notebookUid)
{
    bool success = d->selectIncidences(list, d->selectDeletedIncidences(after, notebookUid));
    setFinished(!success, success ? "select deleted completed"
                : "error selecting deleted incidences");
    return success;
}

bool SqliteStorage::allIncidences(Incidence::List *list, const QString &notebookUid)
{
    bool success =  d->selectIncidences(list, d->selectAllIncidences(notebookUid));
    setFinished(!success, success ? "select all completed"
                : "error selecting all incidences");
    return success;
}

bool SqliteStorage::duplicateIncidences(Incidence::List *list, const Incidence::Ptr &incidence,
                                        const QString &notebookUid)
{
    if (!incidence || incidence->summary().isNull()) {
        return false;
    }
    bool success = d->selectIncidences(list, d->selectDuplicatedIncidences(incidence->dtStart(), notebookUid, incidence->summary()));
    setFinished(!success, success ? "select duplicated completed"
                : "error selecting duplicated incidences");
    return success;
}

QDateTime SqliteStorage::incidenceDeletedDate(const Incidence::Ptr &incidence)
{
    if (!incidence) {
        return QDateTime();
    }
    return d->incidenceDeletedDate(incidence->uid(), incidence->recurrenceId());
}

int SqliteStorage::eventCount()
{
    const char *query = SELECT_EVENT_COUNT;
    int qsize = sizeof(SELECT_EVENT_COUNT);

    return d->selectCount(query, qsize);
}

int SqliteStorage::todoCount()
{
    const char *query = SELECT_TODO_COUNT;
    int qsize = sizeof(SELECT_TODO_COUNT);

    return d->selectCount(query, qsize);
}

int SqliteStorage::journalCount()
{
    const char *query = SELECT_JOURNAL_COUNT;
    int qsize = sizeof(SELECT_JOURNAL_COUNT);

    return d->selectCount(query, qsize);
}

bool SqliteStorage::loadNotebooks()
{
    QList<Notebook*> notebooks;
    Notebook* defaultNb;
    bool success = d->loadNotebooks(&notebooks, &defaultNb);
    if (success) {
        setOpened(notebooks, defaultNb);
    }
    return success;
}

Notebook SqliteStorage::loadNotebook(const QString &uid)
{
    Notebook *nb = nullptr;
    if (d->loadNotebook(&nb, uid)) {
        return *nb;
    } else {
        return Notebook();
    }
}

bool SqliteStorage::modifyNotebook(const Notebook::Ptr &nb, DBOperation dbop)
{
    Notebook *old = nullptr;
    if (dbop == DBUpdate && !d->mDirectObservers.isEmpty() && !d->loadNotebook(&old, nb->uid())) {
        qCWarning(lcMkcal) << "cannot find notebook" << nb->uid() << "for database update";
        return false;
    }
    bool success = d->modifyNotebook(*nb, dbop, nb->uid() == defaultNotebook()->uid());
    if (success) {
        if (dbop == DBInsert) {
            foreach (DirectStorageInterface::Observer *observer, d->mDirectObservers) {
                observer->storageNotebookAdded(this, *nb);
            }
        } else if (dbop == DBUpdate) {
            foreach (DirectStorageInterface::Observer *observer, d->mDirectObservers) {
                observer->storageNotebookModified(this, *nb, *old);
            }
        } else if (dbop == DBDelete) {
            foreach (DirectStorageInterface::Observer *observer, d->mDirectObservers) {
                observer->storageNotebookDeleted(this, *nb);
            }
        }
    }
    delete old;
    return success;
}

void SqliteStorage::fileChanged(const QString &path)
{
    if (d->fileChanged()) {
        qCDebug(lcMkcal) << path << "has been modified";
        setModified(path);
    }
}

void SqliteStorage::virtual_hook(int id, void *data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);
    Q_ASSERT(false);
}
