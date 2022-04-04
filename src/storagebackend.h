/*
  Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>.

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

#ifndef STORAGEBACKEND_H
#define STORAGEBACKEND_H

#include <QMultiHash>
#include <QObject>

#include <KCalendarCore/Incidence>

#include "mkcal_export.h"
#include "notebook.h"

class MkcalTool;
class tst_load;

namespace mKCal {

/**
  @brief
  This class provides a backend storage interface.
  Every action on the storage are synchronous.
*/
class MKCAL_EXPORT StorageBackend : public QObject
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

    enum DBOperation {
      DBNone,
      DBInsert,
      DBUpdate,
      DBMarkDeleted,
      DBDelete,
      DBSelect
    };

    /**
      A shared pointer to a StorageBackend
    */
    typedef QSharedPointer<StorageBackend> Ptr;

    /**
      A list of incidences, indexed by notebook ids.
    */
    typedef QMultiHash<QString, KCalendarCore::Incidence::Ptr> Collection;

    /**
      Constructs a new StorageBackend object.

      @param timeZone the time zone in which dates are expressed.
    */
    explicit StorageBackend(const QTimeZone &timeZone);

    /**
      Destructor.
    */
    virtual ~StorageBackend();

    /**
      Returns the time zone of the backend.
     */
    QTimeZone timeZone() const;

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
    virtual bool load(const QString &uid, const QDateTime &recurrenceId = QDateTime()) = 0;

    /**
      Load incidences at given date into the memory. All incidences that
      happens within date, or starts / ends within date or span
      during date are loaded into memory. The time zone used to expand
      date into points in time is the time zone of the associated calendar.
      In addition, all recurring events are also loaded into memory since
      there is no way to know in advance if they will have occurrences
      intersecting date. Internally, recurring incidences and incidences of
      date are cached to avoid loading them several times.

      @param date date
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QDate &date) = 0;

    /**
      Load incidences between given dates into the memory. start is inclusive,
      while end is exclusive. The same definitions and restrictions for loading
      apply as for load(const QDate &) method.

      @param start is the starting date
      @param end is the ending date, exclusive
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QDate &start, const QDate &end) = 0;

    /**
      Load all incidences sharing the same uid into the memory.

      @param uid is uid of the series
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadSeries(const QString &uid) = 0;

    /**
      Load the incidence matching the given identifier. This method may be
      more fragile than load(uid, recid) though since the instanceIdentifier
      is not saved as is in the database.

      @param instanceIdentifier is an identifier returned by Incidence::instanceIdentifier()
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadIncidenceInstance(const QString &instanceIdentifier) = 0;

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
    virtual int loadCompletedTodos(bool hasDate, int limit, QDateTime *last) = 0;

    /**
      Load incidences based on start/due date or creation date.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadIncidences(bool hasDate, int limit, QDateTime *last) = 0;

    /**
      Load future incidences based on start/due date.

      Load direction is ascending, i.e., starting from the oldest
      event that is still valid at the day of the loadIncidences
      call. (=end time > 00:00:00 on that day).

      @param limit load only that many incidences
      @param last last loaded incidence start date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadFutureIncidences(int limit, QDateTime *last) = 0;

    /**
      Load incidences that have location information based on parameters.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadGeoIncidences(bool hasDate, int limit, QDateTime *last) = 0;

    /**
      Load all contacts in the database. Doesn't put anything into calendar.
      Resulting list of persons is ordered by the number of appearances.
      Use Person::count to get the number of appearances.

      @return ordered list of persons
    */
    virtual KCalendarCore::Person::List loadContacts() = 0;

    /**
      Load all incidences that have the specified attendee.
      Also includes all shared notes (in a shared notebook).

      @param person person in question
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadContactIncidences(const KCalendarCore::Person &person,
                                      int limit, QDateTime *last) = 0;

    /**
      Load journal entries based on parameters. Load direction is
      descending, i.e. starting from the most recently modified
      journal.

      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadJournals(int limit, QDateTime *last) = 0;

    /**
      Remove from storage all incidences that have been previously
      marked as deleted and that matches the UID / RecID of the incidences
      in list. The action is performed immediately on database.

      @return True on success, false otherwise.
     */
    virtual bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list) = 0;

    /**
      Execute the given changes in the database.

      @param additions incidences to be added to the database, sorted
      by notebook id. Any incidences marked as deleted sharing the
      same UID/RecID pair are purged before insertion.
      @param modifications incidences to be updated in the database,
      sorted by notebook id.
      @param deletions incidences to be either marked as deleted or
      simply purged from the database, sorted by notebook id.
      @param deleteAction specifies the action to be performed for
      deleted incidences.

      @return True on success, false otherwise.
     */
    virtual bool storeIncidences(const Collection &additions,
                                 const Collection &modifications,
                                 const Collection &deletions,
                                 StorageBackend::DeleteAction deleteAction) = 0;

    /**
      Closes the storage backend.
      @return true if the close was successful; false otherwise.
    */
    virtual bool close() = 0;

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

    // Observer Specific Methods //
    class MKCAL_EXPORT Observer
    {
    public:
        /**
          Destructor.
        */
        virtual ~Observer();

        /**
          Notify the Observer that a storage has been opened.

          @param storage is a pointer to the StorageBackend object that
          is being observed.
          @param notebooks is a list of notebooks in the storage.
          @param defaultNotebook is the default notebook in the storage.
        */
        virtual void storageOpened(StorageBackend *storage,
                                   const Notebook::List &notebooks,
                                   const Notebook::Ptr &defaultNotebook);

        /**
          Notify the Observer that a storage has been closed.

          @param storage is a pointer to the StorageBackend object that
          is being observed.
        */
        virtual void storageClosed(StorageBackend *storage);

        /**
          Notify the Observer that a storage has been modified.
          The content of modifications is unknown and potentialy
          all may have been changed.

          @param storage is a pointer to the StorageBackend object that
          is being observed.
          @param notebooks is a list of notebooks in the storage
          after the external modifications.
          @param defaultNotebook is the new default notebook in the storage.
        */
        virtual void storageModified(StorageBackend *storage,
                                     const Notebook::List &notebooks, const Notebook::Ptr &defaultNotebook);

        /**
          Notify the Observer that a Storage has been updated to reflect the
          content of the associated calendar. This notification is delivered
          because of local changes done in-process by a call to
          StorageBackend::store() for instance.

          See also storageModified() for a notification for modifications
          done to the database by an external process.

          @param storage is a pointer to the StorageBackend object that
          is being observed.
          @param added is a list of newly added incidences in the storage
          @param modified is a list of updated incidences in the storage
          @param deleted is a list of deleted incidences from the storage
        */
        virtual void storageUpdated(StorageBackend *storage,
                                    const Collection &added,
                                    const Collection &modified,
                                    const Collection &deleted);

        /**
          Notify the Observer that incidences have been loaded from storage.
          The loaded incidence are sorted with their notebook id.

          @param storage is a pointer to the StorageBackend object that
          is being observed.
          @param incidences is a list of incidences, sorted by notebook ids.
        */
        virtual void incidenceLoaded(StorageBackend *storage,
                                     const Collection &incidences);
    };

    /**
      Registers an Observer for this Storage.

      @param observer is a pointer to an Observer object that will be
      watching this Storage.

      @see unregisterObserver()
     */
    void registerObserver(Observer *observer);

    /**
      Unregisters an Observer for this Storage.

      @param observer is a pointer to an Observer object that has been
      watching this Storage.

      @see registerObserver()
     */
    void unregisterObserver(Observer *observer);

    // Notebook Methods //

    /**
      Add new notebook to the storage.
      Notebook object is owned by the storage if operation succeeds.

      @param nb notebook
      @param isDefault true if nb is the default notebook for the storage.
      @return true if operation was successful; false otherwise.
    */
    bool addNotebook(const Notebook &nb, bool isDefault = false);

    /**
      Update notebook parameters.

      @param nb notebook
      @param isDefault true if nb is the default notebook for the storage.
      @return true if add was successful; false otherwise.
    */
    bool updateNotebook(const Notebook &nb, bool isDefault = false);

    /**
      Delete notebook from storage.

      @param nb notebook
      @return true if delete was successful; false otherwise.
    */
    bool deleteNotebook(const Notebook &nb);

    /**
      Creates a default notebook. The notebook is not added to the storage.

      @param name notebook's name, if empty default used
      @param color notebook's color in format "#FF0042", if empty default used
      @return pointer to the created notebook
    */
    static Notebook::Ptr createDefaultNotebook(QString name = QString(),
                                               QString color = QString());

protected:
    virtual bool modifyNotebook(const Notebook &nb, DBOperation dbop, bool isDefault) = 0;
    void setTimeZone(const QTimeZone &timeZone);

    bool getLoadDates(const QDate &start, const QDate &end,
                      QDateTime *loadStart, QDateTime *loadEnd) const;

    void addLoadedRange(const QDate &start, const QDate &end) const;
    bool isRecurrenceLoaded() const;
    void setIsRecurrenceLoaded(bool loaded);

    void storageOpened(const Notebook::List &notebooks, const Notebook::Ptr &defaultNotebook);
    void storageClosed();
    void storageModified(const Notebook::List &notebooks, const Notebook::Ptr &defaultNotebook);
    void storageUpdated(const Collection &added,
                        const Collection &modified,
                        const Collection &deleted);
    void incidenceLoaded(const Collection &incidences);

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

    void clearLoaded();

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(StorageBackend)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

    friend class ::MkcalTool;
    friend class ::tst_load;
};

}

#endif
