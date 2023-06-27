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
#include "singlesqlitebackend_p.h"
#include "sqliteformat.h"
#include "logging_p.h"

#include <KCalendarCore/MemoryCalendar>
using namespace KCalendarCore;

using namespace mKCal;

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class SqliteStorage::Private
{
public:
    Private(const ExtendedCalendar::Ptr &calendar, SqliteStorage *storage,
            const QString &databaseName
           )
        : mCalendar(calendar),
          mStorage(storage),
          mBackend(databaseName),
          mIsLoading(false)
    {}
    ~Private()
    {
    }

    ExtendedCalendar::Ptr mCalendar;
    SqliteStorage *mStorage;
    SingleSqliteBackend mBackend;

    QHash<QString, Incidence::Ptr> mIncidencesToInsert;
    QHash<QString, Incidence::Ptr> mIncidencesToUpdate;
    QHash<QString, Incidence::Ptr> mIncidencesToDelete;
    bool mIsLoading;

    bool addIncidence(const Incidence::Ptr &incidence, const QString &notebookUid);
    bool addIncidences(const Incidence::List &incidences, const QString &notebookUid);
    bool addIncidences(const QHash<QString, Incidence::List> &incidences);
    bool saveIncidences(QHash<QString, Incidence::Ptr> &list, DBOperation dbop);
};
//@endcond

SqliteStorage::SqliteStorage(const ExtendedCalendar::Ptr &cal, const QString &databaseName,
                             bool validateNotebooks)
    : ExtendedStorage(cal, validateNotebooks),
      d(new Private(cal, this, databaseName))
{
    connect(&d->mBackend, &SingleSqliteBackend::modified,
            this, &SqliteStorage::onModified);
    connect(&d->mBackend, &SingleSqliteBackend::updated,
            this, &SqliteStorage::onUpdated);
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
    return d->mBackend.databaseName();
}

bool SqliteStorage::open()
{
    if (d->mBackend.open()) {
        return loadNotebooks();
    } else {
        qCWarning(lcMkcal) << "cannot open database";
        return false;
    }
}

bool SqliteStorage::close()
{
    return d->mBackend.close() && ExtendedStorage::close();
}

bool SqliteStorage::load()
{
    bool success = true;

    for (const Notebook::Ptr &nb : notebooks()) {
        Incidence::List list;
        success = (d->mBackend.incidences(&list, nb->uid()) && d->addIncidences(list, nb->uid())) && success;
    }

    setIsRecurrenceLoaded(success);
    if (success) {
        addLoadedRange(QDate(), QDate());
    }

    return success;
}

bool SqliteStorage::load(const QString &uid)
{
    // Don't reload an existing incidence from DB.
    // Either the calendar is already in sync with
    // the calendar or the database has been externally
    // modified and in that case, the calendar has been emptied.
    if (calendar()->incidence(uid)) {
        return true;
    }

    QHash<QString, Incidence::List> hash;
    return d->mBackend.incidences(&hash, uid) && d->addIncidences(hash);
}

bool SqliteStorage::load(const QDate &start, const QDate &end)
{
    bool success = true;
    QDateTime loadStart, loadEnd;
    if (getLoadDates(start, end, &loadStart, &loadEnd)) {
        bool loadAllRecurringIncidences = !isRecurrenceLoaded();
        QHash<QString, Incidence::List> hash;
        success = d->mBackend.incidences(&hash, loadStart, loadEnd, loadAllRecurringIncidences) && d->addIncidences(hash);

        if (success) {
            addLoadedRange(loadStart.date(), loadEnd.date());
        }
        if (loadAllRecurringIncidences) {
            setIsRecurrenceLoaded(success);
        }
    }

    return success;
}

bool SqliteStorage::loadNotebookIncidences(const QString &notebookUid)
{
    Incidence::List list;
    return d->mBackend.incidences(&list, notebookUid) && d->addIncidences(list, notebookUid);
}

bool SqliteStorage::search(const QString &key, QStringList *identifiers, int limit)
{
    if (!identifiers) {
        return false;
    }
    QHash<QString, Incidence::List> hash;
    QHash<QString, QStringList> ids;
    if (d->mBackend.search(&hash, &ids, key, limit) && d->addIncidences(hash)) {
        for (QHash<QString, QStringList>::ConstIterator it = ids.constBegin();
             it != ids.constEnd(); it++) {
            identifiers->append(it.value());
        }
        return true;
    }
    return false;
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

bool SqliteStorage::Private::addIncidences(const Incidence::List &incidences, const QString &notebookUid)
{
    mIsLoading = true;
    for (const Incidence::Ptr &incidence : incidences) {
        addIncidence(incidence, notebookUid);
    }
    mIsLoading = false;

    return true;
}

bool SqliteStorage::Private::addIncidences(const QHash<QString, Incidence::List> &incidences)
{
    for (QHash<QString, Incidence::List>::ConstIterator it =  incidences.constBegin();
         it != incidences.constEnd(); it++) {
        addIncidences(it.value(), it.key());
    }

    return true;
}
//@endcond

bool SqliteStorage::purgeDeletedIncidences(const KCalendarCore::Incidence::List &list,
                                           const QString &notebookUid)
{
    return d->mBackend.purgeDeletedIncidences(notebookUid, list);
}

bool SqliteStorage::save()
{
    return save(ExtendedStorage::MarkDeleted);
}

bool SqliteStorage::save(ExtendedStorage::DeleteAction deleteAction)
{
    if (!d->mBackend.deferSaving()) {
        return false;
    }

    int errors = 0;

    // Incidences to insert
    if (!d->mIncidencesToInsert.isEmpty()
        && !d->saveIncidences(d->mIncidencesToInsert, DBInsert)) {
        errors++;
    }

    // Incidences to update
    if (!d->mIncidencesToUpdate.isEmpty()
        && !d->saveIncidences(d->mIncidencesToUpdate, DBUpdate)) {
        errors++;
    }

    // Incidences to delete
    if (!d->mIncidencesToDelete.isEmpty()) {
        DBOperation dbop = deleteAction == ExtendedStorage::PurgeDeleted ? DBDelete : DBMarkDeleted;
        if (!d->saveIncidences(d->mIncidencesToDelete, dbop)) {
            errors++;
        }
    }

    if (!d->mBackend.commit()) {
        errors++;
    }

    d->mIncidencesToInsert.clear();
    d->mIncidencesToUpdate.clear();
    d->mIncidencesToDelete.clear();

    if (errors == 0) {
        emitStorageFinished(false, "save completed");
    } else {
        emitStorageFinished(true, "errors saving incidences");
    }

    return errors == 0;
}

//@cond PRIVATE
bool SqliteStorage::Private::saveIncidences(QHash<QString, Incidence::Ptr> &list, DBOperation dbop)
{
    int errors = 0;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";
    QHash<QString, Incidence::Ptr>::const_iterator it;

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
        bool success = false;
        qCDebug(lcMkcal) << operation << "incidence" << (*it)->uid() << "notebook" << notebookUid;
        if (dbop == DBInsert) {
            success = mBackend.addIncidence(notebookUid, **it);
        } else if (dbop == DBUpdate) {
            success = mBackend.modifyIncidence(notebookUid, **it);
        } else if (dbop == DBMarkDeleted) {
            success = mBackend.deleteIncidence(notebookUid, **it);
        } else if (dbop == DBDelete) {
            success = mBackend.purgeIncidence(notebookUid, **it);
        }
        if (!success) {
            errors++;
        }
    }

    // TODO What if there were errors? Options: 1) rollback 2) best effort.

    return errors == 0;
}

void SqliteStorage::onModified()
{
    emitStorageModified(d->mBackend.databaseName());
}

static Incidence::List toIncidences(const QHash<QString, Incidence::Ptr> &incidences,
                                    const QHash<QString, QStringList> &hash)
{
    Incidence::List list;
    
    QHash<QString, QStringList>::ConstIterator it;
    for (it = hash.constBegin(); it != hash.constEnd(); it++) {
        for (const QString &id : it.value()) {
            const Incidence::Ptr inc = incidences.value(id);
            if (inc) {
                list.append(inc);
            }
        }
    }

    return list;
}

void SqliteStorage::onUpdated(const QHash<QString, QStringList> &added,
                              const QHash<QString, QStringList> &modified,
                              const QHash<QString, QStringList> &deleted)
{
    const Incidence::List additions = toIncidences(d->mIncidencesToInsert, added);
    const Incidence::List modifications = toIncidences(d->mIncidencesToUpdate, modified);
    const Incidence::List deletions = toIncidences(d->mIncidencesToDelete, deleted);
    if (!additions.isEmpty() || !modifications.isEmpty() || !deletions.isEmpty()) {
        emitStorageUpdated(additions, modifications, deletions);
    }
}
//@endcond

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
    if (!notebookUid.isEmpty()) {
        return d->mBackend.insertedIncidences(list, notebookUid, after);
    } else {
        bool success = true;
        for (const Notebook::Ptr &nb : notebooks()) {
            success = d->mBackend.insertedIncidences(list, nb->uid(), after) && success;
        }
        return success;
    }
}

bool SqliteStorage::modifiedIncidences(Incidence::List *list, const QDateTime &after,
                                       const QString &notebookUid)
{
    if (!notebookUid.isEmpty()) {
        return d->mBackend.modifiedIncidences(list, notebookUid, after);
    } else {
        bool success = true;
        for (const Notebook::Ptr &nb : notebooks()) {
            success = d->mBackend.modifiedIncidences(list, nb->uid(), after) && success;
        }
        return success;
    }
}

bool SqliteStorage::deletedIncidences(Incidence::List *list, const QDateTime &after,
                                      const QString &notebookUid)
{
    if (!notebookUid.isEmpty()) {
        return d->mBackend.deletedIncidences(list, notebookUid, after);
    } else {
        bool success = true;
        for (const Notebook::Ptr &nb : notebooks()) {
            success = d->mBackend.deletedIncidences(list, nb->uid(), after) && success;
        }
        return success;
    }
}

bool SqliteStorage::allIncidences(Incidence::List *list, const QString &notebookUid)
{
    bool success = true;

    if (notebookUid.isEmpty()) {
        for (const Notebook::Ptr &nb : notebooks()) {
            success = d->mBackend.incidences(list, nb->uid()) && success;
        }
    } else {
        success = d->mBackend.incidences(list, notebookUid);
    }

    return success;
}

QDateTime SqliteStorage::incidenceDeletedDate(const Incidence::Ptr &incidence)
{
    Incidence::List list;
    if (!deletedIncidences(&list)) {
        return QDateTime();
    }
    for (Incidence::List::ConstIterator it = list.constBegin();
         it != list.constEnd(); it++) {
        if ((*it)->uid() == incidence->uid()
            && ((!(*it)->hasRecurrenceId() && !incidence->hasRecurrenceId())
                || (*it)->recurrenceId() == incidence->recurrenceId())) {
            return d->mBackend.deletedDate(**it);
        }
    }
    return QDateTime();
}

bool SqliteStorage::loadNotebooks()
{
    Notebook::List list;
    Notebook::Ptr defaultNb;
    if (d->mBackend.notebooks(&list, &defaultNb)) {
        d->mIsLoading = true;
        for (Notebook::List::ConstIterator it = list.constBegin();
             it != list.constEnd(); it++) {
            bool isDefault = defaultNb && defaultNb->uid() == (*it)->uid();
            if (isDefault && !setDefaultNotebook(*it)) {
                qCWarning(lcMkcal) << "cannot add default notebook" << (*it)->uid() << (*it)->name() << "to storage";
            } else if (!isDefault && !addNotebook(*it)) {
                qCWarning(lcMkcal) << "cannot add notebook" << (*it)->uid() << (*it)->name() << "to storage";
            }
        }
        d->mIsLoading = false;
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
    } else {
        qCWarning(lcMkcal) << "cannot load notebooks from calendar";
        close();
        return false;
    }
    return true;
}

bool SqliteStorage::insertNotebook(const Notebook::Ptr &nb)
{
    if (d->mIsLoading) {
        return true;
    }
    if (!nb) {
        return false;
    }
    nb->setCreationDate(QDateTime::currentDateTimeUtc());
    if (d->mBackend.addNotebook(*nb, nb == defaultNotebook())) {
        return true;
    }
    return false;
}

bool SqliteStorage::modifyNotebook(const Notebook::Ptr &nb)
{
    if (d->mIsLoading) {
        return true;
    }
    if (!nb) {
        return false;
    }
    if (d->mBackend.updateNotebook(*nb, nb == defaultNotebook())) {
        return true;
    }
    return false;
}

bool SqliteStorage::eraseNotebook(const Notebook::Ptr &nb)
{
    if (d->mIsLoading) {
        return true;
    }
    if (!nb) {
        return false;
    }
    if (d->mBackend.deleteNotebook(*nb)) {
        return true;
    }
    return false;
}

void SqliteStorage::virtual_hook(int id, void *data)
{
    Q_UNUSED(id);
    Q_UNUSED(data);
    Q_ASSERT(false);
}
