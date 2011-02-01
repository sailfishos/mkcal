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

#ifdef MKCAL_TRACKER_SUPPORT

/**
  @file
  This file is part of the API for handling calendar data and
  defines the TrackerStorage class.

  This class is deprecated and not supported any more. Kept for
  history of the project, and as example of different storages.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
*/

#ifndef MKCAL_TRACKERSTORAGE_H
#define MKCAL_TRACKERSTORAGE_H

#include "extendedstorage.h"

#include <QtCore/QMetaType>
#include <QtCore/QVector>
#include <QtDBus/QtDBus>

Q_DECLARE_METATYPE( QVector<QStringList> )

namespace mKCal {

/**
  @brief
  This class provides a calendar storage as content framework tracker.
*/
class MKCAL_EXPORT TrackerStorage : public ExtendedStorage
{
  Q_OBJECT

  public:
    /**
      Constructs a new TrackerStorage object for Calendar.

      @param calendar is a pointer to a valid Calendar object.
      @param synchronuousMode set true to wait for completion
    */
    explicit TrackerStorage( const ExtendedCalendar::Ptr &cal, bool synchronuousMode = true );

    /**
      Destructor.
    */
    virtual ~TrackerStorage();

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
    bool load( const QString &uid, const KDateTime &recurrenceId = KDateTime() );

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
      ExtendedStorage::loadJournals( int limit, KDateTime *last )

      Dummy, doesn't do anything. Always returns 0
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
      ExtendedStorage::loadContactIncidences( const Person::Ptr & )
    */
    int loadContactIncidences( const KCalCore::Person::Ptr &person, int limit, KDateTime *last );

    /**
      @copydoc
      CalStorage::save()
    */
    bool save();

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
      Calendar::CalendarObserver::calendarIncidenceAdditionCanceled()
    */
    void calendarIncidenceAdditionCanceled( const KCalCore::Incidence::Ptr &incidence );

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

    // Methods for TrackerFormat //

    /**
      Called by TrackerFormat after loading an incidence.

      @param incidence loaded incidence
    */
    void loaded( const KCalCore::Incidence::Ptr &incidence );

    /**
      Called by TrackerFormat after loading incidences.

      @param error true if error happened
      @param message final message
    */
    void loaded( bool error, QString message );

    /**
      Called by TrackerFormat after saving an incidence.

      @param incidence saved incidence
    */
    void saved( const KCalCore::Incidence::Ptr &incidence );

    /**
      Called by TrackerFormat after saving incidences.

      @param error true if error happened
      @param message final message
    */
    void saved( bool error, QString message );

  public Q_SLOTS:
    void SubjectsAdded( QStringList const &subjects );
    void SubjectsRemoved( QStringList const &subjects );
    void SubjectsChanged( QStringList const &subjects );

  protected:
    bool loadNotebooks();
    bool reloadNotebooks();
    bool modifyNotebook( const Notebook::Ptr &nb, DBOperation dbop, bool signal = true );

  private:
    //@cond PRIVATE
    Q_DISABLE_COPY( TrackerStorage )
    class Private;
    Private *const d;
    //@endcond
};

}

#endif

#endif
