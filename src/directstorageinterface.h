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

#ifndef DIRECT_STORAGE_INTERFACE_H
#define DIRECT_STORAGE_INTERFACE_H

#include <QDateTime>
#include <QString>

#include <KCalendarCore/MemoryCalendar>

namespace mKCal {

class MKCAL_EXPORT DirectStorageInterface
{
 public:
    // Synchronization Specific Methods //

    /**
      Get inserted incidences from storage.

      NOTE: time stamps assigned by KCalExtended are created during save().
      To obtain a time stamp that is guaranteed to not included recent changes,
      sleep for a second or increment the current time by a second.

      @param list inserted incidences
      @param after list only incidences inserted after or at given datetime
      @param notebookUid list only incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool insertedIncidences(KCalendarCore::Incidence::List *list,
                                    const QDateTime &after = QDateTime(),
                                    const QString &notebookUid = QString()) = 0;

    /**
      Get modified incidences from storage.
      NOTE: if an incidence is both created and modified after the
      given time, it will be returned in insertedIncidences only, not here!

      @param list modified incidences
      @param after list only incidences modified after or at given datetime
      @param notebookUid list only incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool modifiedIncidences(KCalendarCore::Incidence::List *list,
                                    const QDateTime &after = QDateTime(),
                                    const QString &notebookUid = QString()) = 0;

    /**
      Get deleted incidences from storage.

      @param list deleted incidences
      @param after list only incidences deleted after or at given datetime
      @param notebookUid list only incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool deletedIncidences(KCalendarCore::Incidence::List *list,
                                   const QDateTime &after = QDateTime(),
                                   const QString &notebookUid = QString()) = 0;

    /**
      Get all incidences from storage.

      @param list notebook's incidences
      @param notebookUid list incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool allIncidences(KCalendarCore::Incidence::List *list,
                               const QString &notebookUid = QString()) = 0;

    /**
      Get possible duplicates for given incidence.

      @param list duplicate incidences
      @param incidence incidence to check
      @param notebookUid list incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool duplicateIncidences(KCalendarCore::Incidence::List *list,
                                     const KCalendarCore::Incidence::Ptr &incidence,
                                     const QString &notebookUid = QString()) = 0;

    /**
      Get deletion time of incidence

      @param incidence incidence to check
      @return valid deletion time of incidence in UTC if incidence has been deleted otherwise QDateTime()
    */
    virtual QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &incidence) = 0;

    /**
      Get count of events

      @return count of events
    */
    virtual int eventCount() = 0;

    /**
      Get count of todos

      @return count of todos
    */
    virtual int todoCount() = 0;

    /**
      Get count of journals

      @return count of journals
    */
    virtual int journalCount() = 0;

    /**
      Remove from storage all incidences that have been previously
      marked as deleted and that matches the UID / RecID of the incidences
      in list. The action is performed immediately on database.

      @return True on success, false otherwise.
     */
    virtual bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list) = 0;

    /**
      Load all contacts in the database. Doesn't put anything into calendar.
      Resulting list of persons is ordered by the number of appearances.
      Use Person::count to get the number of appearances.

      @return ordered list of persons
    */
    virtual KCalendarCore::Person::List loadContacts() = 0;

    // Notebook Methods //

    /**
      Read one single notebook from DB.

      @param uid notebook uid
      @return a notebook
    */
    virtual Notebook loadNotebook(const QString &uid) = 0;

    // Observer Specific Methods //

    class MKCAL_EXPORT Observer
    {
    public:
        virtual void storageNotebookAdded(DirectStorageInterface *storage,
                                          const Notebook &nb) {};
        virtual void storageNotebookModified(DirectStorageInterface *storage,
                                             const Notebook &nb, const Notebook&old) {};
        virtual void storageNotebookDeleted(DirectStorageInterface *storage,
                                            const Notebook &nb) {};
        virtual void storageIncidenceAdded(DirectStorageInterface *storage,
                                           const KCalendarCore::Calendar *calendar,
                                           const KCalendarCore::Incidence::List &added) {};
        virtual void storageIncidenceModified(DirectStorageInterface *storage,
                                              const KCalendarCore::Calendar *calendar,
                                              const KCalendarCore::Incidence::List &modified) {};
        virtual void storageIncidenceDeleted(DirectStorageInterface *storage,
                                             const KCalendarCore::Calendar *calendar,
                                             const KCalendarCore::Incidence::List &deleted) {};
    };

    /**
      Registers an Observer for this Storage.

      @param observer is a pointer to an Observer object that will be
      watching this Storage.

      @see unregisterObserver()
     */
    virtual void registerDirectObserver(DirectStorageInterface::Observer *observer) = 0;

    /**
      Unregisters an Observer for this Storage.

      @param observer is a pointer to an Observer object that has been
      watching this Storage.

      @see registerObserver()
     */
    virtual void unregisterDirectObserver(DirectStorageInterface::Observer *observer) = 0;
};
    
}

#endif
