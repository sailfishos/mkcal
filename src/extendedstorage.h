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
#include "notebook.h"

#include <calstorage.h>
#include <calendar.h>

#include <kdatetime.h>

namespace KCalCore {
class Incidence;
}

class MkcalTool;

namespace mKCal {

/**
  Database operation type.
*/
enum DBOperation {
    DBNone,
    DBInsert,
    DBUpdate,
    DBMarkDeleted,
    DBDelete,
    DBSelect
};

// Default alarm receiver is the organiser application.
const char *const DBusService = "com.nokia.organiser";
const char *const DBusInterface = "com.nokia.OrganiserAlarmIf";
const char *const DBusPath = "/";
const char *const DBusName = "alarm";

/**
  @brief
  This class provides a calendar storage interface.
  Every action on the storage can be asynchronous, which means that actions
  are only scheduled for execution. Caller must use ExtendedStorageObserver to get
  notified about the completion.
*/
class MKCAL_EXPORT ExtendedStorage
    : public KCalCore::CalStorage, public KCalCore::Calendar::CalendarObserver
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

    /**
      A shared pointer to a ExtendedStorage
    */
    typedef QSharedPointer<ExtendedStorage> Ptr;

    /**
      Constructs a new ExtendedStorage object.

      @param cal is a pointer to a valid Calendar object.
      @param validateNotebooks set to true for loading/saving only those
             incidences that belong to an existing notebook of this storage

      @warning Do not usea storage as a global object, on closing the application
      it can dead lock. If you do so, be ready to destroy it manually before the
      application closes.

      @warning Once an Incidence has been added to the ExtendedStorage the UID
      cannot change. It is possible to do so through the API, but the internal
      hash tables will not be updated and hence the changes will not be tracked.
    */
    explicit ExtendedStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks = false);

    /**
      Destructor.
    */
    virtual ~ExtendedStorage();

    /**
      @copydoc
      CalStorage::open()
    */
    virtual bool open() = 0;

    /**
      @copydoc
      CalStorage::load()
    */
    virtual bool load() = 0;

    /**
      Load incidence by uid into the memory.

      @param uid is uid of incidence
      @param recurrenceid is recurrenceid of incidence, default null
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QString &uid, const KDateTime &recurrenceId = KDateTime()) = 0;

    /**
      Load incidences at given date into the memory.

      @param date date
      @return true if the load was successful and specific date wasn't already loaded; false otherwise.
    */
    virtual bool load(const QDate &date) = 0;

    /**
      Load incidences between given dates into the memory.

      @param start is the starting date
      @param end is the ending date
      @return true if the load was successful and specific dates wasn't already loaded; false otherwise.
    */
    virtual bool load(const QDate &start, const QDate &end) = 0;

    /**
      Load incidences of one notebook into the memory.

      @param notebookUid is uid of notebook
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadNotebookIncidences(const QString &notebookUid) = 0;

    /**
      Load journal type entries
    */
    virtual bool loadJournals() = 0;

    /**
      Load plain incidences (no startdate and no enddate).

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadPlainIncidences() = 0;

    /**
      Load recurring incidences.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadRecurringIncidences() = 0;

    /**
      Load incidences that have geo parameters.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadGeoIncidences() = 0;

    /**
      Load incidences that have geo parameters inside given rectangle.

      @param geoLatitude latitude
      @param geoLongitude longitude
      @param diffLatitude maximum latitudinal difference
      @param diffLongitude maximum longitudinal difference
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadGeoIncidences(float geoLatitude, float geoLongitude,
                                   float diffLatitude, float diffLongitude) = 0;

    /**
      Load incidences that have attendee.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadAttendeeIncidences() = 0;

    // Smart Loading Functions //

    /**
      Load all uncompleted todos.

      @return number of loaded todos, or -1 on error
    */
    virtual int loadUncompletedTodos() = 0;

    /**
      Load completed todos based on parameters. Load direction is descending,
      i.e., starting from most distant upcoming todo.

      @param hasDate set true to load todos that have due date
      @param limit load only that many todos
      @param last last loaded todo due/creation date in return
      @return number of loaded todos, or -1 on error
    */
    virtual int loadCompletedTodos(bool hasDate, int limit, KDateTime *last) = 0;

    /**
      Load incidences based on start/due date or creation date.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadIncidences(bool hasDate, int limit, KDateTime *last) = 0;

    /**
      Load future incidences based on start/due date.

      Load direction is ascending, i.e., starting from the oldest
      event that is still valid at the day of the loadIncidences
      call. (=end time > 00:00:00 on that day).

      @param limit load only that many incidences
      @param last last loaded incidence start date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadFutureIncidences(int limit, KDateTime *last) = 0;

    /**
      Load incidences that have location information based on parameters.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadGeoIncidences(bool hasDate, int limit, KDateTime *last) = 0;

    /**
      Load all unread incidences that are related to an invitation.

      @return number of loaded incidences, or -1 on error
    */
    virtual int loadUnreadInvitationIncidences() = 0;

    /**
      Load incidences that are related to an invitation.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadOldInvitationIncidences(int limit, KDateTime *last) = 0;

    /**
      Load all contacts in the database. Doesn't put anything into calendar.
      Resulting list of persons is ordered by the number of appearances.
      Use Person::count to get the number of appearances.

      @return ordered list of persons
    */
    virtual KCalCore::Person::List loadContacts() = 0;

    /**
      Load all incidences that have the specified attendee.
      Also includes all shared notes (in a shared notebook).

      @param person person in question
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadContactIncidences(const KCalCore::Person::Ptr &person,
                                      int limit, KDateTime *last) = 0;

    /**
      Load journal entries based on parameters. Load direction is
      descending, i.e. starting from the most recently modified
      journal.

      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadJournals(int limit, KDateTime *last) = 0;

    /**
      Remove from storage all incidences that have been previously
      marked as deleted and that matches the UID / RecID of the incidences
      in list. The action is performed immediately on database.

      @return True on success, false otherwise.
     */
    virtual bool purgeDeletedIncidences(const KCalCore::Incidence::List &list) = 0;

    /**
      @copydoc
      CalStorage::save()
    */
    virtual bool save() = 0;

    /**
      This is an overload of save() method. When @deleteAction is
      PurgeDeleted, the deleted incidences are not marked as deleted but completely
      removed from the database and won't appear anymore when calling
      deletedIncidences().

      @param deleteAction the action to apply to deleted incidences
      @return True if successful; false otherwise
    */
    virtual bool save(DeleteAction deleteAction) = 0;

    /**
      Mark if supported by the storage that an incidence has been opened.
      This should be called only if the Incidence has been opened by the user
      and displayed all the contents. Being in a list doesn't qualify for it.

      @param incidence The incidence that has been opened
      @return True if sucessful; false otherwise
    */
    virtual bool notifyOpened(const KCalCore::Incidence::Ptr &incidence) = 0;

    /**
      Cancel any ongoing action (load etc.).

      @return true cancel was successful; false otherwise
    */
    virtual bool cancel() = 0;

    /**
      @copydoc
      CalStorage::close()
    */
    virtual bool close() = 0;

    // Internal Calendar Listener Methods //

    /**
      @copydoc
      Calendar::CalendarObserver::calendarModified()
    */
    virtual void calendarModified(bool modified, KCalCore::Calendar *calendar) = 0;

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdded()
    */
    virtual void calendarIncidenceAdded(const KCalCore::Incidence::Ptr &incidence) = 0;

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceChanged()
    */
    virtual void calendarIncidenceChanged(const KCalCore::Incidence::Ptr &incidence) = 0;

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceDeleted()
    */
    virtual void calendarIncidenceDeleted(const KCalCore::Incidence::Ptr &incidence) = 0;

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdditionCanceled()
    */
    virtual void calendarIncidenceAdditionCanceled(const KCalCore::Incidence::Ptr &incidence) = 0;

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
    virtual bool insertedIncidences(KCalCore::Incidence::List *list,
                                    const KDateTime &after = KDateTime(),
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
    virtual bool modifiedIncidences(KCalCore::Incidence::List *list,
                                    const KDateTime &after = KDateTime(),
                                    const QString &notebookUid = QString()) = 0;

    /**
      Get deleted incidences from storage.

      @param list deleted incidences
      @param after list only incidences deleted after or at given datetime
      @param notebookUid list only incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool deletedIncidences(KCalCore::Incidence::List *list,
                                   const KDateTime &after = KDateTime(),
                                   const QString &notebookUid = QString()) = 0;

    /**
      Get all incidences from storage.

      @param list notebook's incidences
      @param notebookUid list incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool allIncidences(KCalCore::Incidence::List *list,
                               const QString &notebookUid = QString()) = 0;

    /**
      Get possible duplicates for given incidence.

      @param list duplicate incidences
      @param incidence incidence to check
      @param notebookUid list incidences for given notebook
      @return true if execution was scheduled; false otherwise
    */
    virtual bool duplicateIncidences(KCalCore::Incidence::List *list,
                                     const KCalCore::Incidence::Ptr &incidence,
                                     const QString &notebookUid = QString()) = 0;

    /**
      Get deletion time of incidence

      @param incidence incidence to check
      @return valid deletion time of incidence in UTC if incidence has been deleted otherwise KDateTime()
    */
    virtual KDateTime incidenceDeletedDate(const KCalCore::Incidence::Ptr &incidence) = 0;

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

    // Observer Specific Methods //

    /**
      Registers an Observer for this Storage.

      @param observer is a pointer to an Observer object that will be
      watching this Storage.

      @see unregisterObserver()
     */
    void registerObserver(ExtendedStorageObserver *observer);

    /**
      Unregisters an Observer for this Storage.

      @param observer is a pointer to an Observer object that has been
      watching this Storage.

      @see registerObserver()
     */
    void unregisterObserver(ExtendedStorageObserver *observer);

    // Notebook Methods //

    /**
      Add new notebook to the storage.
      Notebook object is owned by the storage if operation succeeds.
      Operation is executed immediately into storage, @see modifyNotebook().

      @param nb notebook
      @param signal for modifynotebook. Default true, false only when database is initialized
      @return true if operation was successful; false otherwise.

      @note if the Notebook doesn't have a uid that is a valid UUID a new one will
      be generated on insertion.
    */
    bool addNotebook(const Notebook::Ptr &nb, bool signal = true);

    /**
      Update notebook parameters.
      Operation is executed immediately into storage, @see modifyNotebook().

      @param nb notebook
      @return true if add was successful; false otherwise.
    */
    bool updateNotebook(const Notebook::Ptr &nb);

    /**
      Delete notebook from storage.
      Operation is executed immediately into storage, @see modifyNotebook().

      @param nb notebook
      @param onlyMemory. If true deleting notebooks only from memory but not from database.
      Default false, true only when notebooks are reloaded from database
      @return true if delete was successful; false otherwise.
    */
    bool deleteNotebook(const Notebook::Ptr &nb, bool onlyMemory = false);

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
      Search for notebook in a list.

      @param list notebook list
      @param uid notebook uid
      @return pointer to notebook
    */
    Notebook::Ptr notebook(Notebook::List &list, const QString &uid);

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
    KCalCore::Incidence::Ptr checkAlarm(const QString &uid, const QString &recurrenceId,
                                        bool loadAlways = false);

    /**
      Creates and sets a default notebook. Usually called for an empty
      calendar.

      @param name notebook's name, if empty default used
      @param color notebook's color in format "#FF0042", if empty default used
      @return pointer to the created notebook
    */
    Notebook::Ptr createDefaultNotebook(QString name = QString(),
                                        QString color = QString());

    /**
      Standard trick to add virtuals later.

      @param id is any integer unique to this class which we will use to identify the method
             to be called.
      @param data is a pointer to some glob of data, typically a struct.
    */
    virtual void virtual_hook(int id, void *data) = 0;

protected:
    virtual bool loadNotebooks() = 0;
    virtual bool reloadNotebooks() = 0;
    virtual bool modifyNotebook(const Notebook::Ptr &nb, DBOperation dbop,
                                bool signal = true) = 0;

    bool getLoadDates(const QDate &start, const QDate &end,
                      KDateTime &loadStart, KDateTime &loadEnd);

    void setLoadDates(const QDate &start, const QDate &end);

    void setModified(const QString &info);
    void setProgress(const QString &info);
    void setFinished(bool error, const QString &info);

    // These alarm methods are used to communicate with an external
    // daemon, like timed, to bind Incidence::Alarm with the system notification.
    void clearAlarms(const KCalCore::Incidence::Ptr &incidence);
    void clearAlarms(const KCalCore::Incidence::List &incidences);
    void clearAlarms(const QString &nname);
    void setAlarms(const KCalCore::Incidence::Ptr &incidence);
    void setAlarms(const KCalCore::Incidence::List &incidences);
    void resetAlarms(const KCalCore::Incidence::List &incidences);
    void resetAlarms(const KCalCore::Incidence::Ptr &incidence);

    bool isUncompletedTodosLoaded();
    void setIsUncompletedTodosLoaded(bool loaded);

    bool isCompletedTodosDateLoaded();
    void setIsCompletedTodosDateLoaded(bool loaded);
    bool isCompletedTodosCreatedLoaded();
    void setIsCompletedTodosCreatedLoaded(bool loaded);

    bool isJournalsLoaded();
    void setIsJournalsLoaded(bool loaded);

    bool isDateLoaded();
    void setIsDateLoaded(bool loaded);
    bool isCreatedLoaded();
    void setIsCreatedLoaded(bool loaded);
    bool isFutureDateLoaded();
    void setIsFutureDateLoaded(bool loaded);

    bool isGeoDateLoaded();
    void setIsGeoDateLoaded(bool loaded);
    bool isGeoCreatedLoaded();
    void setIsGeoCreatedLoaded(bool loaded);

    bool isUnreadIncidencesLoaded();
    void setIsUnreadIncidencesLoaded(bool loaded);

    bool isInvitationIncidencesLoaded();
    void setIsInvitationIncidencesLoaded(bool loaded);

    void clearLoaded();

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(ExtendedStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

    friend class ::MkcalTool;
};

}

#endif
