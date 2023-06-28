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
  This file is providing an interface associating a storage with
  a KCalendarCore::MemoryCalendar.

  @author Damien Caliste \<dcaliste@free.fr\>
*/

#ifndef MKCAL_CALENDARSTORAGE_H
#define MKCAL_CALENDARSTORAGE_H

#include "notebook.h"

#include <KCalendarCore/CalStorage>
#include <KCalendarCore/MemoryCalendar>

namespace KCalendarCore {
class Incidence;
}

namespace mKCal {

/**
  @brief
  This class provides a calendar storage interface.
  Every action on the storage can be synchronous or asynchronous,
  depending on the storage implementation. SqliteCalendarStorage is a
  synchronous implementation. 

  In any case, caller can use CalendarStorage::Observer to get
  notified about the action done.
*/
class Q_DECL_EXPORT CalendarStorage
    : public KCalendarCore::CalStorage
{
    Q_OBJECT

public:

    /**
      Action to be performed on save for deleted incidences.
    */
    enum DeleteAction {
        MarkDeleted,
        PurgeDeleted
    };

    typedef QSharedPointer<CalendarStorage> Ptr;

    /**
      Constructs a new CalendarStorage object. If a notebook with the
      cal->id() already exists in the backend, it will be associated
      on opening otherwise one will be created on the fly when saving.

      @param cal is a pointer to a valid Calendar object.

      @warning Once an Incidence has been added to the CalendarStorage the UID
      cannot change. It is possible to do so through the API, but the internal
      hash tables will not be updated and hence the changes will not be tracked.
    */
    explicit CalendarStorage(const KCalendarCore::MemoryCalendar::Ptr &cal);

    /**
      Constructs a new CalendarStorage object. Creating a new
      KCalendarCore::MemoryCalendar to represent it into memory.

      @param uid is a identifier for the calendar. If a notebook with this
             identifier already exists in the backend, it will be associated
             on opening otherwise one will be created on the fly when saving.

      @warning Once an Incidence has been added to the CalendarStorage the UID
      cannot change. It is possible to do so through the API, but the internal
      hash tables will not be updated and hence the changes will not be tracked.
    */
    explicit CalendarStorage(const QString &uid);

    virtual ~CalendarStorage();

    /**
      Associated notebook to the storage.

      @return pointer to notebook
    */
    Notebook::Ptr notebook() const;

    /**
      @copydoc
      CalStorage::open()
    */
    virtual bool open();

    /**
      @copydoc
      CalStorage::load()
    */
    virtual bool load() = 0;

    /**
      Load all incidences sharing the same uid into the memory.

      @param uid is uid of the series
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QString &uid) = 0;

    /**
      @copydoc
      CalStorage::save()
    */
    virtual bool save();

    /**
      This is an overload of save() method. When @deleteAction is
      PurgeDeleted, the deleted incidences are not marked as deleted but completely
      removed from the database and won't appear anymore when calling
      deletedIncidences().

      @param deleteAction the action to apply to deleted incidences
      @return True if successful; false otherwise
    */
    virtual bool save(DeleteAction deleteAction);

    /**
      @copydoc
      CalStorage::close()
    */
    virtual bool close();

    /**
      Get deleted incidences from storage.

      @param list deleted incidences
      @param after restricts returned list to incidences deleted strictly after.
      @return True on success, false otherwise.
    */
    virtual bool deletedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after = QDateTime()) = 0;

    /**
      Remove from storage all incidences that have been previously
      marked as deleted and that matches the UID / RecID of the incidences
      in list. The action is performed immediately on database.

      @param list is the incidences to remove from the DB
      @return True on success, false otherwise.
     */
    virtual bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list) = 0;

    /**
      Get created incidences after a given date.

      Warning: since the Incidence::created() is a user value, the returned list
      may miss incidences or contain false positive values.

      @param list stores the list of returned incidences
      @param after restricts returned list to incidences created strictly after.
      @return True on success, false otherwise.
    */
    virtual bool insertedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after) = 0;

    /**
      Get modified incidences after a given date.

      Warning: since the Incidence::lastModified() is a user value, the returned list
      may miss incidences or contain false positive values.

      @param list stores the list of returned incidences
      @param after restricts returned list to incidences modified strictly after.
      @return True on success, false otherwise.
    */
    virtual bool modifiedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after) = 0;

    // Observer Specific Methods //

    /**
       @class CalendarStorage::Observer

       The CalendarStorage::Observer class.
    */
    class Q_DECL_EXPORT Observer //krazy:exclude=dpointer
        {
        public:
        virtual ~Observer() {};

        /**
           Notify the Observer that a Storage has been modified by an external
           process. There is no information about what has been changed.

           See also storageUpdated() for a notification of modifications done
           in-process.

           @param storage is a pointer to the CalendarStorage object that
           is being observed.
        */
        virtual void storageModified(CalendarStorage *storage);

        /**
           Notify the Observer that a Storage has been updated to reflect the
           content of the associated calendar. This notification is delivered
           because of local changes done in-process by a call to
           CalendarStorage::save() for instance.

           See also storageModified() for a notification for modifications
           done to the database by an external process.

           @param storage is a pointer to the CalendarStorage object that
           is being observed.
           @param added is a list of newly added incidences in the storage
           @param modified is a list of updated incidences in the storage
           @param deleted is a list of deleted incidences from the storage
        */
        virtual void storageUpdated(CalendarStorage *storage,
                                    const KCalendarCore::Incidence::List &added,
                                    const KCalendarCore::Incidence::List &modified,
                                    const KCalendarCore::Incidence::List &deleted);
        };

    /**
      Registers an Observer for this Storage.

      @param observer is a pointer to an Observer object that will be
      watching this Storage.

      @see unregisterObserver()
     */
    void registerObserver(CalendarStorage::Observer *observer);

    /**
      Unregisters an Observer for this Storage.

      @param observer is a pointer to an Observer object that has been
      watching this Storage.

      @see registerObserver()
     */
    void unregisterObserver(CalendarStorage::Observer *observer);

    /**
      Constructs a new CalendarStorage object using the default
      implementation. A new KCalendarCore::MemoryCalendar is created
      to represent it into memory.
    */
    static Ptr systemStorage();

    /**
      Like systemStorage(), but open the default calendar, creating it if necessary.
    */
    static Ptr systemDefaultCalendar();

protected:
    bool openDefaultNotebook() const;
    virtual Notebook::Ptr loadedNotebook() const = 0;
    virtual bool save(const KCalendarCore::Incidence::List &added,
                      const KCalendarCore::Incidence::List &modified,
                      const KCalendarCore::Incidence::List &deleted,
                      DeleteAction deleteAction) = 0;

    virtual KCalendarCore::Incidence::List incidences(const QString &uid = QString()) = 0;
    bool addIncidences(const KCalendarCore::Incidence::List &list);

    void emitStorageModified();
    void emitStorageUpdated(const QStringList &added,
                            const QStringList &modified,
                            const QStringList &deleted);
    void emitNotebookAdded();
    void emitNotebookUpdated(const Notebook &old);

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(CalendarStorage)
    class Private;
    Private *const d;
    //@endcond
};

}

#endif
