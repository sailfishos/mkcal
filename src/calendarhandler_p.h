
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
  This file is a helper API to monitor a KCalendarCore::MemoryCalendar.

  @author Damien Caliste \<dcaliste@free.fr\>
*/

#ifndef MKCAL_CALENDARHANDLER_H
#define MKCAL_CALENDARHANDLER_H

#include "notebook.h"

#include <KCalendarCore/MemoryCalendar>

namespace mKCal {

class CalendarHandler: public KCalendarCore::Calendar::CalendarObserver
{
public:
    CalendarHandler(const QTimeZone &timezone = QTimeZone::systemTimeZone());
    CalendarHandler(KCalendarCore::MemoryCalendar::Ptr calendar);
    ~CalendarHandler();

    KCalendarCore::MemoryCalendar::Ptr calendar() const;

    Notebook::Ptr notebook() const;
    void setNotebook(const Notebook::Ptr &notebook);

    /**
      Add the given list of incidences to the calendar, without
      registering them as added or updated incidences.

      @param list, a list of incidences.
      @returns true on success.
     */
    bool addIncidences(const KCalendarCore::Incidence::List &list);

    /**
      Make the association between instanceIdentifiers and incidences
      from the added list.

      @param ids, a list of instanceIdentifiers.
      @returns the corresponding incidences.
     */
    KCalendarCore::Incidence::List insertedIncidences(const QStringList &ids) const;

    /**
      Make the association between instanceIdentifiers and incidences
      from the updated list.

      @param ids, a list of instanceIdentifiers.
      @returns the corresponding incidences.
     */
    KCalendarCore::Incidence::List updatedIncidences(const QStringList &ids) const;

    /**
      Make the association between instanceIdentifiers and incidences
      from the deleted list.

      @param ids, a list of instanceIdentifiers.
      @returns the corresponding incidences.
     */
    KCalendarCore::Incidence::List deletedIncidences(const QStringList &ids) const;

    /**
      Clear the list of added, updated or deleted incidences.
     */
    void clearObservedIncidences();

    /**
      Provide the list of added, updated or deleted incidences in the calendar.

      @param toAdd, a list of incidences that have been added to the calendar.
      @param toUpdate, a list of incidences that have been modified in the calendar.
      @param toDelete, a list of incidences that have been deleted from the calendar.
     */
    void observedIncidences(KCalendarCore::Incidence::List *toAdd,
                            KCalendarCore::Incidence::List *toUpdate,
                            KCalendarCore::Incidence::List *toDelete) const;

private:
    QHash<QString, KCalendarCore::Incidence::Ptr> mIncidencesToInsert;
    QHash<QString, KCalendarCore::Incidence::Ptr> mIncidencesToUpdate;
    QHash<QString, KCalendarCore::Incidence::Ptr> mIncidencesToDelete;

    void calendarModified(bool modified, KCalendarCore::Calendar *calendar) override;
    void calendarIncidenceAdded(const KCalendarCore::Incidence::Ptr &incidence) override;
    void calendarIncidenceChanged(const KCalendarCore::Incidence::Ptr &incidence) override;
    void calendarIncidenceDeleted(const KCalendarCore::Incidence::Ptr &incidence,
                                  const KCalendarCore::Calendar *calendar) override;
    void calendarIncidenceAdditionCanceled(const KCalendarCore::Incidence::Ptr &incidence) override;
        
    KCalendarCore::MemoryCalendar::Ptr mCalendar;
    Notebook::Ptr mNotebook;
};

}

#endif
