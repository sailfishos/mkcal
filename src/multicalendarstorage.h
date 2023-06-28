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
  This file is part of the API for handling calendar data and
  defines an interface to store multi KCalendarCore::Calendar
  and access data from them transparently.

  @author Damien Caliste \<dcaliste@free.fr\>
*/

#ifndef MKCAL_MULTICALENDARSTORAGE_H
#define MKCAL_MULTICALENDARSTORAGE_H

#include "notebook.h"

#include <QObject>

#include <KCalendarCore/MemoryCalendar>

namespace KCalendarCore {
class Incidence;
}

namespace mKCal {

/**
  @brief
  This class provides a way to aggregate different sources of
  calendar incidences.
*/
class Q_DECL_EXPORT MultiCalendarStorage: public QObject
{
    Q_OBJECT

public:

    /**
      Action to be performed on save for deleted incidences.
    */
    enum DeleteAction {
        MarkDeleted,
        PurgeDeleted,
        PurgeOnLocal
    };

    typedef QSharedPointer<MultiCalendarStorage> Ptr;

    /**
      Constructs a new MultiCalendarStorage object. The @param
      timezone is used to setup the MemoryCalendar that will be
      used to host the incidences in memory.

      @warning Once an Incidence has been added to the MultiCalendarStorage the UID
      cannot change. It is possible to do so through the API, but the internal
      hash tables will not be updated and hence the changes will not be tracked.
    */
    explicit MultiCalendarStorage(const QTimeZone &timezone = QTimeZone::systemTimeZone());

    virtual ~MultiCalendarStorage();

    /**
      Sets the time zone used by the memory calendars storing the incidences.

      @param timezone a time zone
      @return true if any of the internal MemoryCalendar changed its
              timezone definition.
    */
    bool setTimeZone(const QTimeZone &timezone);

    /**
      Open the storage. If it does not exits yet, it is
      initialised. Existing notebooks can then be obtained by notebooks().
    */
    virtual bool open();

    /**
      Load incidences between given dates into the memory. start is inclusive,
      while end is exclusive. Any recurring incidences or exceptions
      that appear to be within the window will trigger the full series
      to be loaded into memory.

      @param start is the starting date
      @param end is the ending date, exclusive
      @return true if the load was successful; false otherwise.
    */
    virtual bool load(const QDate &start, const QDate &end) = 0;

    /**
      Load the incidence matching the given identifier. If the
      incidence recurs or is an exception, the full series will be
      loaded into memory.

      @param multiCalendarIdentifier is an identifier returned by
             multiCalendarIdentifier()
      @return true if the load was successful; false otherwise.
    */
    virtual bool loadIncidenceInstance(const QString &multiCalendarIdentifier);

    /**
      Get all incidences from storage that match key. Incidences are
      loaded into their respective MemoryCalendar. More incidences than
      the listed ones in @param identifiers may be loaded into memory
      to ensure calendar consistency with respect to exceptions of
      recurring incidences.

      Matching is done on summary, description and location fields.

      @param key can be any substring from the summary, the description or the location.
      @param identifiers optional, stores the instance identifiers of
             matching incidences, as returned by multiCalendarIdentifier().
      @param limit the maximum number of loaded incidences, unlimited by default
      @return true on success.
     */
    virtual bool search(const QString &key, QStringList *identifiers, int limit = 0) = 0;

    /**
      Save changes to the storage. When @deleteAction is
      PurgeDeleted, the deleted incidences are not marked as deleted but completely
      removed from the database and won't appear anymore when calling
      deletedIncidences().

      @param notebookUid when not empty, commit changes only relevant
             to this notebook.
      @param deleteAction the action to apply to deleted incidences
      @return True if successful; false otherwise
    */
    virtual bool save(const QString &notebookUid = QString(),
                      DeleteAction deleteAction = MarkDeleted);

    /**
      Close the storage. notebooks() will then return an empty list
      and asociated MemoryCalendars are released. They are not closed
      though, and pointers to them are still valid.
    */
    virtual bool close();

    /**
      Retrieve the calendar associated to a notebook.

      @param notebookUid specifies a notebook.
      @return the MemoryCalendar storing the incidences of notebookUid
     */
    KCalendarCore::MemoryCalendar::Ptr calendar(const QString &notebookUid) const;

    /**
      Retrieve the incidence associated to the identifier. The incidence should
      already be loaded in memory, see loadIncidenceInstance().

      @param multiCalendarIdentifier is an identifier returned by
             multiCalendarIdentifier()
      @return the associated incidence, if any.
     */
    KCalendarCore::Incidence::Ptr instance(const QString &multiCalendarIdentifier) const;

    /**
      Retrieve the calendar holding the identifier.

      @param multiCalendarIdentifier is an identifier returned by
             multiCalendarIdentifier()
      @return the associated calendar, if any.
     */
    KCalendarCore::MemoryCalendar::Ptr calendarOfInstance(const QString &multiCalendarIdentifier) const;

    /**
      Retrieve the notebook associated to the identifier.

      @param multiCalendarIdentifier is an identifier returned by
             multiCalendarIdentifier()
      @return the associated notebook, if any.
     */
    Notebook::Ptr notebookOfInstance(const QString &multiCalendarIdentifier) const;

    /**
      List all notebooks.

      @return list of notebooks
    */
    Notebook::List notebooks() const;

    /**
      Search for notebook.

      @param uid notebook uid
      @return pointer to notebook
    */
    Notebook::Ptr notebook(const QString &uid) const;

    /**
      Add new notebook to the storage. No changes are done on the
      storage. Call save() to actually perform the addition.

      @return the newly created notebook for this storage.
    */
    Notebook::Ptr addNotebook();

    /**
      Delete notebook from storage. No changes are done on the
      storage. Call save() to actually perform the deletion.

      @param notebookUid notebook uid
      @return true if delete was successful; false otherwise.
    */
    bool deleteNotebook(const QString &notebookUid);

    /**
      setDefaultNotebook to the storage.

      @param nb notebook
      @return true if operation was successful; false otherwise.
    */
    bool setDefaultNotebook(const QString &notebookUid);

    /**
      defaultNotebook.

      @return pointer to default notebook.
    */
    Notebook::Ptr defaultNotebook();

    // Observer Specific Methods //

    /**
       @class MultiCalendarStorageObserver

       The MultiCalendarStorageObserver class.
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

           @param storage is a pointer to the MultiCalendarStorage object that
           is being observed.
        */
        virtual void storageModified(MultiCalendarStorage *storage);

        /**
           Notify the Observer that a Storage has been updated to reflect the
           content of the associated calendar. This notification is delivered
           because of local changes done in-process by a call to
           MultiCalendarStorage::save() for instance.

           See also storageModified() for a notification for modifications
           done to the database by an external process.

           @param storage is a pointer to the MultiCalendarStorage object that
           is being observed.
           @param notebookUid is the notebook the changes were done in
           @param added is a list of newly added incidences in the storage
           @param modified is a list of updated incidences in the storage
           @param deleted is a list of deleted incidences from the storage
        */
        virtual void storageUpdated(MultiCalendarStorage *storage,
                                    const QString &notebookUid,
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
    void registerObserver(MultiCalendarStorage::Observer *observer);

    /**
      Unregisters an Observer for this Storage.

      @param observer is a pointer to an Observer object that has been
      watching this Storage.

      @see registerObserver()
     */
    void unregisterObserver(MultiCalendarStorage::Observer *observer);

    /**
      Uniquely define an incidence belonging to a notebook.

      @return an identifier.
     */
    static QString multiCalendarIdentifier(const QString &notebookUid,
                                      const KCalendarCore::Incidence &incidence);

    /**
      Constructs a new MultiCalendarStorage object using the default
      implementation.

      @param timezone defines a time zone to represents the incidences
             in memory.
    */
    static Ptr systemStorage(const QTimeZone &timezone = QTimeZone::systemTimeZone());

protected:
    virtual Notebook::List loadedNotebooks(QString *defaultUid = nullptr) const = 0;
    virtual bool save(const QString &notebookUid,
                      const QHash<QString, KCalendarCore::Incidence::List> &added,
                      const QHash<QString, KCalendarCore::Incidence::List> &modified,
                      const QHash<QString, KCalendarCore::Incidence::List> &deleted,
                      DeleteAction deleteAction) = 0;

    virtual KCalendarCore::Incidence::List incidences(const QString &notebookUid,
                                                      const QString &uid) = 0;
    bool addIncidences(const QHash<QString, KCalendarCore::Incidence::List> &list);

    void emitStorageModified();
    void emitStorageUpdated(const QHash<QString, QStringList> &added,
                            const QHash<QString, QStringList> &modified,
                            const QHash<QString, QStringList> &deleted);
    void emitNotebookAdded();
    void emitNotebookUpdated(const Notebook &old);

    bool getLoadDates(const QDate &start, const QDate &end,
                      QDateTime *loadStart, QDateTime *loadEnd) const;

    void addLoadedRange(const QDate &start, const QDate &end) const;
    bool isRecurrenceLoaded() const;
    void setIsRecurrenceLoaded(bool loaded);

    static QString multiCalendarIdentifier(const QString &notebookUid,
                                           const QString &identifier);

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(MultiCalendarStorage)
    class Private;
    Private *const d;
    //@endcond
};

}

#endif
