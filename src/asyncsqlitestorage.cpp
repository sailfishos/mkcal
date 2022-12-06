/*
  This file is part of the mkcal library.

  Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>

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
#include "asyncsqlitestorage.h"
#include "sqlitestorage_p.h"
#include "logging_p.h"

#include <QObject>
#include <QThread>

using namespace KCalendarCore;
using namespace mKCal;

class MKCAL_HIDE SqliteStorageWorker
    : public QObject
    , public SqliteStorageImpl
    , public DirectStorageInterface
{
    Q_OBJECT

 public:
    SqliteStorageWorker(const QTimeZone &timeZone, const QString &databaseName)
        : QObject(), SqliteStorageImpl(timeZone, databaseName)
    {
    }
    ~SqliteStorageWorker()
    {
    }
 public slots:
    void open()
    {
        if (SqliteStorageImpl::open()) {
            connect(mWatcher, &QFileSystemWatcher::fileChanged,
                    this, &SqliteStorageWorker::fileChanged);
            loadNotebooks();
        }
    }
    void close()
    {
        if (SqliteStorageImpl::close()) {
            emit closed();
        }
    }
    void fileChanged(const QString &path)
    {
        if (SqliteStorageImpl::fileChanged()) {
            qCDebug(lcMkcal) << path << "has been modified";
            emit modified(path);
        }
    }
    void registerDirectObserver(DirectStorageInterface::Observer *observer)
    {
        if (!mDirectObservers.contains(observer)) {
            mDirectObservers.append(observer);
        }
    }
    void unregisterDirectObserver(DirectStorageInterface::Observer *observer)
    {
        mDirectObservers.removeAll(observer);
    }
    void loadNotebooks()
    {
        QList<Notebook*> notebooks;
        Notebook* defaultNb;
        if (SqliteStorageImpl::loadNotebooks(&notebooks, &defaultNb)) {
            emit notebookLoaded(notebooks, defaultNb);
        }
    }
    Notebook loadNotebook(const QString &uid) override
    {
        Notebook *nb = nullptr;
        if (SqliteStorageImpl::loadNotebook(&nb, uid)) {
            return *nb;
        } else {
            return Notebook();
        }
    }
    void modifyNotebook(Notebook nb, DBOperation dbop, bool isDefault)
    {
        Notebook *old = nullptr;
        if (dbop == DBUpdate && !mDirectObservers.isEmpty() && !SqliteStorageImpl::loadNotebook(&old, nb.uid())) {
            qCWarning(lcMkcal) << "cannot find notebook" << nb.uid() << "for database update";
            return;
        }
        bool success = SqliteStorageImpl::modifyNotebook(nb, dbop, isDefault);
        if (success) {
            if (dbop == DBInsert) {
                foreach (DirectStorageInterface::Observer *observer, mDirectObservers) {
                    observer->storageNotebookAdded(this, nb);
                }
            } else if (dbop == DBUpdate) {
                foreach (DirectStorageInterface::Observer *observer, mDirectObservers) {
                    observer->storageNotebookModified(this, nb, *old);
                }
            } else if (dbop == DBDelete) {
                foreach (DirectStorageInterface::Observer *observer, mDirectObservers) {
                    observer->storageNotebookDeleted(this, nb);
                }
            }
        }
        delete old;
    }
    void loadIncidences(const DBLoadOperationWrapper &wrapper)
    {
        QMultiHash<QString, Incidence*> incidences;
        bool success = SqliteStorageImpl::loadIncidences(&incidences, *wrapper.dbop);
        emit incidenceLoaded(wrapper, success ? 0 : -1, -1, incidences);
    }
    void loadLimitedIncidences(const DBLoadOperationWrapper &wrapper, int limit,
                               QDateTime *last, bool useDate, bool ignoreEnd)
    {
        QMultiHash<QString, Incidence*> incidences;
        int count = SqliteStorageImpl::loadIncidences(&incidences,
                                                      *static_cast<const DBLoadDateLimited*>(wrapper.dbop),
                                                      limit, last, useDate, ignoreEnd);
        emit incidenceLoaded(wrapper, count, limit, incidences);
    }
    void loadBatch(const QList<DBLoadOperationWrapper> &wrappers)
    {
        QList<bool> results;
        QMultiHash<QString, Incidence*> incidences;
        for (const DBLoadOperationWrapper &wrapper : wrappers) {
            results << SqliteStorageImpl::loadIncidences(&incidences, *wrapper.dbop);
        }
        emit incidenceLoadedByBatch(wrappers, results, incidences);
    }
    void save(const MemoryCalendar *calendar,
              const QStringList &toAdd,
              const QStringList &toUpdate,
              const QStringList &toDelete,
              ExtendedStorage::DeleteAction deleteAction)
    {
        QStringList added, modified, deleted;
        SqliteStorageImpl::save(calendar, toAdd, toUpdate, toDelete,
                                &added, &modified, &deleted, deleteAction);
        if (!added.isEmpty()) {
            Incidence::List list;
            for (const QString &id : added) {
                Incidence::Ptr incidence = calendar->instance(id);
                if (incidence) {
                    list << incidence;
                }
            }
            foreach (DirectStorageInterface::Observer *observer, mDirectObservers) {
                observer->storageIncidenceAdded(this, calendar, list);
            }
        }
        if (!modified.isEmpty()) {
            Incidence::List list;
            for (const QString &id : modified) {
                Incidence::Ptr incidence = calendar->instance(id);
                if (incidence) {
                    list << incidence;
                }
            }
            foreach (DirectStorageInterface::Observer *observer, mDirectObservers) {
                observer->storageIncidenceModified(this, calendar, list);
            }
        }
        if (!deleted.isEmpty()) {
            Incidence::List list;
            for (const QString &id : deleted) {
                Incidence::Ptr incidence = calendar->instance(id);
                if (incidence) {
                    list << incidence;
                }
            }
            foreach (DirectStorageInterface::Observer *observer, mDirectObservers) {
                observer->storageIncidenceDeleted(this, calendar, list);
            }
        }
        emit incidenceSaved(calendar, added, modified, deleted);
    }
    bool insertedIncidences(Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid)
    {
        return selectIncidences(list, selectInsertedIncidences(after, notebookUid));
    }
    bool modifiedIncidences(Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid)
    {
        return selectIncidences(list, selectModifiedIncidences(after, notebookUid));
    }
    bool deletedIncidences(Incidence::List *list, const QDateTime &after,
                           const QString &notebookUid)
    {
        return selectIncidences(list, selectDeletedIncidences(after, notebookUid));
    }
    bool allIncidences(Incidence::List *list, const QString &notebookUid)
    {
        return selectIncidences(list, selectAllIncidences(notebookUid));
    }
    bool duplicateIncidences(Incidence::List *list,
                             const QDateTime &after,
                             const QString &notebookUid,
                             const QString &summary)
    {
        return selectIncidences(list, selectDuplicatedIncidences(after, notebookUid, summary));
    }
    bool duplicateIncidences(Incidence::List *list,
                             const Incidence::Ptr &incidence,
                             const QString &notebookUid)
    {
        if (!incidence || incidence->summary().isNull()) {
            return false;
        }
        return duplicateIncidences(list, incidence->dtStart(), notebookUid, incidence->summary());
    }
    void purgeDeleted(const QList<IncidenceId> &toDelete)
    {
        SqliteStorageImpl::purgeDeletedIncidences(toDelete);
    }
    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list)
    {
        return SqliteStorageImpl::purgeDeletedIncidences(list);
    }
    int eventCount()
    {
        const char *query = SELECT_EVENT_COUNT;
        int qsize = sizeof(SELECT_EVENT_COUNT);

        return selectCount(query, qsize);
    }
    int todoCount()
    {
        const char *query = SELECT_TODO_COUNT;
        int qsize = sizeof(SELECT_TODO_COUNT);

        return selectCount(query, qsize);
    }
    int journalCount()
    {
        const char *query = SELECT_JOURNAL_COUNT;
        int qsize = sizeof(SELECT_JOURNAL_COUNT);

        return selectCount(query, qsize);
    }
    QDateTime incidenceDeletedDate(const Incidence::Ptr &incidence)
    {
        if (!incidence) {
            return QDateTime();
        }
        return SqliteStorageImpl::incidenceDeletedDate(incidence->uid(), incidence->recurrenceId());
    }
    Person::List loadContacts()
    {
        return SqliteStorageImpl::loadContacts();
    }
 signals:
    void closed();
    void modified(const QString &path);
    void notebookLoaded(const QList<Notebook*> &notebooks, Notebook *defaultNb);
    void incidenceSaved(const MemoryCalendar *calendar, const QStringList &added,
                        const QStringList &modified, const QStringList &deleted);
    void incidenceLoaded(const DBLoadOperationWrapper &wrapper, int count, int limit,
                         const QMultiHash<QString, Incidence*> &incidences);
    void incidenceLoadedByBatch(const QList<DBLoadOperationWrapper> &wrappers,
                                const QList<bool> &results,
                                const QMultiHash<QString, Incidence*> &incidences);
 private:
    QList<DirectStorageInterface::Observer*> mDirectObservers;
};

class MKCAL_HIDE AsyncSqliteStorage::Private: public QObject
{
    Q_OBJECT
public:
    Private(const QTimeZone &timeZone, AsyncSqliteStorage *storage,
            const QString &databaseName)
        : mSqliteWorker(new SqliteStorageWorker(timeZone, databaseName))
    {
        mSqliteWorker->moveToThread(&mWorkerThread);
        connect(&mWorkerThread, &QThread::finished,
                mSqliteWorker, &QObject::deleteLater);
        connect(mSqliteWorker, &SqliteStorageWorker::modified,
                storage, &AsyncSqliteStorage::setModified);
        connect(mSqliteWorker, &SqliteStorageWorker::notebookLoaded,
                storage, &AsyncSqliteStorage::setOpened);
        connect(mSqliteWorker, &SqliteStorageWorker::closed,
                storage, &AsyncSqliteStorage::setClosed);
        connect(mSqliteWorker, &SqliteStorageWorker::incidenceSaved,
                storage, &AsyncSqliteStorage::incidenceSaved);
        connect(mSqliteWorker, &SqliteStorageWorker::incidenceLoaded,
                storage, &AsyncSqliteStorage::incidenceLoaded);
        connect(mSqliteWorker, &SqliteStorageWorker::incidenceLoadedByBatch,
                storage, &AsyncSqliteStorage::incidenceLoadedByBatch);
        mWorkerThread.setObjectName("SqliteWorker");
        mWorkerThread.start();
    }
    ~Private()
    {
    }
    SqliteStorageWorker *mSqliteWorker;
    QThread mWorkerThread;
};

AsyncSqliteStorage::AsyncSqliteStorage(const ExtendedCalendar::Ptr &cal, const QString &databaseName,
                                       bool validateNotebooks)
    : ExtendedStorage(cal, validateNotebooks),
      d(new Private(cal->timeZone(), this, databaseName))
{
    qRegisterMetaType<QDateTime*>("QDateTime*");
    qRegisterMetaType<Notebook>("Notebook");
    qRegisterMetaType<DBOperation>("DBOperation");
    qRegisterMetaType<DBLoadOperationWrapper>("DBLoadOperationWrapper");
    qRegisterMetaType<QList<DBLoadOperationWrapper>>("QList<DBLoadOperationWrapper>");
    qRegisterMetaType<QList<bool>>("QList<bool>");
    qRegisterMetaType<QMultiHash<QString, Incidence*>>("QMultiHash<QString, Incidence*>");
    qRegisterMetaType<DirectStorageInterface::Observer*>("DirectStorageInterface::Observer*");
    qRegisterMetaType<QList<Notebook*>>("QList<Notebook*>");
    qRegisterMetaType<const MemoryCalendar*>("const MemoryCalendar*");
    qRegisterMetaType<ExtendedStorage::DeleteAction>("ExtendedStorage::DeleteAction");
    qRegisterMetaType<QList<IncidenceId>>("QList<IncidenceId>");
}

AsyncSqliteStorage::~AsyncSqliteStorage()
{
    d->mWorkerThread.quit();
    d->mWorkerThread.wait();
    delete d;
}

QString AsyncSqliteStorage::databaseName() const
{
    return d->mSqliteWorker->mDatabaseName;
}

bool AsyncSqliteStorage::open()
{
    QMetaObject::invokeMethod(d->mSqliteWorker, "open", Qt::QueuedConnection);
    return ExtendedStorage::open();
}

bool AsyncSqliteStorage::cancel()
{
    return true;
}

bool AsyncSqliteStorage::close()
{
    QMetaObject::invokeMethod(d->mSqliteWorker, "close", Qt::QueuedConnection);
    return ExtendedStorage::close();
}

void AsyncSqliteStorage::registerDirectObserver(DirectStorageInterface::Observer *observer)
{
    QMetaObject::invokeMethod(d->mSqliteWorker, "registerObserver",
                              Qt::QueuedConnection,
                              Q_ARG(DirectStorageInterface::Observer*, observer));
}

void AsyncSqliteStorage::unregisterDirectObserver(DirectStorageInterface::Observer *observer)
{
    QMetaObject::invokeMethod(d->mSqliteWorker, "unregisterObserver",
                              Qt::QueuedConnection,
                              Q_ARG(DirectStorageInterface::Observer*, observer));
}

bool AsyncSqliteStorage::loadIncidences(const DBLoadOperation &dbop)
{
    if (runLoadOperation(dbop)) {
        DBLoadOperationWrapper wrapper(&dbop);
        QMetaObject::invokeMethod(d->mSqliteWorker, "loadIncidences", Qt::QueuedConnection,
                                  Q_ARG(DBLoadOperationWrapper, wrapper));
    }
    return true;
}

int AsyncSqliteStorage::loadIncidences(const DBLoadDateLimited &dbop,
                                       QDateTime *last, int limit,
                                       bool useDate, bool ignoreEnd)
{
    int count;
    DBLoadOperationWrapper wrapper(&dbop);
    QMetaObject::invokeMethod(d->mSqliteWorker, "loadLimitedIncidences",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(DBLoadOperationWrapper, wrapper),
                              Q_ARG(int, limit),
                              Q_ARG(QDateTime*, last),
                              Q_ARG(bool, useDate),
                              Q_ARG(bool, ignoreEnd));
    return count;
}

bool AsyncSqliteStorage::loadBatch(const QList<DBLoadOperationWrapper> &wrappers)
{
    if (!wrappers.isEmpty()) {
        QMetaObject::invokeMethod(d->mSqliteWorker, "loadBatch",
                                  Qt::QueuedConnection,
                                  Q_ARG(QList<DBLoadOperationWrapper>, wrappers));
    }
    return true;
}

bool AsyncSqliteStorage::notifyOpened(const Incidence::Ptr &incidence)
{
    Q_UNUSED(incidence);
    return false;
}

bool AsyncSqliteStorage::loadNotebooks()
{
    QMetaObject::invokeMethod(d->mSqliteWorker, "loadNotebooks", Qt::QueuedConnection);
    return true;
}

Notebook AsyncSqliteStorage::loadNotebook(const QString &uid)
{
    Notebook nb;
    QMetaObject::invokeMethod(d->mSqliteWorker, "loadNotebook",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(Notebook, nb),
                              Q_ARG(QString, uid));
    return nb;
}

bool AsyncSqliteStorage::modifyNotebook(const Notebook::Ptr &nb, DBOperation dbop)
{
    QMetaObject::invokeMethod(d->mSqliteWorker, "modifyNotebook",
                              Qt::QueuedConnection,
                              Q_ARG(Notebook, *nb),
                              Q_ARG(DBOperation, dbop),
                              Q_ARG(bool, nb->uid() == defaultNotebook()->uid()));
    return true;
}

static QString duplicate(const Calendar &calendar, const Incidence::Ptr &incidence,
                         MemoryCalendar *savedIncidences)
{
    Incidence::Ptr dup = savedIncidences->incidence(incidence->uid(), incidence->recurrenceId());
    if (dup) {
        return dup->instanceIdentifier();
    }
    const QString notebookUid = calendar.notebook(incidence);
    savedIncidences->addNotebook(notebookUid, calendar.isVisible(notebookUid));
    Incidence::Ptr storeParent;
    if (incidence->recurs() || incidence->hasRecurrenceId()) {
        storeParent = calendar.incidence(incidence->uid());
    }
    if (storeParent) {
        Incidence::Ptr parent(storeParent->clone());
        savedIncidences->addIncidence(parent);
        savedIncidences->setNotebook(parent, notebookUid);
        for (Incidence::Ptr &storeException : calendar.instances(parent)) {
            Incidence::Ptr exception(storeException->clone());
            savedIncidences->addIncidence(exception);
            savedIncidences->setNotebook(exception, notebookUid);
        }
        dup = savedIncidences->incidence(incidence->uid(),
                                         incidence->recurrenceId());
    } else {
        dup = Incidence::Ptr(incidence->clone());
        savedIncidences->addIncidence(dup);
        savedIncidences->setNotebook(dup, notebookUid);
    }
    return dup->instanceIdentifier();
}

bool AsyncSqliteStorage::storeIncidences(const QMultiHash<QString, Incidence::Ptr> &additions,
                                         const QMultiHash<QString, Incidence::Ptr> &modifications,
                                         const QMultiHash<QString, Incidence::Ptr> &deletions,
                                         ExtendedStorage::DeleteAction deleteAction)
{
    MemoryCalendar *savedIncidences = new MemoryCalendar(calendar()->timeZone());

    QMultiHash<QString, Incidence::Ptr>::ConstIterator it;

    QStringList toAdd;
    for (it = additions.constBegin(); it != additions.constEnd(); it++) {
        const QString uid = duplicate(*calendar(), *it, savedIncidences);
        toAdd.append(uid);
    }
    QStringList toUpdate;
    for (it = modifications.constBegin(); it != modifications.constEnd(); it++) {
        const QString uid = duplicate(*calendar(), *it, savedIncidences);
        toUpdate.append(uid);
    }
    QStringList toDelete;
    for (it = deletions.constBegin(); it != deletions.constEnd(); it++) {
        Incidence::Ptr incidence((*it)->clone());
        savedIncidences->addIncidence(incidence);
        toDelete.append(incidence->instanceIdentifier());
    }
    QMetaObject::invokeMethod(d->mSqliteWorker, "save",
                              Qt::QueuedConnection,
                              Q_ARG(const MemoryCalendar*, savedIncidences),
                              Q_ARG(QStringList, toAdd),
                              Q_ARG(QStringList, toUpdate),
                              Q_ARG(QStringList, toDelete),
                              Q_ARG(ExtendedStorage::DeleteAction, deleteAction));
    return true;
}

static Incidence::List idsToIncidences(const MemoryCalendar *saveCalendar,
                                       const QStringList &ids,
                                       const Calendar::Ptr &calendar)
{
    Incidence::List list;
    for (const QString &id : ids) {
        Incidence::Ptr incidence = saveCalendar->instance(id);
        if (incidence) {
            Incidence::Ptr calIncidence = calendar->incidence(incidence->uid(), incidence->recurrenceId());
            if (calIncidence) {
                list << calIncidence;
            }
        }
    }
    return list;
}

void AsyncSqliteStorage::incidenceSaved(const MemoryCalendar *saveCalendar,
                                        const QStringList &added,
                                        const QStringList &modified,
                                        const QStringList &deleted)
{
    // Only report incidences that are still in-memory.
    const Incidence::List additions = idsToIncidences(saveCalendar, added, calendar());
    const Incidence::List modifications = idsToIncidences(saveCalendar, modified, calendar());
    Incidence::List deletions;
    const Incidence::List all = calendar()->incidences(QString()); // List all including deleted ones.
    for (const QString &id : deleted) {
        Incidence::Ptr incidence = saveCalendar->instance(id);
        if (incidence) {
            // Very hakish way to get the pointer to the actual deleted incidences from calendar()
            for (const Incidence::Ptr &calIncidence : all) {
                if (calIncidence->uid() == incidence->uid() && calIncidence->recurrenceId() == incidence->recurrenceId()) {
                    deletions << calIncidence;
                }
            }
        }
    }
    setUpdated(additions, modifications, deletions);
    delete saveCalendar;
}

bool AsyncSqliteStorage::insertedIncidences(Incidence::List *list, const QDateTime &after,
                                            const QString &notebookUid)
{
    bool success;
    QMetaObject::invokeMethod(d->mSqliteWorker, "insertedIncidences",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, success),
                              Q_ARG(Incidence::List*, list),
                              Q_ARG(QDateTime, after),
                              Q_ARG(QString, notebookUid));
    setFinished(!success, success ? "select inserted completed"
                : "error selecting inserted incidences");
    return success;
}

bool AsyncSqliteStorage::modifiedIncidences(Incidence::List *list, const QDateTime &after,
                                            const QString &notebookUid)
{
    bool success;
    QMetaObject::invokeMethod(d->mSqliteWorker, "modifiedIncidences",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, success),
                              Q_ARG(Incidence::List*, list),
                              Q_ARG(QDateTime, after),
                              Q_ARG(QString, notebookUid));
    setFinished(!success, success ? "select updated completed"
                : "error selecting updated incidences");
    return success;
}

bool AsyncSqliteStorage::deletedIncidences(Incidence::List *list, const QDateTime &after,
                                           const QString &notebookUid)
{
    bool success;
    QMetaObject::invokeMethod(d->mSqliteWorker, "deletedIncidences",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, success),
                              Q_ARG(Incidence::List*, list),
                              Q_ARG(QDateTime, after),
                              Q_ARG(QString, notebookUid));
    setFinished(!success, success ? "select deleted completed"
                : "error selecting updated incidences");
    return success;
}

bool AsyncSqliteStorage::allIncidences(Incidence::List *list,
                                       const QString &notebookUid)
{
    bool success;
    QMetaObject::invokeMethod(d->mSqliteWorker, "allIncidences",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, success),
                              Q_ARG(Incidence::List*, list),
                              Q_ARG(QString, notebookUid));
    setFinished(!success, success ? "select all completed"
                : "error selecting all incidences");
    return success;
}

bool AsyncSqliteStorage::duplicateIncidences(Incidence::List *list,
                                             const Incidence::Ptr &incidence,
                                             const QString &notebookUid)
{
    if (!incidence || incidence->summary().isNull()) {
        return false;
    }
    bool success;
    QMetaObject::invokeMethod(d->mSqliteWorker, "duplicateIncidences",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, success),
                              Q_ARG(Incidence::List*, list),
                              Q_ARG(QDateTime, incidence->dtStart()),
                              Q_ARG(QString, notebookUid),
                              Q_ARG(QString, incidence->summary()));
    setFinished(!success, success ? "select all completed"
                : "error selecting all incidences");
    return success;
}

bool AsyncSqliteStorage::purgeDeletedIncidences(const Incidence::List &list)
{
    QList<IncidenceId> toDelete;
    for (const Incidence::Ptr &incidence : list) {
        toDelete.append(IncidenceId{incidence->uid(), incidence->recurrenceId()});    }
    QMetaObject::invokeMethod(d->mSqliteWorker, "purgeDeleted",
                              Qt::QueuedConnection,
                              Q_ARG(QList<IncidenceId>, toDelete));
    return true;
}

QDateTime AsyncSqliteStorage::incidenceDeletedDate(const Incidence::Ptr &incidence)
{
    if (!incidence) {
        return QDateTime();
    }
    QDateTime dt;
    IncidenceId id{incidence->uid(), incidence->recurrenceId()};
    QMetaObject::invokeMethod(d->mSqliteWorker, "incidenceDeletedDate",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QDateTime, dt),
                              Q_ARG(IncidenceId, id));
    return dt;
}

int AsyncSqliteStorage::eventCount()
{
    const char *query = SELECT_EVENT_COUNT;
    int qsize = sizeof(SELECT_EVENT_COUNT);

    int count;
    QMetaObject::invokeMethod(d->mSqliteWorker, "selectCount",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(const char*, query),
                              Q_ARG(int, qsize));
    return count;
}

int AsyncSqliteStorage::todoCount()
{
    const char *query = SELECT_TODO_COUNT;
    int qsize = sizeof(SELECT_TODO_COUNT);

    int count;
    QMetaObject::invokeMethod(d->mSqliteWorker, "selectCount",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(const char*, query),
                              Q_ARG(int, qsize));
    return count;
}

int AsyncSqliteStorage::journalCount()
{
    const char *query = SELECT_JOURNAL_COUNT;
    int qsize = sizeof(SELECT_JOURNAL_COUNT);

    int count;
    QMetaObject::invokeMethod(d->mSqliteWorker, "selectCount",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(const char*, query),
                              Q_ARG(int, qsize));
    return count;
}

Person::List AsyncSqliteStorage::loadContacts()
{
    Person::List list;
    QMetaObject::invokeMethod(d->mSqliteWorker, "loadContacts",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(Person::List, list));
    return list;
}

void AsyncSqliteStorage::virtual_hook(int id, void *data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);
    Q_ASSERT(false);
}

#include "asyncsqlitestorage.moc"
