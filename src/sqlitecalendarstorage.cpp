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

#include "sqlitecalendarstorage.h"
#include "singlesqlitebackend_p.h"
#include "logging_p.h"

using namespace KCalendarCore;

using namespace mKCal;

//@cond PRIVATE
class SqliteCalendarStorage::Private
{
public:
    Private(const QString &databaseName)
        : mBackend(databaseName)
    {
    }

    SingleSqliteBackend mBackend;
    Notebook::Ptr mDbNotebook;
    bool mIsDefault = false;

    bool loadNotebook(const QString &notebookUid);
    bool loadDefaultNotebook();
};
//@endcond

SqliteCalendarStorage::SqliteCalendarStorage(const MemoryCalendar::Ptr &cal,
                                             const QString &databaseName)
    : CalendarStorage(cal)
    , d(new Private(databaseName))
{
}

SqliteCalendarStorage::SqliteCalendarStorage(const QString &uid,
                                             const QString &databaseName)
    : CalendarStorage(uid)
    , d(new Private(databaseName))
{
}

SqliteCalendarStorage::~SqliteCalendarStorage()
{
    delete d;
}

bool SqliteCalendarStorage::open()
{
    if (!d->mBackend.open()
        || (openDefaultNotebook() && !d->loadDefaultNotebook())
        || (!openDefaultNotebook() && !d->loadNotebook(calendar()->id()))) {
        return false;
    }
    connect(&d->mBackend, &SingleSqliteBackend::modified,
            this, &SqliteCalendarStorage::onModified);
    connect(&d->mBackend, &SingleSqliteBackend::updated,
            this, &SqliteCalendarStorage::onUpdated);
    return CalendarStorage::open();
}

bool SqliteCalendarStorage::close()
{
    if (!d->mBackend.close()) {
        return false;
    }
    d->mDbNotebook.clear();
    d->mIsDefault = false;
    disconnect(&d->mBackend, &SingleSqliteBackend::modified,
               this, &SqliteCalendarStorage::onModified);
    disconnect(&d->mBackend, &SingleSqliteBackend::updated,
               this, &SqliteCalendarStorage::onUpdated);
    return CalendarStorage::close();
}

bool SqliteCalendarStorage::Private::loadDefaultNotebook()
{
    mDbNotebook.clear();
    mIsDefault = true;

    Notebook::List list;
    if (mBackend.notebooks(&list, &mDbNotebook)) {
        return true;
    } else {
        qCWarning(lcMkcal) << "cannot load notebooks.";
        return false;
    }
}

bool SqliteCalendarStorage::Private::loadNotebook(const QString &notebookUid)
{
    mDbNotebook.clear();
    mIsDefault = false;
    if (notebookUid.isEmpty()) {
        return true;
    }

    Notebook::List list;
    Notebook::Ptr defaultNb;
    if (mBackend.notebooks(&list, &defaultNb)) {
        for (Notebook::List::ConstIterator it = list.constBegin();
             it != list.constEnd(); it++) {
            if ((*it)->uid() == notebookUid) {
                mDbNotebook = *it;
                break;
            }
        }
        mIsDefault = mDbNotebook && defaultNb
            && mDbNotebook->uid() == defaultNb->uid();
        return true;
    } else {
        qCWarning(lcMkcal) << "cannot load notebooks.";
        return false;
    }
}

void SqliteCalendarStorage::onModified()
{
    d->loadNotebook(calendar()->id());
    emitStorageModified();
}

void SqliteCalendarStorage::onUpdated(const QHash<QString, QStringList> &added,
                                      const QHash<QString, QStringList> &modified,
                                      const QHash<QString, QStringList> &deleted)
{
    emitStorageUpdated(added.value(calendar()->id()),
                       modified.value(calendar()->id()),
                       deleted.value(calendar()->id()));
}

Notebook::Ptr SqliteCalendarStorage::loadedNotebook() const
{
    return d->mDbNotebook ? Notebook::Ptr(new Notebook(*d->mDbNotebook)) : Notebook::Ptr();
}

bool SqliteCalendarStorage::load()
{
    Incidence::List list;
    return d->mBackend.incidences(&list, calendar()->id())
        && addIncidences(list);
}

bool SqliteCalendarStorage::load(const QString &uid)
{
    if (uid.isEmpty()) {
        return load();
    }

    // Don't reload an existing incidence from DB.
    // Either the calendar is already in sync with
    // the calendar or the database has been externally
    // modified and in that case, the calendar has been emptied.
    if (calendar()->incidence(uid)) {
        return true;
    }

    Incidence::List list;
    return d->mBackend.incidences(&list, calendar()->id(), uid)
        && addIncidences(list);
}

bool SqliteCalendarStorage::deletedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after)
{
    return d->mBackend.deletedIncidences(list, calendar()->id(), after);
}

bool SqliteCalendarStorage::insertedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after)
{
    return d->mBackend.insertedIncidences(list, calendar()->id(), after);
}

bool SqliteCalendarStorage::modifiedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after)
{
    return d->mBackend.modifiedIncidences(list, calendar()->id(), after);
}

Incidence::List SqliteCalendarStorage::incidences(const QString &uid)
{
    Incidence::List list;
    if (uid.isEmpty() || !calendar()->incidence(uid)) {
        d->mBackend.incidences(&list, calendar()->id(), uid);
    } else {
        list.append(calendar()->incidence(uid));
        list += calendar()->instances(list[0]);
    }
    return list;
}

bool SqliteCalendarStorage::save(const Incidence::List &added,
                                 const Incidence::List &modified,
                                 const Incidence::List &deleted,
                                 DeleteAction deleteAction)
{
    if (!notebook()) {
        return false;
    }

    if (notebook()->isRunTimeOnly()) {
        return true;
    }

    // Ensure notebook exists in DB and is up-to-date.
    if (!d->mDbNotebook) {
        if (d->mBackend.addNotebook(*notebook(), d->mIsDefault)) {
            d->mDbNotebook = Notebook::Ptr(new Notebook(*notebook()));
            emitNotebookAdded();
        } else {
            qCWarning(lcMkcal) << "cannot add notebook" << calendar()->id();
            return false;
        }
    } else if (!(*d->mDbNotebook == *notebook())) {
        if (d->mBackend.updateNotebook(*notebook(), d->mIsDefault)) {
            Notebook::Ptr old = d->mDbNotebook;
            d->mDbNotebook = Notebook::Ptr(new Notebook(*notebook()));
            emitNotebookUpdated(*old);
        } else {
            qCWarning(lcMkcal) << "cannot update notebook" << calendar()->id();
            return false;
        }
    }

    // Now save incidence changes.
    if (!d->mBackend.deferSaving()) {
        return false;
    }

    bool success = true;
    for (const Incidence::Ptr &incidence : added) {
        success = d->mBackend.addIncidence(calendar()->id(), *incidence) && success;
    }
    for (const Incidence::Ptr &incidence : modified) {
        success = d->mBackend.modifyIncidence(calendar()->id(), *incidence) && success;
    }
    if (deleteAction == MarkDeleted) {
        for (const Incidence::Ptr &incidence : deleted) {
            success = d->mBackend.deleteIncidence(calendar()->id(), *incidence) && success;
        }
    } else if (deleteAction == PurgeDeleted) {
        for (const Incidence::Ptr &incidence : deleted) {
            success = d->mBackend.purgeIncidence(calendar()->id(), *incidence) && success;
        }
    }

    success = d->mBackend.commit() && success;

    return success;
}

bool SqliteCalendarStorage::purgeDeletedIncidences(const KCalendarCore::Incidence::List &list)
{
    return d->mBackend.purgeDeletedIncidences(calendar()->id(), list);
}
