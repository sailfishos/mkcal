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

#ifndef TIMED_H
#define TIMED_H

#include "mkcal_export.h"
#include "extendedstorageobserver.h"
#include "directstorageinterface.h"

#include <KCalendarCore/MemoryCalendar>

#ifdef TIMED_SUPPORT
# include <timed-qt5/interface.h>
#endif

namespace mKCal {

class MKCAL_EXPORT TimedPlugin: public DirectStorageInterface::Observer
{
 public:
    TimedPlugin() {};

    void storageIncidenceAdded(DirectStorageInterface *storage,
                               const KCalendarCore::Calendar *calendar,
                               const KCalendarCore::Incidence::List &added) override;
    void storageIncidenceModified(DirectStorageInterface *storage,
                                  const KCalendarCore::Calendar *calendar,
                                  const KCalendarCore::Incidence::List &modified) override;
    void storageIncidenceDeleted(DirectStorageInterface *storage,
                                 const KCalendarCore::Calendar *calendar,
                                 const KCalendarCore::Incidence::List &deleted) override;
    void storageNotebookModified(DirectStorageInterface *storage,
                                 const Notebook &nb, const Notebook &old) override;
    void storageNotebookDeleted(DirectStorageInterface *storage,
                                const Notebook &nb) override;
 private:
#if defined(TIMED_SUPPORT)
    // These alarm methods are used to communicate with an external
    // daemon, like timed, to bind Incidence::Alarm with the system notification.
    void clearAlarms(const KCalendarCore::Incidence::Ptr &incidence);
    void clearAlarms(const KCalendarCore::Incidence::List &incidences);
    void clearAlarms(const QString &notebookUid);
    QDateTime getNextOccurrence(const KCalendarCore::Incidence::Ptr &incidence,
                                const QDateTime &start,
                                const KCalendarCore::Incidence::List &exceptions);
    void setAlarms(const KCalendarCore::Calendar &calendar,
                   const KCalendarCore::Incidence::List &incidences);

    void addAlarms(const KCalendarCore::Incidence::Ptr &incidence,
                   const QString &nbuid, Maemo::Timed::Event::List *events,
                   const QDateTime &now);
    void commitEvents(Maemo::Timed::Event::List &events);
#endif
};

}

#endif
