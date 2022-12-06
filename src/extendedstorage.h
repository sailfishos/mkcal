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
#include "extendedstorageobserver.h"
#include "directstorageinterface.h"
#include "notebook.h"

#include <KCalendarCore/CalStorage>
#include <KCalendarCore/Calendar>

namespace KCalendarCore {
class Incidence;
}

class MkcalTool;
class tst_load;

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

enum DBLoadType {
    LOAD_NONE,
    LOAD_ALL,
    LOAD_BY_IDS,
    LOAD_SERIES,
    LOAD_BY_DATETIMES,
    LOAD_BY_NOTEBOOK,
    LOAD_JOURNALS,
    LOAD_PLAINS,
    LOAD_RECURSIVES,
    LOAD_BY_GEO,
    LOAD_BY_CREATED_GEO,
    LOAD_BY_ATTENDEE,
    LOAD_BY_UNCOMPLETED_TODOS,
    LOAD_BY_COMPLETED_TODOS,
    LOAD_BY_CREATED_COMPLETED_TODOS,
    LOAD_BY_END,
    LOAD_BY_CREATED,
    LOAD_BY_FUTURE,
    LOAD_BY_CONTACT
};

struct DBLoadOperation
{
    virtual ~DBLoadOperation() {}
    DBLoadOperation(DBLoadType type): type(type) {}
    DBLoadType type = LOAD_NONE;
    bool operator==(const DBLoadOperation &other) const
    {
        return compare(other);
    }
protected:
    DBLoadOperation(const DBLoadOperation &other): type(other.type) {}
    virtual bool compare(const DBLoadOperation &other) const
    {
        return type == other.type;
    }
};

struct DBLoadAll: public DBLoadOperation
{
    DBLoadAll(): DBLoadOperation(LOAD_ALL) {}
};

struct DBLoadByIds: public DBLoadOperation
{
    DBLoadByIds(const QString &uid, const QDateTime &recurrenceId)
        : DBLoadOperation(LOAD_BY_IDS)
        , uid(uid), recurrenceId(recurrenceId) {}
    QString uid;
    QDateTime recurrenceId;
protected:
    bool compare(const DBLoadOperation &other) const override
    {
        return DBLoadOperation::compare(other)
            && uid == static_cast<const DBLoadByIds*>(&other)->uid
            && recurrenceId == static_cast<const DBLoadByIds*>(&other)->recurrenceId;
    }
};

struct DBLoadSeries: public DBLoadOperation
{
    DBLoadSeries(const QString &uid)
        : DBLoadOperation(LOAD_SERIES), uid(uid) {}
    QString uid;
protected:
    bool compare(const DBLoadOperation &other) const override
    {
        return DBLoadOperation::compare(other)
            && uid == static_cast<const DBLoadSeries*>(&other)->uid;
    }
};

struct DBLoadByDateTimes: public DBLoadOperation
{
    DBLoadByDateTimes(const QDateTime &start, const QDateTime &end)
        : DBLoadOperation(LOAD_BY_DATETIMES), start(start), end(end) {}
    QDateTime start, end;
protected:
    bool compare(const DBLoadOperation &other) const override
    {
        return DBLoadOperation::compare(other)
            && start == static_cast<const DBLoadByDateTimes*>(&other)->start
            && end == static_cast<const DBLoadByDateTimes*>(&other)->end;
    }
};

struct DBLoadByNotebook: public DBLoadOperation
{
    DBLoadByNotebook(const QString &uid)
        : DBLoadOperation(LOAD_BY_NOTEBOOK), notebookUid(uid) {}
    QString notebookUid;
protected:
    bool compare(const DBLoadOperation &other) const override
    {
        return DBLoadOperation::compare(other)
            && notebookUid == static_cast<const DBLoadByNotebook*>(&other)->notebookUid;
    }
};

struct DBLoadPlainIncidences: public DBLoadOperation
{
    DBLoadPlainIncidences(): DBLoadOperation(LOAD_PLAINS) {}
};

struct DBLoadRecursiveIncidences: public DBLoadOperation
{
    DBLoadRecursiveIncidences(): DBLoadOperation(LOAD_RECURSIVES) {}
};

struct DBLoadAttendeeIncidences: public DBLoadOperation
{
    DBLoadAttendeeIncidences(): DBLoadOperation(LOAD_BY_ATTENDEE) {}
};

struct DBLoadUncompletedTodos: public DBLoadOperation
{
    DBLoadUncompletedTodos(): DBLoadOperation(LOAD_BY_UNCOMPLETED_TODOS) {}
};

struct DBLoadDateLimited: public DBLoadOperation
{
    DBLoadDateLimited(DBLoadType type, QDateTime *last)
        : DBLoadOperation(type), last(last) {}
    QDateTime *last = nullptr;
protected:
    bool compare(const DBLoadOperation &other) const override
    {
        return DBLoadOperation::compare(other)
            && ((!last && !static_cast<const DBLoadDateLimited*>(&other)->last)
                || (last && static_cast<const DBLoadDateLimited*>(&other)->last));
    }
};

struct DBLoadJournals: public DBLoadDateLimited
{
    DBLoadJournals(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_JOURNALS, last) {}
};

struct DBLoadGeoIncidences: public DBLoadDateLimited
{
    DBLoadGeoIncidences(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_GEO, last) {}
    DBLoadGeoIncidences(float latitude, float longitude, float diffLatitude, float diffLongitude)
        : DBLoadDateLimited(LOAD_BY_GEO, nullptr)
        , latitude(latitude), longitude(longitude)
        , diffLatitude(diffLatitude), diffLongitude(diffLongitude) {}
    float latitude, longitude;
    float diffLatitude = -1.f, diffLongitude = -1.f;
protected:
    bool compare(const DBLoadOperation &other) const override
    {
        return DBLoadDateLimited::compare(other)
            && latitude == static_cast<const DBLoadGeoIncidences*>(&other)->latitude
            && longitude == static_cast<const DBLoadGeoIncidences*>(&other)->longitude
            && diffLatitude == static_cast<const DBLoadGeoIncidences*>(&other)->diffLatitude
            && diffLongitude == static_cast<const DBLoadGeoIncidences*>(&other)->diffLongitude;
    }
};

struct DBLoadCreatedGeoIncidences: public DBLoadDateLimited
{
    DBLoadCreatedGeoIncidences(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_CREATED_GEO, last) {}
};

struct DBLoadCompletedTodos: public DBLoadDateLimited
{
    DBLoadCompletedTodos(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_COMPLETED_TODOS, last) {}
};

struct DBLoadCreatedAndCompletedTodos: public DBLoadDateLimited
{
    DBLoadCreatedAndCompletedTodos(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_CREATED_COMPLETED_TODOS, last) {}
};

struct DBLoadByEnd: public DBLoadDateLimited
{
    DBLoadByEnd(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_END, last) {}
};

struct DBLoadByCreated: public DBLoadDateLimited
{
    DBLoadByCreated(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_CREATED, last) {}
};

struct DBLoadFuture: public DBLoadDateLimited
{
    DBLoadFuture(QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_FUTURE, last) {};
};

struct DBLoadByContacts: public DBLoadDateLimited
{
    DBLoadByContacts(const KCalendarCore::Person &person, QDateTime *last = nullptr)
        : DBLoadDateLimited(LOAD_BY_CONTACT, last), person(person) {}
    const KCalendarCore::Person person;
protected:
    bool compare(const DBLoadOperation &other) const override
    {
        return DBLoadDateLimited::compare(other)
            && person == static_cast<const DBLoadByContacts*>(&other)->person;
    }
};

struct DBLoadOperationWrapper
{
    DBLoadOperationWrapper() {}
    DBLoadOperationWrapper(const DBLoadOperation *op)
    {
        if (!op) {
            return;
        }
        switch (op->type) {
        case (LOAD_ALL):
            dbop = new DBLoadAll(*static_cast<const DBLoadAll*>(op));
            return;
        case (LOAD_BY_IDS):
            dbop = new DBLoadByIds(*static_cast<const DBLoadByIds*>(op));
            return;
        case (LOAD_SERIES):
            dbop = new DBLoadSeries(*static_cast<const DBLoadSeries*>(op));
            return;
        case (LOAD_BY_DATETIMES):
            dbop = new DBLoadByDateTimes(*static_cast<const DBLoadByDateTimes*>(op));
            return;
        case (LOAD_BY_NOTEBOOK):
            dbop = new DBLoadByNotebook(*static_cast<const DBLoadByNotebook*>(op));
            return;
        case (LOAD_JOURNALS):
            dbop = new DBLoadJournals(*static_cast<const DBLoadJournals*>(op));
            return;
        case (LOAD_PLAINS):
            dbop = new DBLoadPlainIncidences(*static_cast<const DBLoadPlainIncidences*>(op));
            return;
        case (LOAD_RECURSIVES):
            dbop = new DBLoadRecursiveIncidences(*static_cast<const DBLoadRecursiveIncidences*>(op));
            return;
        case (LOAD_BY_GEO):
            dbop = new DBLoadGeoIncidences(*static_cast<const DBLoadGeoIncidences*>(op));
            return;
        case (LOAD_BY_CREATED_GEO):
            dbop = new DBLoadCreatedGeoIncidences(*static_cast<const DBLoadCreatedGeoIncidences*>(op));
            return;
        case (LOAD_BY_ATTENDEE):
            dbop = new DBLoadAttendeeIncidences(*static_cast<const DBLoadAttendeeIncidences*>(op));
            return;
        case (LOAD_BY_UNCOMPLETED_TODOS):
            dbop = new DBLoadUncompletedTodos(*static_cast<const DBLoadUncompletedTodos*>(op));
            return;
        case (LOAD_BY_COMPLETED_TODOS):
            dbop = new DBLoadCompletedTodos(*static_cast<const DBLoadCompletedTodos*>(op));
            return;
        case (LOAD_BY_CREATED_COMPLETED_TODOS):
            dbop = new DBLoadCreatedAndCompletedTodos(*static_cast<const DBLoadCreatedAndCompletedTodos*>(op));
            return;
        case (LOAD_BY_END):
            dbop = new DBLoadByEnd(*static_cast<const DBLoadByEnd*>(op));
            return;
        case (LOAD_BY_CREATED):
            dbop = new DBLoadByCreated(*static_cast<const DBLoadByCreated*>(op));
            return;
        case (LOAD_BY_FUTURE):
            dbop = new DBLoadFuture(*static_cast<const DBLoadFuture*>(op));
            return;
        case (LOAD_BY_CONTACT):
            dbop = new DBLoadByContacts(*static_cast<const DBLoadByContacts*>(op));
            return;
        default:
            dbop = new DBLoadOperation(op->type);
            return;
        }
    }
    DBLoadOperationWrapper(const DBLoadOperationWrapper &other)
        : DBLoadOperationWrapper(other.dbop)
    {
    }
    ~DBLoadOperationWrapper()
    {
        delete dbop;
    }
    const DBLoadOperation *dbop = nullptr;
};

/**
  @brief
  This class provides a calendar storage interface.
  Every action on the storage can be asynchronous, which means that actions
  are only scheduled for execution. Caller must use ExtendedStorageObserver to get
  notified about the completion.
*/
class MKCAL_EXPORT ExtendedStorage
    : public KCalendarCore::CalStorage
    , public DirectStorageInterface
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

      @warning Do not use storage as a global object, on closing the application
      it can dead lock. If you do so, be ready to destroy it manually before the
      application closes.

      @warning Once an Incidence has been added to the ExtendedStorage the UID
      cannot change. It is possible to do so through the API, but the internal
      hash tables will not be updated and hence the changes will not be tracked.
    */
    explicit ExtendedStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks = true);

    /**
      Destructor.
    */
    virtual ~ExtendedStorage();

    /**
      @copydoc
      CalStorage::open()
    */
    virtual bool open();

    /**
      @copydoc
      CalStorage::load()
    */
    virtual bool load();

    /**
      Load incidence by uid into the memory.

      @param uid is uid of incidence
      @param recurrenceid is recurrenceid of incidence, default null
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QString &uid, const QDateTime &recurrenceId = QDateTime());

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
    virtual bool load(const QDate &date);

    /**
      Load incidences between given dates into the memory. start is inclusive,
      while end is exclusive. The same definitions and restrictions for loading
      apply as for load(const QDate &) method.

      @param start is the starting date
      @param end is the ending date, exclusive
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QDate &start, const QDate &end);

    /**
      Load all incidences sharing the same uid into the memory.

      @param uid is uid of the series
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadSeries(const QString &uid);

    /**
      Load the incidence matching the given identifier. This method may be
      more fragile than load(uid, recid) though since the instanceIdentifier
      is not saved as is in the database.

      @param instanceIdentifier is an identifier returned by Incidence::instanceIdentifier()
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadIncidenceInstance(const QString &instanceIdentifier);

    /**
      Load incidences of one notebook into the memory.

      @param notebookUid is uid of notebook
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadNotebookIncidences(const QString &notebookUid);

    /**
      Load journal type entries
    */
    virtual bool loadJournals();

    /**
      Load plain incidences (no startdate and no enddate).

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadPlainIncidences();

    /**
      Load recurring incidences.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadRecurringIncidences();

    /**
      Load incidences that have geo parameters.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadGeoIncidences();

    /**
      Load incidences that have geo parameters inside given rectangle.

      @param geoLatitude latitude
      @param geoLongitude longitude
      @param diffLatitude maximum latitudinal difference
      @param diffLongitude maximum longitudinal difference
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadGeoIncidences(float geoLatitude, float geoLongitude,
                                   float diffLatitude, float diffLongitude);

    /**
      Load incidences that have attendee.

      @return true if the load was successful; false otherwise.
    */
    virtual bool loadAttendeeIncidences();

    /**
      Will actually perform any load action when runBatchLoading() is called.
    */
    bool startBatchLoading();

    /**
      Run every load actions since the last call to startBatchLoading().
    */
    bool runBatchLoading();

    // Smart Loading Functions //

    /**
      Load all uncompleted todos.

      @return number of loaded todos, or -1 on error
    */
    virtual int loadUncompletedTodos();

    /**
      Load completed todos based on parameters. Load direction is descending,
      i.e., starting from most distant upcoming todo.

      @param hasDate set true to load todos that have due date
      @param limit load only that many todos
      @param last last loaded todo due/creation date in return
      @return number of loaded todos, or -1 on error
    */
    virtual int loadCompletedTodos(bool hasDate, int limit, QDateTime *last);

    /**
      Load incidences based on start/due date or creation date.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadIncidences(bool hasDate, int limit, QDateTime *last);

    /**
      Load future incidences based on start/due date.

      Load direction is ascending, i.e., starting from the oldest
      event that is still valid at the day of the loadIncidences
      call. (=end time > 00:00:00 on that day).

      @param limit load only that many incidences
      @param last last loaded incidence start date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadFutureIncidences(int limit, QDateTime *last);

    /**
      Load incidences that have location information based on parameters.
      Load direction is descending, i.e., starting from most distant
      upcoming incidence.

      @param hasDate set true to load incidences that have start/due date
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadGeoIncidences(bool hasDate, int limit, QDateTime *last);

    /**
      Load all incidences that have the specified attendee.
      Also includes all shared notes (in a shared notebook).

      @param person person in question
      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadContactIncidences(const KCalendarCore::Person &person,
                                      int limit, QDateTime *last);

    /**
      Load journal entries based on parameters. Load direction is
      descending, i.e. starting from the most recently modified
      journal.

      @param limit load only that many incidences
      @param last last loaded incidence due/creation date in return
      @return number of loaded incidences, or -1 on error
    */
    virtual int loadJournals(int limit, QDateTime *last);

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
      Mark if supported by the storage that an incidence has been opened.
      This should be called only if the Incidence has been opened by the user
      and displayed all the contents. Being in a list doesn't qualify for it.

      @param incidence The incidence that has been opened
      @return True if sucessful; false otherwise
    */
    virtual bool notifyOpened(const KCalendarCore::Incidence::Ptr &incidence) = 0;

    /**
      Cancel any ongoing action (load etc.).

      @return true cancel was successful; false otherwise
    */
    virtual bool cancel() = 0;

    /**
      @copydoc
      CalStorage::close()
    */
    virtual bool close();

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
      @return true if operation was successful; false otherwise.

      @note if the Notebook doesn't have a uid that is a valid UUID a new one will
      be generated on insertion.
    */
    bool addNotebook(const Notebook::Ptr &nb);

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
      @return true if delete was successful; false otherwise.
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
    Notebook::Ptr notebook(const QString &uid) const;

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
    bool validateNotebooks() const;

    /**
      Returns true if the given notebook is valid for the storage.
      That means that storage can load/save incidences on this notebook.

      @param notebookUid notebook uid
      @return true or false
    */
    bool isValidNotebook(const QString &notebookUid) const;

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

    /**
      Creates and sets a default notebook. Usually called for an empty
      calendar.

      Notice: deprecated since 0.6.10. Instead, create a notebook
              and call setDefaultNotebook().

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

    virtual void registerDirectObserver(DirectStorageInterface::Observer *observer) = 0;
    virtual void unregisterDirectObserver(DirectStorageInterface::Observer *observer) = 0;

protected:
    virtual bool loadNotebooks() = 0;
    virtual bool modifyNotebook(const Notebook::Ptr &nb, DBOperation dbop) = 0;
    virtual bool loadBatch(const QList<DBLoadOperationWrapper> &dbops) = 0;
    virtual bool loadIncidences(const DBLoadOperation &dbop) = 0;
    virtual int loadIncidences(const DBLoadDateLimited &dbop, QDateTime *last,
                               int limit = -1, bool useDate = false, bool ignoreEnd = false) = 0;
    virtual bool storeIncidences(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &additions,
                                 const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &modifications,
                                 const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &deletions,
                                 ExtendedStorage::DeleteAction deleteAction) = 0;

    bool getLoadDates(const QDate &start, const QDate &end,
                      QDateTime *loadStart, QDateTime *loadEnd) const;

    void addLoadedRange(const QDate &start, const QDate &end) const;
    bool isRecurrenceLoaded() const;
    void setIsRecurrenceLoaded(bool loaded);

    bool runLoadOperation(const DBLoadOperation &dbop);
    void incidenceLoaded(const DBLoadOperationWrapper &wrapper, int count, int limit,
                         const QMultiHash<QString, KCalendarCore::Incidence*> &incidences);
    void incidenceLoadedByBatch(const QList<DBLoadOperationWrapper> &wrappers,
                                const QList<bool> &results,
                                const QMultiHash<QString, KCalendarCore::Incidence*> &incidences);
    
    void setLoadOperationDone(const DBLoadOperation &dbop, int count, int limit = -1);
    void setLoaded(const QMultiHash<QString, KCalendarCore::Incidence*> &incidences);

    void setOpened(const QList<Notebook*> notebooks, Notebook* defaultNb);
    void setClosed();
    void setModified(const QString &info);
    void setFinished(bool error, const QString &info);
    void setUpdated(const KCalendarCore::Incidence::List &added,
                    const KCalendarCore::Incidence::List &modified,
                    const KCalendarCore::Incidence::List &deleted);

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

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(ExtendedStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

    friend class ::MkcalTool;
    friend class ::tst_load;
};

}

#endif
