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
        mKCal::Notebook::Ptr nb = mKCal::Notebook::Ptr(new mKCal::Notebook("dummy-name",
                                                                           "dummy-desc"));
        bool r;
        r = addNotebook(nb);
        Q_ASSERT(r);
        r = setDefaultNotebook(nb);
        Q_ASSERT(r);
    }

    void calendarModified(bool, KCalendarCore::Calendar *)
    {
    }
    void calendarIncidenceAdded(const KCalendarCore::Incidence::Ptr &)
    {
    }
    void calendarIncidenceChanged(const KCalendarCore::Incidence::Ptr &)
    {
    }
    void calendarIncidenceDeleted(const KCalendarCore::Incidence::Ptr &, const KCalendarCore::Calendar *)
    {
    }
    void calendarIncidenceAdditionCanceled(const KCalendarCore::Incidence::Ptr &)
    {
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

    /**
      @copydoc
      ExtendedStorage::save()
    */
    bool save()
    {
        return true;
    }
    bool save(DeleteAction)
    {
        return true;
    }
    bool close()
    {
        return true;
    }
    bool load(const QString &)
    {
        return true;
    }
    bool load(const QDate &, const QDate &)
    {
        return true;
    }
    bool loadNotebookIncidences(const QString &)
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
    bool loadNotebooks()
    {
        return true;
    }
    bool insertNotebook(const Notebook::Ptr &)
    {
        return true;
    }
    bool modifyNotebook(const Notebook::Ptr &)
    {
        return true;
    }
    bool eraseNotebook(const Notebook::Ptr &)
    {
        return true;
    }
    QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &)
    {
        return QDateTime();
    }
    void virtual_hook(int, void *)
    {
        return;
    }
};

#endif /* DUMMYSTORAGE_H */
