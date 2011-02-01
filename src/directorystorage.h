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
  defines the DirectoryStorage class.

  Deprecated. Kept here for history of the code and as another
  example of a different storage.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/

#ifndef MKCAL_DIRECTORYSTORAGE_H
#define MKCAL_DIRECTORYSTORAGE_H

#ifdef MKCAL_DIRECTORY_SUPPORT

#include "mkcal_export.h"
#include "extendedstorage.h"

#include <calformat.h>

namespace mKCal {

/**
  @brief
  This class provides a calendar storage as local files in a directory, one file for each notebook.
*/
class MKCAL_EXPORT DirectoryStorage : public ExtendedStorage
{
  Q_OBJECT

  public:
    /**
      Constructs a new DirectoryStorage object for Calendar @p calendar with format
      @p format, and storage to file @p fileName.

      @param calendar is a pointer to a valid ExtendedCalendar object.
      @param directory is the name of the disk directory containing
      the Calendar data.
      @param format is a pointer to a valid CalFormat object that specifies
      the calendar format to be used. DirectoryStorage takes ownership;
      i.e., the memory for @p format is deleted by this destructor.
      If no format is specified, then iCalendar format is assumed.
      @param validateNotebooks set to true for saving only those incidences
      that belong to an existing notebook of this storage
    */
    explicit DirectoryStorage( const ExtendedCalendar::Ptr &calendar,
                               const QString &directory = QString(),
                               KCalCore::CalFormat *format = 0,
                               bool validateNotebooks = false );

    /**
      Destructor.
    */
    virtual ~DirectoryStorage();

    /**
      Returns a string containing the name of the calendar directory.
      @see setDirectory().
    */
    QString directory() const;

    /**
      Sets the CalFormat object to use for this storage.

      @param format is a pointer to a valid CalFormat object that specifies
      the calendar format to be used. DirectoryStorage takes ownership.
      @see format().
    */
    void setFormat( KCalCore::CalFormat *format );

    /**
      Returns a pointer to the CalFormat object used by this storage.
      @see setFormat().
    */
    KCalCore::CalFormat *format() const;

    /**
      Creates a snapshot for the storage.

      @param from directory containing the Calendar data.
      @param to directory for copying the Calendar data.
      @return true if snapshot was successful; false otherwise.
    */
    static bool snapshot( const QString &from, const QString &to );

    /**
      @copydoc
      CalStorage::open()
    */
    bool open();

    /**
      @copydoc
      CalStorage::load()
    */
    bool load();

    /**
      @copydoc
      ExtendedStorage::load(const QString &, const KDateTime &)
    */
    bool load( const QString &uid, const KDateTime &recurrenceId );

    /**
      @copydoc
      ExtendedStorage::load(const QDate &)
    */
    bool load( const QDate &date );

    /**
      @copydoc
      ExtendedStorage::load(const QDate &, const QDate &)
    */
    bool load( const QDate &start, const QDate &end );

    /**
      Load only one notebook into memory.

      @param notebook notebook name

      @return true if the load was successful; false otherwise.
    */
    bool load( const QString &notebook );

    /**
      @copydoc
      ExtendedStorage::loadNotebookIncidences(const QString &)
    */
    bool loadNotebookIncidences( const QString &notebookUid );

    /**
      @copydoc
      ExtendedStorage::loadJournals()
    */
    bool loadJournals();

    /**
      @copydoc
      ExtendedStorage::loadJournals()
    */
    int loadJournals( int limit, KDateTime *last );

    /**
      @copydoc
      ExtendedStorage::loadPlainIncidences()
    */
    bool loadPlainIncidences();

    /**
      @copydoc
      ExtendedStorage::loadRecurringIncidences()
    */
    bool loadRecurringIncidences();

    /**
      @copydoc
      ExtendedStorage::loadGeoIncidences()
    */
    bool loadGeoIncidences();

    /**
      @copydoc
      ExtendedStorage::loadGeoIncidences(float, float, float, float)
    */
    bool loadGeoIncidences( float geoLatitude, float geoLongitude,
                            float diffLatitude, float diffLongitude );

    /**
      @copydoc
      ExtendedStorage::loadAttendeeIncidences()
    */
    bool loadAttendeeIncidences();

    /**
      @copydoc
      ExtendedStorage::loadUncompletedTodos()
    */
    int loadUncompletedTodos();

    /**
      @copydoc
      ExtendedStorage::loadCompletedTodos()
    */
    int loadCompletedTodos( bool hasDate, int limit, KDateTime *last );

    /**
      @copydoc
      ExtendedStorage::loadIncidences( bool, bool, int, KDateTime* );
    */
    int loadIncidences( bool hasDate, int limit, KDateTime *last );

    /**
      @copydoc
      ExtendedStorage::loadFutureIncidences( bool, int, KDateTime* );
    */
    int loadFutureIncidences( int limit, KDateTime *last );

    /**
      @copydoc
      ExtendedStorage::loadGeoIncidences( bool, bool, int, KDateTime* );
    */
    int loadGeoIncidences( bool hasDate, int limit, KDateTime *last );

    /**
      @copydoc
      ExtendedStorage::loadUnreadInvitationIncidences()
    */
    int loadUnreadInvitationIncidences();

    /**
      @copydoc
      ExtendedStorage::loadOldInvitationIncidences()
    */
    int loadOldInvitationIncidences( int limit, KDateTime *last );

    /**
      @copydoc
      ExtendedStorage::loadContacts()
    */
    KCalCore::Person::List loadContacts();

    /**
      @copydoc
      ExtendedStorage::loadContactIncidences( const KCalCore::Person::Ptr & )
    */
    int loadContactIncidences( const KCalCore::Person::Ptr &person, int limit, KDateTime *last );

    /**
      @copydoc
      CalStorage::save()
    */
    bool save();

    /**
      Save only one notebook into disk.

      @param notebook notebook name

      @return true if the save was successful; false otherwise.
    */
    bool save( const QString &notebook );

    /**
      @copydoc
      ExtendedStorage::cancel()
    */
    bool cancel();

    /**
      @copydoc
      CalStorage::close()
    */
    bool close();

    // Internal Calendar Observer Methods //

    /**
      @copydoc
      Calendar::CalendarObserver::calendarModified()
    */
    void calendarModified( bool modified, KCalCore::Calendar *calendar );

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceCreated()
    */
    void calendarIncidenceCreated( const KCalCore::Incidence::Ptr &incidence );

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdded()
    */
    void calendarIncidenceAdded( const KCalCore::Incidence::Ptr &incidence );

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceChanged()
    */
    void calendarIncidenceChanged( const KCalCore::Incidence::Ptr &incidence );

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceDeleted()
    */
    void calendarIncidenceDeleted( const KCalCore::Incidence::Ptr &incidence );

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdditionCanceled()
    */
    void calendarIncidenceAdditionCanceled( const KCalCore::Incidence::Ptr &incidence );

    // Synchronization Specific Methods //

    /**
      @copydoc
      ExtendedStorage::insertedIncidences()
    */
    bool insertedIncidences( KCalCore::Incidence::List *list, const KDateTime &after,
                             const QString &notebook = QString() );

    /**
      @copydoc
      ExtendedStorage::modifiedIncidences()
    */
    bool modifiedIncidences( KCalCore::Incidence::List *list, const KDateTime &after,
                             const QString &notebook = QString() );

    /**
      @copydoc
      ExtendedStorage::deletedIncidences()
    */
    bool deletedIncidences( KCalCore::Incidence::List *list, const KDateTime &after,
                            const QString &notebook = QString() );

    /**
      @copydoc
      ExtendedStorage::allIncidences()
    */
    bool allIncidences( KCalCore::Incidence::List *list, const QString &notebook = QString() );

    /**
      @copydoc
      ExtendedStorage::duplicateIncidences()
    */
    bool duplicateIncidences( KCalCore::Incidence::List *list,
                              const KCalCore::Incidence::Ptr &incidence,
                              const QString &notebook = QString() );

    /**
      @copydoc
      ExtendedStorage::incidenceDeletedDate()
    */
    KDateTime incidenceDeletedDate( const KCalCore::Incidence::Ptr &incidence );

    /**
      @copydoc
      ExtendedStorage::notifyOpened( const KCalCore::Incidence::Ptr & )
    */
    bool notifyOpened( const KCalCore::Incidence::Ptr &incidence );

    /**
      @copydoc
      ExtendedStorage::virtual_hook()
    */
    virtual void virtual_hook( int id, void *data );

  protected:
    bool loadNotebooks();
    bool reloadNotebooks();
    bool modifyNotebook( const Notebook::Ptr &nb, DBOperation dbop, bool signal = true );

  private:
    //@cond PRIVATE
    Q_DISABLE_COPY( DirectoryStorage )
    class Private;
    Private *const d;
    //@endcond

  public Q_SLOTS:
    void fileChanged ( const QString & path );
    void directoryChanged ( const QString & path );

};

}

#endif

#endif
