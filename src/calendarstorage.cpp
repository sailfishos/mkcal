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
/**
  @file
  This file is part of the API for handling calendar data and
  defines the CalendarStorage abstract base class.

  @brief
  An abstract base class that provides a calendar storage interface.

  @author Cornelius Schumacher \<schumacher@kde.org\>
*/
#include "calendarstorage.h"
#include "sqlitecalendarstorage.h"
#include "alarmhandler_p.h"
#include "calendarhandler_p.h"
#include "logging_p.h"

using namespace KCalendarCore;

using namespace mKCal;

//@cond PRIVATE
class mKCal::CalendarStorage::Private: public CalendarHandler, public AlarmHandler
{
public:
    Private(CalendarStorage *storage)
        : CalendarHandler(storage->calendar().staticCast<MemoryCalendar>())
        , mStorage(storage)
    {}

    CalendarStorage *mStorage;
    QList<CalendarStorage::Observer *> mObservers;
    bool openDefaultNotebook = false;

    Incidence::List incidencesWithAlarms(const QString &notebookUid,
                                         const QString &uid) override;
};
//@endcond

CalendarStorage::CalendarStorage(const MemoryCalendar::Ptr &cal)
    : CalStorage(cal ? cal : MemoryCalendar::Ptr(new MemoryCalendar(QTimeZone::systemTimeZone())))
    , d(new CalendarStorage::Private(this))
{
}

CalendarStorage::CalendarStorage(const QString &uid)
    : CalendarStorage(MemoryCalendar::Ptr())
{
    calendar()->setId(uid);
}

CalendarStorage::~CalendarStorage()
{
    delete d;
}

bool CalendarStorage::open()
{
    Notebook::Ptr notebook = loadedNotebook();
    if (!notebook) {
        notebook = Notebook::Ptr(new Notebook(calendar()->id(), calendar()->name(),
                                              QString(), QString(), false, false,
                                              false, calendar()->accessMode() == ReadOnly, true));
    }
    d->setNotebook(notebook);
    return true;
}

bool CalendarStorage::close()
{
    d->setNotebook(Notebook::Ptr());
    return true;
}

void CalendarStorage::Observer::storageModified(CalendarStorage *storage)
{
    Q_UNUSED(storage);
}

void CalendarStorage::Observer::storageUpdated(CalendarStorage *storage,
                                             const KCalendarCore::Incidence::List &added,
                                             const KCalendarCore::Incidence::List &modified,
                                             const KCalendarCore::Incidence::List &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(added);
    Q_UNUSED(modified);
    Q_UNUSED(deleted);
}

void CalendarStorage::registerObserver(CalendarStorage::Observer *observer)
{
    if (!d->mObservers.contains(observer)) {
        d->mObservers.append(observer);
    }
}

void CalendarStorage::unregisterObserver(CalendarStorage::Observer *observer)
{
    d->mObservers.removeAll(observer);
}

void CalendarStorage::emitStorageModified()
{
    Notebook::Ptr dbNotebook = loadedNotebook();
    if (dbNotebook) {
        d->setNotebook(dbNotebook);
    }

    calendar()->close();

    foreach (Observer *observer, d->mObservers) {
        observer->storageModified(this);
    }
}

void CalendarStorage::emitStorageUpdated(const QStringList &added,
                                         const QStringList &modified,
                                         const QStringList &deleted)
{
    const Incidence::List additions = d->insertedIncidences(added);
    const Incidence::List modifications = d->updatedIncidences(modified);
    const Incidence::List deletions = d->deletedIncidences(deleted);
    if (!additions.isEmpty() || !modifications.isEmpty() || !deletions.isEmpty()) {
        foreach (Observer *observer, d->mObservers) {
            observer->storageUpdated(this, additions, modifications, deletions);
        }
    }

    QSet<QPair<QString, QString>> uids;
    for (const Incidence::Ptr &incidence : additions + modifications + deletions) {
        uids.insert(QPair<QString, QString>(calendar()->id(), incidence->uid()));
    }
    d->setupAlarms(uids);
}

Notebook::Ptr CalendarStorage::notebook() const
{
    return d->notebook();
}

void CalendarStorage::emitNotebookAdded()
{
    d->setNotebook(loadedNotebook());
}

void CalendarStorage::emitNotebookUpdated(const Notebook &old)
{
    d->setNotebook(loadedNotebook());

    if (old.isVisible() && !notebook()->isVisible()) {
        d->clearAlarms(calendar()->id());
    } else if (!old.isVisible() && notebook()->isVisible()) {
        d->setupAlarms(calendar()->id());
    }
}

bool CalendarStorage::save()
{
    return save(MarkDeleted);
}

bool CalendarStorage::save(DeleteAction deleteAction)
{
    if (!notebook()) {
        qCWarning(lcMkcal) << "cannot save closed database. Use open() first.";
        return false;
    }

    Incidence::List toAdd, toUpdate, toDelete;
    d->observedIncidences(&toAdd, &toUpdate, &toDelete);

    bool success = save(toAdd, toUpdate, toDelete, deleteAction);

    d->clearObservedIncidences();

    return success;
}

Incidence::List CalendarStorage::Private::incidencesWithAlarms(const QString &notebookUid, const QString &uid)
{
    Incidence::List list;
    if (notebookUid != calendar()->id() || !notebook()->isVisible()) {
        return list;
    }

    // Recurring incidences may not have alarms but their exception may.
    for (const Incidence::Ptr &incidence : mStorage->incidences(uid)) {
        if (incidence->hasEnabledAlarms() || incidence->recurs()) {
            list.append(incidence);
        }
    }
    return list;
}

bool CalendarStorage::addIncidences(const Incidence::List &list)
{
    return d->addIncidences(list);
}

bool CalendarStorage::openDefaultNotebook() const
{
    return d->openDefaultNotebook;
}

CalendarStorage::Ptr CalendarStorage::systemStorage()
{
    MemoryCalendar::Ptr calendar(new MemoryCalendar(QTimeZone::systemTimeZone()));
    return SqliteCalendarStorage::Ptr(new SqliteCalendarStorage(calendar)).staticCast<CalendarStorage>();
}

CalendarStorage::Ptr CalendarStorage::systemDefaultCalendar()
{
    CalendarStorage::Ptr storage = systemStorage();
    storage->d->openDefaultNotebook = true;
    return storage;
}
