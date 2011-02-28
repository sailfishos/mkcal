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
 * exactly nothing. It is only inteded to use for testing pouposes
 */

class MKCAL_EXPORT DummyStorage : public mKCal::ExtendedStorage
{
public:
    DummyStorage(const mKCal::ExtendedCalendar::Ptr &cal) : mKCal::ExtendedStorage(cal)
    {
        mKCal::Notebook::Ptr nb = mKCal::Notebook::Ptr( new mKCal::Notebook("dummy-name",
                                                "dummy-desc") );
        bool r;
        r = addNotebook(nb);
        Q_ASSERT(r);
        r = setDefaultNotebook(nb);
        Q_ASSERT(r);
    }

    void calendarModified(bool, KCalCore::Calendar *)
    {
    }
    void calendarIncidenceAdded(const KCalCore::Incidence::Ptr&)
    {
    }
    void calendarIncidenceChanged(const KCalCore::Incidence::Ptr&)
    {
    }
    void calendarIncidenceDeleted(const KCalCore::Incidence::Ptr&)
    {
    }
    void calendarIncidenceAdditionCanceled(const KCalCore::Incidence::Ptr&)
    {
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

    /**
      @copydoc
      ExtendedStorage::save()
    */
    bool save()
    {
        return true;
    }
    bool close()
    {
        return true;
    }
    bool load(const QString&, const KDateTime&)
    {
        return true;
    }
    bool load(const QDate&)
    {
        return true;
    }
    bool load(const QDate&, const QDate&)
    {
        return true;
    }
    bool loadNotebookIncidences(const QString&)
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
    int loadCompletedTodos(bool, int, KDateTime*)
    {
        return 0;
    }
    int loadIncidences(bool, int, KDateTime*)
    {
        return 0;
    }
    int loadFutureIncidences(int, KDateTime*)
    {
        return 0;
    }
    int loadGeoIncidences(bool, int, KDateTime*)
    {
        return 0;
    }
    int loadUnreadInvitationIncidences()
    {
        return 0;
    }
    int loadOldInvitationIncidences(int, KDateTime*)
    {
        return 0;
    }
    KCalCore::Person::List loadContacts()
    {
        KCalCore::Person::List l;
        return l;
    }
    int loadContactIncidences(const KCalCore::Person::Ptr&, int, KDateTime*)
    {
        return 0;
    }
    int loadJournals(int, KDateTime*)
    {
        return 0;
    }
    bool notifyOpened(const KCalCore::Incidence::Ptr&)
    {
        return true;
    }
    bool cancel()
    {
        return true;
    }
    void calendarModified(bool, const KCalCore::Calendar*) const
    {
    }
    void calendarIncidenceAdded(const KCalCore::Incidence::Ptr&) const
    {
    }
    void calendarIncidenceChanged(const KCalCore::Incidence::Ptr&) const
    {
    }
    void calendarIncidenceDeleted(const KCalCore::Incidence::Ptr&) const
    {
    }
    void calendarIncidenceAdditionCanceled(const KCalCore::Incidence::Ptr&) const
    {
    }
    bool insertedIncidences(KCalCore::Incidence::List *, const KDateTime&, const QString&)
    {
        return true;
    }
    bool modifiedIncidences(KCalCore::Incidence::List *, const KDateTime&, const QString&)
    {
        return true;
    }
    bool deletedIncidences(KCalCore::Incidence::List *, const KDateTime&, const QString&)
    {
        return true;
    }
    bool allIncidences(KCalCore::Incidence::List *, const QString&)
    {
        return true;
    }
    bool duplicateIncidences(KCalCore::Incidence::List *, const KCalCore::Incidence::Ptr&, const QString&)
    {
        return true;
    }
    bool loadNotebooks()
    {
        return true;
    }
    bool reloadNotebooks()
    {
        return true;
    }
    bool modifyNotebook(const mKCal::Notebook::Ptr&, mKCal::DBOperation, bool)
    {
        return true;
    }
    KDateTime incidenceDeletedDate( const KCalCore::Incidence::Ptr &incidence )
    {
      return KDateTime();
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
    void virtual_hook( int, void * ) {
      return;
    }
};

#endif /* DUMMYSTORAGE_H */
