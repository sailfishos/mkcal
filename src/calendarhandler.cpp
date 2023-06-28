
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

#include "calendarhandler_p.h"
#include "logging_p.h"

using namespace KCalendarCore;

using namespace mKCal;

CalendarHandler::CalendarHandler(const QTimeZone &timezone)
    : CalendarHandler(MemoryCalendar::Ptr(new MemoryCalendar(timezone)))
{
}

CalendarHandler::CalendarHandler(MemoryCalendar::Ptr calendar)
    : mCalendar(calendar)
{
    mCalendar->registerObserver(this);
}

CalendarHandler::~CalendarHandler()
{
    mCalendar->unregisterObserver(this);
}

MemoryCalendar::Ptr CalendarHandler::calendar() const
{
    return mCalendar;
}

Notebook::Ptr CalendarHandler::notebook() const
{
    return mNotebook;
}

void CalendarHandler::setNotebook(const Notebook::Ptr &notebook)
{
    if (notebook) {
        mCalendar->setId(notebook->uid());
        mCalendar->setName(notebook->name());
        mCalendar->setAccessMode(notebook->isReadOnly() ? ReadOnly : ReadWrite);
    }
    mNotebook = notebook;
}

bool CalendarHandler::addIncidences(const Incidence::List &list)
{
    bool success = true;

    mCalendar->unregisterObserver(this);
    for (const Incidence::Ptr &incidence : list) {
        const QString key = incidence->instanceIdentifier();
        if (mIncidencesToInsert.contains(key) ||
            mIncidencesToUpdate.contains(key) ||
            mIncidencesToDelete.contains(key)) {
            qCWarning(lcMkcal) << "not loading" << incidence->uid() << mCalendar->id()
                               << "(local changes)";
        } else {
            bool added = true;
            Incidence::Ptr old(mCalendar->incidence(incidence->uid(),
                                                    incidence->recurrenceId()));
            if (old) {
                if (incidence->revision() > old->revision()) {
                    mCalendar->deleteIncidence(old);   // move old to deleted
                    // and replace it with the new one.
                } else {
                    added = false;
                }
            }
            if (added && !mCalendar->addIncidence(incidence)) {
                qCWarning(lcMkcal) << "cannot add incidence" << incidence->uid() << "to notebook" << mCalendar->id();
                success = false;
            }
        }
    }
    mCalendar->registerObserver(this);

    return success;
}

Incidence::List CalendarHandler::insertedIncidences(const QStringList &ids) const
{
    Incidence::List list;
    for (const QString &id : ids) {
        const Incidence::Ptr inc = mIncidencesToInsert.value(id);
        if (inc) {
            list.append(inc);
        }
    }
    return list;
}

Incidence::List CalendarHandler::updatedIncidences(const QStringList &ids) const
{
    Incidence::List list;
    for (const QString &id : ids) {
        const Incidence::Ptr inc = mIncidencesToUpdate.value(id);
        if (inc) {
            list.append(inc);
        }
    }
    return list;
}

Incidence::List CalendarHandler::deletedIncidences(const QStringList &ids) const
{
    Incidence::List list;
    for (const QString &id : ids) {
        const Incidence::Ptr inc = mIncidencesToDelete.value(id);
        if (inc) {
            list.append(inc);
        }
    }
    return list;
}

void CalendarHandler::clearObservedIncidences()
{
    mIncidencesToInsert.clear();
    mIncidencesToUpdate.clear();
    mIncidencesToDelete.clear();
}

void CalendarHandler::observedIncidences(Incidence::List *toAdd,
                                     Incidence::List *toUpdate,
                                     Incidence::List *toDelete) const
{
    if (mNotebook->isRunTimeOnly()) {
        return;
    }
    for (const Incidence::Ptr &incidence : mIncidencesToInsert.values()) {
        toAdd->append(incidence);
    }
    for (const Incidence::Ptr &incidence : mIncidencesToUpdate.values()) {
        toUpdate->append(incidence);
    }
    for (const Incidence::Ptr &incidence : mIncidencesToDelete.values()) {
        toDelete->append(incidence);
    }
}

void CalendarHandler::calendarModified(bool modified, Calendar *calendar)
{
    Q_UNUSED(calendar);
    qCDebug(lcMkcal) << "calendarModified called:" << modified;
}

void CalendarHandler::calendarIncidenceAdded(const Incidence::Ptr &incidence)
{
    const QString key = incidence->instanceIdentifier();
    if (mIncidencesToDelete.remove(key) > 0) {
        qCDebug(lcMkcal) << "removing incidence from deleted" << key;
        calendarIncidenceChanged(incidence);
    } else if (!mIncidencesToInsert.contains(key)) {
        qCDebug(lcMkcal) << "appending incidence" << key << "for database insert";
        mIncidencesToInsert.insert(key, incidence);
    }
}

void CalendarHandler::calendarIncidenceChanged(const Incidence::Ptr &incidence)
{
    const QString key = incidence->instanceIdentifier();
    if (!mIncidencesToUpdate.contains(key) &&
        !mIncidencesToInsert.contains(key)) {
        qCDebug(lcMkcal) << "appending incidence" << key << "for database update";
        mIncidencesToUpdate.insert(key, incidence);
    }
}

void CalendarHandler::calendarIncidenceDeleted(const Incidence::Ptr &incidence, const KCalendarCore::Calendar *calendar)
{
    Q_UNUSED(calendar);

    const QString key = incidence->instanceIdentifier();
    if (mIncidencesToInsert.contains(key)) {
        qCDebug(lcMkcal) << "removing incidence from inserted" << key;
        mIncidencesToInsert.remove(key);
    } else if (!mIncidencesToDelete.contains(key)) {
        qCDebug(lcMkcal) << "appending incidence" << key << "for database delete";
        mIncidencesToDelete.insert(key, incidence);
    }
}

void CalendarHandler::calendarIncidenceAdditionCanceled(const Incidence::Ptr &incidence)
{
    const QString key = incidence->instanceIdentifier();
    if (mIncidencesToInsert.contains(key)) {
        qCDebug(lcMkcal) << "duplicate - removing incidence from inserted" << key;
        mIncidencesToInsert.remove(key);
    }
}
