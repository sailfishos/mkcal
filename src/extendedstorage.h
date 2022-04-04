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
/**
  @file
  This file is part of the API for handling calendar data and
  defines the ExtendedStorage interface.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Alvaro Manera \<alvaro.manera@nokia.com \>
*/

#ifndef MKCAL_EXTENDEDSTORAGE_H
#define MKCAL_EXTENDEDSTORAGE_H

#include "mkcal_export.h"
#include "extendedcalendar.h"
#include "sqlitestorage.h"
#include "notebook.h"

#include <KCalendarCore/Calendar>

namespace KCalendarCore {
class Incidence;
}

namespace mKCal {

/**
  @brief
  This class provides a calendar storage interface.
  Every action on the storage can be asynchronous, which means that actions
  are only scheduled for execution. Caller must use ExtendedStorageObserver to get
  notified about the completion.
*/
class MKCAL_EXPORT ExtendedStorage
    : public SqliteStorage
{
    Q_OBJECT

public:

    /**
      A shared pointer to a ExtendedStorage
    */
    typedef QSharedPointer<ExtendedStorage> Ptr;

    /**
      Constructs a new ExtendedStorage object.

      @param cal is a pointer to a valid Calendar object.
      @param dbFile a location for the SQLite database.
      @param validateNotebooks set to true for loading/saving only those
             incidences that belong to an existing notebook of this storage

      @warning Do not use storage as a global object, on closing the application
      it can dead lock. If you do so, be ready to destroy it manually before the
      application closes.

      @warning Once an Incidence has been added to the ExtendedStorage the UID
      cannot change. It is possible to do so through the API, but the internal
      hash tables will not be updated and hence the changes will not be tracked.
    */
    explicit ExtendedStorage(const ExtendedCalendar::Ptr &cal, const QString &dbFile = QString(), bool validateNotebooks = true);

    /**
      Destructor.
    */
    virtual ~ExtendedStorage();

    ExtendedCalendar::Ptr calendar();

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
    virtual bool save(StorageBackend::DeleteAction deleteAction);

    /**
      Mark if supported by the storage that an incidence has been opened.
      This should be called only if the Incidence has been opened by the user
      and displayed all the contents. Being in a list doesn't qualify for it.

      @param incidence The incidence that has been opened
      @return True if sucessful; false otherwise
    */
    virtual bool notifyOpened(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      Cancel any ongoing action (load etc.).

      @return true cancel was successful; false otherwise
    */
    virtual bool cancel();

    /**
      @copydoc
      StorageBackend::addNotebook(const Notebook::Ptr nb)

      @note if the Notebook doesn't have a uid that is a valid UUID a new one will
      be generated on insertion.
    */
    bool addNotebook(const Notebook::Ptr &nb);

    /**
      @copydoc
      StorageBackend::updateNotebook(const Notebook::Ptr nb)
    */
    bool updateNotebook(const Notebook::Ptr &nb);

    /**
      @copydoc
      StorageBackend::deleteNotebook(const Notebook::Ptr nb)
    */
    bool deleteNotebook(const Notebook::Ptr &nb);

    /**
      setDefaultNotebook to the storage.

      @param nb notebook
      @return true if operation was successful; false otherwise.
    */
    bool setDefaultNotebook(const Notebook::Ptr &nb);

    /**
      defaultNotebook.

      @return pointer to default notebook.
    */
    Notebook::Ptr defaultNotebook();

    /**
      Search for notebook.

      @param uid notebook uid
      @return pointer to notebook
    */
    Notebook::Ptr notebook(const QString &uid);

    /**
      List all notebooks.

      @return list of notebooks
    */
    Notebook::List notebooks();

    /**
      Determine if notebooks should be validated in saves and loads.
      That means that storage can only load/save incidences into/from
      existing notebooks.

      @param validate true to validate
    */
    void setValidateNotebooks(bool validateNotebooks);

    /**
      Returns true if notebooks should be validated in saves and loads.
      That means that storage can only load/save incidences into/from
      existing notebooks.

      @return true to validate notebooks
    */
    bool validateNotebooks();

    /**
      Returns true if the given notebook is valid for the storage.
      That means that storage can load/save incidences on this notebook.

      @param notebookUid notebook uid
      @return true or false
    */
    bool isValidNotebook(const QString &notebookUid);

    // Alarm Methods //

    /**
      Checking if an incidence has active alarms.
      Application can use this function for getting the incidence in
      question, for example, displaying the incidence after an alarm.

      @param uid uid
      @param recurrenceId recurrenceId
      @param loadAlways set true to load always from storage
      @return the alarmed incidence, or null if there is no active alarm
    */
    KCalendarCore::Incidence::Ptr checkAlarm(const QString &uid, const QString &recurrenceId,
                                        bool loadAlways = false);

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(ExtendedStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond
};

}

#endif
