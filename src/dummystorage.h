/*
  This file is part of the mkcal library.

  Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
  Contact: Alvaro Manera <alvaro.manera@nokia.com>

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
#ifndef DUMMYSTORAGE_H
#define DUMMYSTORAGE_H

#include "extendedstorage.h"
#include "extendedcalendar.h"
#include "notebook.h"


/**
 * This module provides a simple storage abstraction which contains
 * exactly nothing. It is only inteded to use for testing purposes
 */

class MKCAL_EXPORT DummyStorage : public mKCal::ExtendedStorage
{
public:
    DummyStorage(const mKCal::ExtendedCalendar::Ptr &cal) : mKCal::ExtendedStorage(cal)
    {
        mKCal::Notebook nb("dummy-name", "dummy-desc");
        bool r;
        r = addNotebook(nb);
        Q_ASSERT(r);
        r = setDefaultNotebook(nb);
        Q_ASSERT(r);
    }

    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &)
    {
        return true;
    }

    /**
      @copydoc
      ExtendedStorage::open()
    */
    bool open()
    {
        return true;
    }

    /**
      @copydoc
      ExtendedStorage::load()
    */
    bool load()
    {
        return true;
    }

    bool load(const QString &, const QDateTime &)
    {
        return true;
    }
    bool load(const QDate &)
    {
        return true;
    }
    bool load(const QDate &, const QDate &)
    {
        return true;
    }
    bool loadSeries(const QString &)
    {
        return true;
    }
    bool loadIncidenceInstance(const QString &)
    {
        return true;
    }
    bool loadNotebookIncidences(const QString &)
    {
        return true;
    }
    bool loadJournals()
    {
        return true;
    }
    bool loadPlainIncidences()
    {
        return true;
    }
    bool loadRecurringIncidences()
    {
        return true;
    }
    bool loadGeoIncidences()
    {
        return true;
    }
    bool loadGeoIncidences(float, float, float, float)
    {
        return true;
    }
    bool loadAttendeeIncidences()
    {
        return true;
    }
    int loadUncompletedTodos()
    {
        return 0;
    }
    int loadCompletedTodos(bool, int, QDateTime *)
    {
        return 0;
    }
    int loadIncidences(bool, int, QDateTime *)
    {
        return 0;
    }
    int loadFutureIncidences(int, QDateTime *)
    {
        return 0;
    }
    int loadGeoIncidences(bool, int, QDateTime *)
    {
        return 0;
    }
    int loadUnreadInvitationIncidences()
    {
        return 0;
    }
    int loadOldInvitationIncidences(int, QDateTime *)
    {
        return 0;
    }
    KCalendarCore::Person::List loadContacts()
    {
        KCalendarCore::Person::List l;
        return l;
    }
    int loadContactIncidences(const KCalendarCore::Person &, int, QDateTime *)
    {
        return 0;
    }
    int loadJournals(int, QDateTime *)
    {
        return 0;
    }
    bool notifyOpened(const KCalendarCore::Incidence::Ptr &)
    {
        return true;
    }
    bool cancel()
    {
        return true;
    }
    void calendarModified(bool, const KCalendarCore::Calendar *) const
    {
    }
    void calendarIncidenceAdded(const KCalendarCore::Incidence::Ptr &) const
    {
    }
    void calendarIncidenceChanged(const KCalendarCore::Incidence::Ptr &) const
    {
    }
    void calendarIncidenceDeleted(const KCalendarCore::Incidence::Ptr &) const
    {
    }
    void calendarIncidenceAdditionCanceled(const KCalendarCore::Incidence::Ptr &) const
    {
    }
    bool insertedIncidences(KCalendarCore::Incidence::List *, const QDateTime &, const QString &)
    {
        return true;
    }
    bool modifiedIncidences(KCalendarCore::Incidence::List *, const QDateTime &, const QString &)
    {
        return true;
    }
    bool deletedIncidences(KCalendarCore::Incidence::List *, const QDateTime &, const QString &)
    {
        return true;
    }
    bool allIncidences(KCalendarCore::Incidence::List *, const QString &)
    {
        return true;
    }
    bool duplicateIncidences(KCalendarCore::Incidence::List *, const KCalendarCore::Incidence::Ptr &, const QString &)
    {
        return true;
    }
    bool loadNotebooks(QList<Notebook> *notebooks, QString *defaultNotebookId)
    {
        return true;
    }
    bool modifyNotebook(const mKCal::Notebook &, mKCal::DBOperation)
    {
        return true;
    }
    bool storeIncidences(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &additions,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &modification,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &deletions,
                         ExtendedStorage::DeleteAction deleteAction)
    {
        return true;
    }
    QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &)
    {
        return QDateTime();
    }
    int eventCount()
    {
        return 0;
    }
    int todoCount()
    {
        return 0;
    }
    int journalCount()
    {
        return 0;
    }
    QTimeZone timeZone() const
    {
        return QTimeZone();
    }
    void virtual_hook(int, void *)
    {
        return;
    }
};

#endif /* DUMMYSTORAGE_H */
