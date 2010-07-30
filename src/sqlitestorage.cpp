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
  defines the SqliteStorage class.

  @brief
  This class provides a calendar storage as an sqlite database.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
*/
#include <config-mkcal.h>
#include "sqlitestorage.h"
#include "sqliteformat.h"
#include "trackermodify.h"

#include <icalformat.h>
using namespace KCalCore;

#include <kdebug.h>
#include <ksystemtimezone.h>

#include <QFileSystemWatcher>
#include <QSystemSemaphore>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingCall>

#include <iostream>
using namespace std;

#if defined(HAVE_UUID_UUID_H)
#include <uuid/uuid.h>
#endif

using namespace mKCal;

#if defined(MKCAL_FOR_MEEGO)
#include <MLocale>
#endif

/**
 * \brief What have we loaded?
 *
 * This is useful for making the reloading be transparent when the
 * on-disk storage changes.
 */
enum {
  LOADED_LOAD = ( 1 << 0 ),
  LOADED_LOAD_JOURNALS = ( 1 << 1 ),
  LOADED_LOAD_RECURRING_INCIDENCES = ( 1 << 2 )
};

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::SqliteStorage::Private
{
  public:
    Private( const ExtendedCalendar::Ptr calendar, SqliteStorage *storage,
             const QString &databaseName, bool useTracker )
      : mCalendar( calendar ),
        mStorage( storage ),
        mDatabaseName( databaseName ),
        mSem( databaseName, 1, QSystemSemaphore::Create ),
        mChanged( databaseName + ".changed" ),
        mWatcher( 0 ),
        mDatabase( 0 ),
        mFormat( 0 ),
        mUseTracker( useTracker ),
        mDBusIf( 0 ),
        mIsLoading( false ),
        mIsOpened( false ),
        mLoaded( 0 )
    {}
    ~Private()
    {}

    void modifyTracker( Incidence::Ptr incidence, DBOperation dbop, const QString notebookUid )
    {
      QStringList insertQuery;
      QStringList deleteQuery;
      TrackerModify tracker;

      if ( tracker.queries( incidence, dbop, insertQuery, deleteQuery, notebookUid ) ) {
        if ( dbop != DBInsert ) {
          QString query = deleteQuery.join( QString() );
#ifndef QT_NO_DEBUG
          // Use cerr to print only queries.
          cerr << endl << query.toAscii().constData() << endl;
          //kDebug() << "tracker query:" << select;
#endif
#if defined(MKCAL_TRACKER_SYNC)
          QDBusPendingReply<> update = mDBusIf->asyncCall( "SparqlUpdate", query );
          update.waitForFinished();
          if ( update.isError() ) {
            kError() << "tracker query error:" << update.error().message();
          }
#else
          (void)mDBusIf->asyncCall( "SparqlUpdate", query );
#endif
        }
        if ( dbop != DBDelete ) {
          QString query = insertQuery.join( QString() );
#ifndef QT_NO_DEBUG
          // Use cerr to print only queries.
          cerr << endl << query.toAscii().constData() << endl;
          //kDebug() << "tracker query:" << select;
#endif
#if defined(MKCAL_TRACKER_SYNC)
          QDBusPendingReply<> update = mDBusIf->asyncCall( "SparqlUpdate", query );
          update.waitForFinished();
          if ( update.isError() ) {
            kError() << "tracker query error:" << update.error().message();
          }
#else
          (void)mDBusIf->asyncCall( "SparqlUpdate", query );
#endif
        }
      }
    }
    ExtendedCalendar::Ptr mCalendar;
    SqliteStorage *mStorage;
    QString mDatabaseName;
    QSystemSemaphore mSem;
    QFile mChanged;
    QFileSystemWatcher *mWatcher;
    sqlite3 *mDatabase;
    SqliteFormat *mFormat;
    bool mUseTracker;
    QDBusInterface *mDBusIf;
    QMultiHash<QString,Incidence::Ptr> mIncidencesToInsert;
    QMultiHash<QString,Incidence::Ptr> mIncidencesToUpdate;
    QMultiHash<QString,Incidence::Ptr> mIncidencesToDelete;
    QHash<QString, QString> mUidMappings;
    bool mIsLoading;
    bool mIsOpened;
    int mLoaded;
    KDateTime mOriginTime;
    QDateTime mPreWatcherDbTime;

    int loadIncidences( sqlite3_stmt *stmt1,
                        const char *query2, int qsize2, const char *query3, int qsize3,
                        const char *query4, int qsize4, const char *query5, int qsize5,
                        const char *query6, int qsize6,
                        int limit = -1, KDateTime *last = NULL, bool useDate = false );
    bool saveIncidences( QHash<QString,Incidence::Ptr> &list, DBOperation dbop,
                         const char *query1, int qsize1, const char *query2, int qsize2,
                         const char *query3, int qsize3, const char *query4, int qsize4,
                         const char *query5, int qsize5,  const char *query6, int qsize6,
                         const char *query7, int qsize7, const char *query8, int qsize8,
                         const char *query9, int qsize9, const char *query10, int qsize10,
                         const char *query11, int qsize11 );
    bool selectIncidences( Incidence::List *list,
                           const char *query1, int qsize1, const char *query2, int qsize2,
                           const char *query3, int qsize3, const char *query4, int qsize4,
                           const char *query5, int qsize5,const char *query6, int qsize6,
                           DBOperation dbop, const KDateTime &after,
                           const QString &notebookUid, const QString &summary = QString() );
    bool checkVersion();
    bool saveTimezones();
    bool loadTimezones();
    bool notifyOpened( Incidence::Ptr incidence );

};
//@endcond

SqliteStorage::SqliteStorage( const ExtendedCalendar::Ptr cal, const QString &databaseName,
                              bool useTracker, bool validateNotebooks )
  : ExtendedStorage( cal, validateNotebooks ),
    d( new Private( cal, this, databaseName, useTracker ) )
{
  d->mOriginTime = KDateTime( QDate( 1970, 1, 1 ), QTime( 0, 0, 0 ), KDateTime::UTC );
  kDebug() << "time of origin is " << d->mOriginTime << d->mOriginTime.toTime_t();
  cal->registerObserver( this );
}

SqliteStorage::~SqliteStorage()
{
  calendar()->unregisterObserver( this );
  close();
  delete d;
}

QString SqliteStorage::databaseName() const
{
  return d->mDatabaseName;
}

bool SqliteStorage::useTracker() const
{
  return d->mUseTracker;
}

bool SqliteStorage::open()
{
  int rv;
  char *errmsg = NULL;
  const char *query = NULL;
  Notebook::List list;

  if ( d->mIsOpened ) {
    return false;
  }

  if ( !d->mSem.acquire() ) {
    kError() << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    return false;
  }

  if (d->mUseTracker) {
    QDBusConnection bus = QDBusConnection::sessionBus();

    if ( !bus.isConnected() )  {
      kError() << "DBus connection failed";
      return false;
    }

    bool retried = false;
  retry:
    QStringList serviceNames = bus.interface()->registeredServiceNames();
    qDebug() << "DBus service names:" << serviceNames;

    d->mDBusIf =
      new QDBusInterface( "org.freedesktop.Tracker1", "/org/freedesktop/Tracker1/Resources",
                          "org.freedesktop.Tracker1.Resources", bus );
    // TODO delete row above and uncomment row below after tracker has released new protected interfaces
    //d->mDBusIf = new QDBusInterface( "org.fd.T.Resources.SparqlUpdate",
    //                                 "/org/fd/T/Resources/SparqlUpdate",
    //                                 "org.fd.T.Resources.SparqlUpdate", bus );
    if ( !d->mDBusIf->isValid() ) {
      if ( d->mDBusIf ) {
        delete d->mDBusIf;
        d->mDBusIf = 0;
      }
      if ( !retried ) {
        // Try again, let tracker to start.
        retried = true;
        goto retry;
      }
      kError() << "Could not establish a DBus connection to Tracker";
      return false;
    }
  }
  KDateTime::setFromStringDefault( KDateTime::Spec::UTC() );

  rv = sqlite3_open( d->mDatabaseName.toUtf8(), &d->mDatabase );
  if ( rv ) {
    kError() << "sqlite3_open error:" << rv << "on database" << d->mDatabaseName;
    kError() << sqlite3_errmsg( d->mDatabase );
    sqlite3_close( d->mDatabase );
    return false;
  }
  kWarning() << "database" << d->mDatabaseName << "opened";

  d->mIsOpened = true;

  // Set one and half second busy timeout for waiting for internal sqlite locks
  sqlite3_busy_timeout( d->mDatabase, 1500 );

  /* Create Calendars, Components, etc. tables */
  query = CREATE_VERSION;
  sqlite3_exec( d->mDatabase );

  query = CREATE_TIMEZONES;
  sqlite3_exec( d->mDatabase );
  // Create a global empty entry.
  query = INSERT_TIMEZONES;
  sqlite3_exec( d->mDatabase );

  query = CREATE_CALENDARS;
  sqlite3_exec( d->mDatabase );

  query = CREATE_COMPONENTS;
  sqlite3_exec( d->mDatabase );

  query = CREATE_RDATES;
  sqlite3_exec( d->mDatabase );

  query = CREATE_CUSTOMPROPERTIES;
  sqlite3_exec( d->mDatabase );

  query = CREATE_RECURSIVE;
  sqlite3_exec( d->mDatabase );

  query = CREATE_ALARM;
  sqlite3_exec( d->mDatabase );

  query = CREATE_ATTENDEE;
  sqlite3_exec( d->mDatabase );

  /* Create index on frequently used columns */
  query = INDEX_CALENDAR;
  sqlite3_exec( d->mDatabase );

  query = INDEX_COMPONENT;
  sqlite3_exec( d->mDatabase );

  query = INDEX_COMPONENT_UID;
  sqlite3_exec( d->mDatabase );

  query = INDEX_COMPONENT_NOTEBOOK;
  sqlite3_exec( d->mDatabase );

  query = INDEX_RDATES;
  sqlite3_exec( d->mDatabase );

  query = INDEX_CUSTOMPROPERTIES;
  sqlite3_exec( d->mDatabase );

  query = INDEX_RECURSIVE;
  sqlite3_exec( d->mDatabase );

  query = INDEX_ALARM;
  sqlite3_exec( d->mDatabase );

  query = INDEX_ATTENDEE;
  sqlite3_exec( d->mDatabase );

  if ( !d->mChanged.open( QIODevice::WriteOnly ) ) {
    kError() << "cannot open changed file for" << d->mDatabaseName;
    goto error;
  }
  d->mPreWatcherDbTime = QFileInfo( d->mDatabaseName + ".changed" ).lastModified();
  d->mWatcher = new QFileSystemWatcher();
  d->mWatcher->addPath( d->mDatabaseName + ".changed" );
  connect( d->mWatcher, SIGNAL(fileChanged(const QString &)),
           this, SLOT(fileChanged(const QString &)) );

  d->mFormat = new SqliteFormat( this, d->mDatabase );

  if ( !d->checkVersion() ) {
    goto error;
  }

  if ( !d->mSem.release() ) {
    kError() << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    goto error;
  }

  if ( !d->loadTimezones() ) {
    kError() << "cannot load timezones from calendar";
    goto error;
  }

  if ( !loadNotebooks() ) {
    kError() << "cannot load notebooks from calendar";
    goto error;
  }

  list = notebooks();
  if ( list.isEmpty() ) {
    initializeDatabase();
  }

  return true;

 error:
  if ( !d->mSem.release() ) {
    kError() << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
  }
  close();
  return false;
}

bool SqliteStorage::load()
{
  d->mLoaded |= LOADED_LOAD;
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_ALL;
  qsize1 = sizeof( SELECT_COMPONENTS_ALL );

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::load( const QString &uid, const KDateTime &recurrenceId )
{
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;
  QByteArray u;
  qint64 secsRecurId;

  if ( !uid.isEmpty() ) {
    query1 = SELECT_COMPONENTS_BY_UID_AND_RECURID;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_UID_AND_RECURID );

    sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
    u = uid.toUtf8();
    sqlite3_bind_text( stmt1, index, u.constData(), u.length(), SQLITE_STATIC );
    if ( recurrenceId.isValid() ) {
      secsRecurId = toOriginTime( recurrenceId );
      sqlite3_bind_int64( stmt1, index, secsRecurId );
    } else {
      // no recurrenceId, bind NULL
      // note that sqlite3_bind_null doesn't seem to work here
      // also note that sqlite should bind NULL automatically if nothing
      // is bound, but that doesn't work either
      sqlite3_bind_int64( stmt1, index, 0 );
    }
    query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
    qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

    query3 = SELECT_ATTENDEE_BY_ID;
    qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

    query4 = SELECT_ALARM_BY_ID;
    qsize4 = sizeof( SELECT_ALARM_BY_ID );

    query5 = SELECT_RECURSIVE_BY_ID;
    qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

    query6 = SELECT_RDATES_BY_ID;
    qsize6 = sizeof( SELECT_RDATES_BY_ID );

    count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                               query4, qsize4, query5, qsize5, query6, qsize6 );
  }
 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::load( const QDate &date )
{
  if ( !d->mIsOpened ) {
    return false;
  }

  if ( date.isValid() ) {
    return load( date, date.addDays( 1 ) );
  }

  return false;
}

bool SqliteStorage::load( const QDate &start, const QDate &end )
{
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  KDateTime loadStart;
  KDateTime loadEnd;

  d->mIsLoading = true;

  if ( getLoadDates( start, end, loadStart, loadEnd ) ) {
    const char *query1 = NULL;
    const char *query2 = NULL;
    const char *query3 = NULL;
    const char *query4 = NULL;
    const char *query5 = NULL;
    const char *query6 = NULL;
    int qsize1 = 0;
    int qsize2 = 0;
    int qsize3 = 0;
    int qsize4 = 0;
    int qsize5 = 0;
    int qsize6 = 0;

    sqlite3_stmt *stmt1 = NULL;
    const char *tail1 = NULL;
    int index = 1;
    qint64 secsStart;
    qint64 secsEnd;

    // Incidences to insert
    if ( loadStart.isValid() && loadEnd.isValid() ) {
      query1 = SELECT_COMPONENTS_BY_DATE_BOTH;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_DATE_BOTH );
      sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
      secsStart = toOriginTime( loadStart );
      secsEnd = toOriginTime( loadEnd );
      sqlite3_bind_int64( stmt1, index, secsEnd );
      sqlite3_bind_int64( stmt1, index, secsStart );
    } else if ( loadStart.isValid() ) {
      query1 = SELECT_COMPONENTS_BY_DATE_START;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_DATE_START );
      sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
      secsStart = toOriginTime( loadStart );
      sqlite3_bind_int64( stmt1, index, secsStart );
    } else if ( loadEnd.isValid() ) {
      query1 = SELECT_COMPONENTS_BY_DATE_END;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_DATE_END );
      sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
      secsEnd = toOriginTime( loadEnd );
      sqlite3_bind_int64( stmt1, index, secsEnd );
    } else {
      query1 = SELECT_COMPONENTS_ALL;
      qsize1 = sizeof( SELECT_COMPONENTS_ALL );
      sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
    }
    query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
    qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

    query3 = SELECT_ATTENDEE_BY_ID;
    qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

    query4 = SELECT_ALARM_BY_ID;
    qsize4 = sizeof( SELECT_ALARM_BY_ID );

    query5 = SELECT_RECURSIVE_BY_ID;
    qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

    query6 = SELECT_RDATES_BY_ID;
    qsize6 = sizeof( SELECT_RDATES_BY_ID );

    count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                               query4, qsize4, query5, qsize5, query6, qsize6 );

    if ( count > 0 ) {
      if ( loadStart.isValid() && loadEnd.isValid() ) {
        setLoadDates( loadStart.date(), loadEnd.date() );
      } else if ( loadStart.isValid() ) {
        setLoadDates( loadStart.date(), QDate( 9999, 12, 31 ) ); // 9999-12-31
      } else if ( loadEnd.isValid() ) {
        setLoadDates( QDate( 1, 1, 1 ), loadEnd.date() ); // 0001-01-01
      }
    }
  }
 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::loadNotebookIncidences( const QString &notebookUid )
{
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;
  QByteArray u;

  if ( !notebookUid.isEmpty() ) {
    query1 = SELECT_COMPONENTS_BY_NOTEBOOKUID;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_NOTEBOOKUID );

    sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
    u = notebookUid.toUtf8();
    sqlite3_bind_text( stmt1, index, u.constData(), u.length(), SQLITE_STATIC );

    query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
    qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

    query3 = SELECT_ATTENDEE_BY_ID;
    qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

    query4 = SELECT_ALARM_BY_ID;
    qsize4 = sizeof( SELECT_ALARM_BY_ID );

    query5 = SELECT_RECURSIVE_BY_ID;
    qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

    query6 = SELECT_RDATES_BY_ID;
    qsize6 = sizeof( SELECT_RDATES_BY_ID );

    count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                               query4, qsize4, query5, qsize5, query6, qsize6 );
  }
 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::loadJournals()
{
  d->mLoaded |= LOADED_LOAD_JOURNALS;

  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_BY_JOURNAL;
  qsize1 = sizeof( SELECT_COMPONENTS_BY_JOURNAL );

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::loadPlainIncidences()
{
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_BY_PLAIN;
  qsize1 = sizeof( SELECT_COMPONENTS_BY_PLAIN );

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::loadRecurringIncidences()
{
  d->mLoaded |= LOADED_LOAD_RECURRING_INCIDENCES;
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_BY_RECURSIVE;
  qsize1 = sizeof( SELECT_COMPONENTS_BY_RECURSIVE );

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::loadGeoIncidences()
{
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_BY_GEO;
  qsize1 = sizeof(SELECT_COMPONENTS_BY_GEO);

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::loadGeoIncidences( float geoLatitude, float geoLongitude,
                                       float diffLatitude, float diffLongitude )
{
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;

  query1 = SELECT_COMPONENTS_BY_GEO_AREA;
  qsize1 = sizeof( SELECT_COMPONENTS_BY_GEO_AREA );

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
  sqlite3_bind_int64( stmt1, index, geoLatitude - diffLatitude );
  sqlite3_bind_int64( stmt1, index, geoLongitude - diffLongitude );
  sqlite3_bind_int64( stmt1, index, geoLatitude + diffLatitude );
  sqlite3_bind_int64( stmt1, index, geoLongitude + diffLongitude );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

 error:
  d->mIsLoading = false;

  return count >= 0;
}

bool SqliteStorage::loadAttendeeIncidences()
{
  if ( !d->mIsOpened ) {
    return false;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_BY_ATTENDEE;
  qsize1 = sizeof(SELECT_COMPONENTS_BY_ATTENDEE);

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

 error:
  d->mIsLoading = false;

  return count >= 0;
}

int SqliteStorage::loadUncompletedTodos()
{
  if ( !d->mIsOpened ) {
    return -1;
  }

  if ( isUncompletedTodosLoaded() ) {
    return 0;
  }

  int rv = 0;
  int count = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_BY_UNCOMPLETED_TODOS;
  qsize1 = sizeof( SELECT_COMPONENTS_BY_UNCOMPLETED_TODOS );

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

  setIsUncompletedTodosLoaded( count >= 0 );

 error:
  d->mIsLoading = false;

  return count;
}

int SqliteStorage::loadCompletedTodos( bool hasDate, int limit, KDateTime *last )
{
  if ( !d->mIsOpened || !last ) {
    return -1;
  }

  if ( hasDate ) {
    if ( isCompletedTodosDateLoaded() ) {
      return 0;
    }
  } else {
    if ( isCompletedTodosCreatedLoaded() ) {
      return 0;
    }
  }
  int rv = 0;
  int count = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;
  qint64 secsStart;

  if ( last->isValid() ) {
    secsStart = toOriginTime( *last );
  } else {
    secsStart = LLONG_MAX; // largest time
  }

  if (hasDate) {
    query1 = SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_DATE;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_DATE );
  } else {
    query1 = SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_CREATED;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_CREATED );
  }
  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
  sqlite3_bind_int64( stmt1, index, secsStart );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6,
                             limit, last, hasDate );

  if ( count >= 0 && count < limit ) {
    if ( hasDate ) {
      setIsCompletedTodosDateLoaded( true );
    } else {
      setIsCompletedTodosCreatedLoaded( true );
    }
  }

 error:
  d->mIsLoading = false;

  return count;
}

int SqliteStorage::loadIncidences( bool hasDate, int limit, KDateTime *last )
{
  if ( !d->mIsOpened || !last ) {
    return -1;
  }

  if ( hasDate ) {
    if ( isDateLoaded() ) {
      return 0;
    }
  } else {
    if ( isCreatedLoaded() ) {
      return 0;
    }
  }
  int rv = 0;
  int count = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;
  qint64 secsStart;

  if ( last->isValid() ) {
    secsStart = toOriginTime( *last );
  } else {
    secsStart = LLONG_MAX; // largest time
  }
  if ( hasDate ) {
    query1 = SELECT_COMPONENTS_BY_DATE_SMART;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_DATE_SMART );
  } else {
    query1 = SELECT_COMPONENTS_BY_CREATED_SMART;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_CREATED_SMART );
  }
  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
  sqlite3_bind_int64( stmt1, index, secsStart );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6,
                             limit, last, hasDate );

  if ( count >= 0 && count < limit ) {
    if ( hasDate ) {
      setIsDateLoaded( true );
    } else {
      setIsCreatedLoaded( true );
    }
  }

 error:
  d->mIsLoading = false;

  return count;
}

int SqliteStorage::loadGeoIncidences( bool hasDate, int limit, KDateTime *last )
{
  if ( !d->mIsOpened || !last ) {
    return -1;
  }

  if ( hasDate ) {
    if ( isGeoDateLoaded() ) {
      return 0;
    }
  } else {
    if ( isGeoCreatedLoaded() ) {
      return 0;
    }
  }
  int rv = 0;
  int count = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;
  qint64 secsStart;

  if ( last->isValid() ) {
    secsStart = toOriginTime( *last );
  } else {
    secsStart = LLONG_MAX; // largest time
  }
  if ( hasDate ) {
    query1 = SELECT_COMPONENTS_BY_GEO_AND_DATE;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_GEO_AND_DATE );
  } else {
    query1 = SELECT_COMPONENTS_BY_GEO_AND_CREATED;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_GEO_AND_CREATED );
  }
  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
  sqlite3_bind_int64( stmt1, index, secsStart );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6,
                             limit, last, hasDate );

  if ( count >= 0 && count < limit ) {
    if ( hasDate ) {
      setIsGeoDateLoaded( true );
    } else {
      setIsGeoCreatedLoaded(true);
    }
  }

 error:
  d->mIsLoading = false;

  return count;
}

int SqliteStorage::loadUnreadInvitationIncidences()
{
  if ( !d->mIsOpened ) {
    return false;
  }

  if ( isUnreadIncidencesLoaded() ) {
    return 0;
  }

  int rv = 0;
  int count = -1;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_COMPONENTS_BY_INVITATION_UNREAD;
  qsize1 = sizeof( SELECT_COMPONENTS_BY_INVITATION_UNREAD );

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6 );

  setIsUnreadIncidencesLoaded( count >= 0 );

 error:
  d->mIsLoading = false;

  return count;
}

int SqliteStorage::loadOldInvitationIncidences( int limit, KDateTime *last )
{
  if ( !d->mIsOpened || !last ) {
    return -1;
  }

  if ( isInvitationIncidencesLoaded() ) {
    return 0;
  }

  int rv = 0;
  int count = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;
  qint64 secsStart;

  query1 = SELECT_COMPONENTS_BY_INVITATION_AND_CREATED;
  qsize1 = sizeof( SELECT_COMPONENTS_BY_INVITATION_AND_CREATED );
  if ( last->isValid() ) {
    secsStart = toOriginTime( *last );
  } else {
    secsStart = LLONG_MAX; // largest time
  }
  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
  sqlite3_bind_int64( stmt1, index, secsStart );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6,
                             limit, last, false );

  if ( count >= 0 && count < limit ) {
    setIsInvitationIncidencesLoaded( true );
  }

 error:
  d->mIsLoading = false;

  return count;
}

Person::List SqliteStorage::loadContacts()
{
  Person::List list;

  if ( !d->mIsOpened ) {
    return list;
  }

  int rv = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  int qsize1 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  query1 = SELECT_ATTENDEE_AND_COUNT;
  qsize1 = sizeof(SELECT_ATTENDEE_AND_COUNT);

  sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );

  list = d->mFormat->selectContacts( stmt1 );

 error:
  d->mIsLoading = false;

  return list;
}

int SqliteStorage::loadContactIncidences( const Person::Ptr &person, int limit, KDateTime *last )
{
  if ( !d->mIsOpened || !last ) {
    return -1;
  }

  int rv = 0;
  int count = 0;
  d->mIsLoading = true;

  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;

  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;
  int index = 1;
  qint64 secsStart = 0;
  QByteArray email;

  if ( !person->isEmpty() ) {
    email = person->email().toUtf8();
    query1 = SELECT_COMPONENTS_BY_ATTENDEE_EMAIL_AND_CREATED;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_ATTENDEE_EMAIL_AND_CREATED );
    sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
    sqlite3_bind_text( stmt1, index, email, email.length(), SQLITE_STATIC );
  } else {
    query1 = SELECT_COMPONENTS_BY_ATTENDEE_AND_CREATED;
    qsize1 = sizeof( SELECT_COMPONENTS_BY_ATTENDEE_AND_CREATED );
    sqlite3_prepare_v2( d->mDatabase, query1, qsize1, &stmt1, &tail1 );
  }
  if ( last->isValid() ) {
    secsStart = toOriginTime( *last );
  } else {
    secsStart = LLONG_MAX; // largest time
  }
  sqlite3_bind_int64( stmt1, index, secsStart );

  query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
  qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

  query3 = SELECT_ATTENDEE_BY_ID;
  qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

  query4 = SELECT_ALARM_BY_ID;
  qsize4 = sizeof( SELECT_ALARM_BY_ID );

  query5 = SELECT_RECURSIVE_BY_ID;
  qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

  query6 = SELECT_RDATES_BY_ID;
  qsize6 = sizeof( SELECT_RDATES_BY_ID );

  count = d->loadIncidences( stmt1, query2, qsize2, query3, qsize3,
                             query4, qsize4, query5, qsize5, query6, qsize6,
                             limit, last, false );

 error:
  d->mIsLoading = false;

  return count;
}

bool SqliteStorage::notifyOpened( const Incidence::Ptr &incidence )
{
  if ( incidence && d->mUseTracker ) {
    return d->notifyOpened( incidence );
  } else {
    return false;
  }
}

int SqliteStorage::Private::loadIncidences( sqlite3_stmt *stmt1,
                                            const char *query2, int qsize2,
                                            const char *query3, int qsize3,
                                            const char *query4, int qsize4,
                                            const char *query5, int qsize5,
                                            const char *query6, int qsize6,
                                            int limit, KDateTime *last, bool useDate ) {
  int rv = 0;
  int count = 0;
  sqlite3_stmt *stmt2 = NULL;
  sqlite3_stmt *stmt3 = NULL;
  sqlite3_stmt *stmt4 = NULL;
  sqlite3_stmt *stmt5 = NULL;
  sqlite3_stmt *stmt6 = NULL;
  const char *tail2 = NULL;
  const char *tail3 = NULL;
  const char *tail4 = NULL;
  const char *tail5 = NULL;
  const char *tail6 = NULL;
  Incidence::Ptr incidence;
  KDateTime previous;
  QString notebookUid;

  if ( !mSem.acquire() ) {
    kError() << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
    return false;
  }

  sqlite3_prepare_v2( mDatabase, query2, qsize2, &stmt2, &tail2 );
  sqlite3_prepare_v2( mDatabase, query3, qsize3, &stmt3, &tail3 );
  sqlite3_prepare_v2( mDatabase, query4, qsize4, &stmt4, &tail4 );
  sqlite3_prepare_v2( mDatabase, query5, qsize5, &stmt5, &tail5 );
  sqlite3_prepare_v2( mDatabase, query6, qsize6, &stmt6, &tail6 );

  while ( ( incidence =
            mFormat->selectComponents( stmt1, stmt2, stmt3, stmt4, stmt5, stmt6, notebookUid ) ) ) {
    bool hasNotebook = mCalendar->hasValidNotebook( notebookUid );
    if ( mIncidencesToInsert.contains( incidence->uid(), incidence ) ||
         mIncidencesToUpdate.contains( incidence->uid(), incidence ) ||
         mIncidencesToDelete.contains( incidence->uid(), incidence ) ||
         ( mStorage->validateNotebooks() && !hasNotebook ) ) {
      kWarning() << "not loading" << incidence->uid() << notebookUid
                 << ( !hasNotebook ? "(invalidated notebook)" : "(local changes)" );
    } else {
      if ( incidence->type() == Incidence::TypeEvent ) {
        Event::Ptr event = incidence.staticCast<Event>();
        Event::Ptr old;
        if ( !event->hasRecurrenceId() ) {
          old = mCalendar->event( event->uid() );
        } else {
          old = mCalendar->event( event->uid(), event->recurrenceId() );
        }
        if ( old ) {
          if ( event->revision() > old->revision() ) {
            kDebug() << "updating event" << event->uid()
                     << event->dtStart() << event->dtEnd()
                     << "in calendar";
            mCalendar->deleteEvent( old ); // move old to deleted
            mCalendar->addEvent( event ); // and replace it with this one
            mCalendar->setNotebook( event, notebookUid );
          } else {
            event = old;
          }
        } else {
          kDebug() << "adding event" << event->uid()
                   << event->dtStart() << event->dtEnd()
                   << "in calendar";
          mCalendar->addEvent( event );
          mCalendar->setNotebook( event, notebookUid );
        }
        if ( event != old ) {
          count++; // added into calendar
        }
        if ( useDate && event->dtEnd().isValid() ) {
          previous = event->dtEnd();
        } else if ( useDate && event->dtStart().isValid() ) {
          previous = event->dtStart();
        } else {
          previous = event->created();
        }
      } else if ( incidence->type() == Incidence::TypeTodo ) {
        Todo::Ptr todo = incidence.staticCast<Todo>();
        Todo::Ptr old;
        if ( !todo->hasRecurrenceId() ) {
          old = mCalendar->todo( todo->uid() );
        } else {
          old = mCalendar->todo( todo->uid(), todo->recurrenceId() );
        }
        if ( old ) {
          if ( todo->revision() > old->revision() ) {
            kDebug() << "updating todo" << todo->uid()
                     << todo->dtDue() << todo->created()
                     << "in calendar";
            mCalendar->deleteTodo( old ); // move old to deleted
            mCalendar->addTodo( todo ); // and replace it with this one
            mCalendar->setNotebook( todo, notebookUid );
          } else {
            todo = old;
          }
        } else {
          kDebug() << "adding todo" << todo->uid()
                   << todo->dtDue() << todo->created()
                   << "in calendar";
          mCalendar->addTodo( todo );
          mCalendar->setNotebook( todo, notebookUid );
        }
        if ( todo != old ) {
          count++; // added into calendar
        }
        if ( useDate && todo->dtDue().isValid() ) {
          previous = todo->dtDue();
        } else if ( useDate && todo->dtStart().isValid() ) {
            previous = todo->dtStart();
        } else {
          previous = todo->created();
        }
      } else if ( incidence->type() == Incidence::TypeJournal ) {
        Journal::Ptr journal = incidence.staticCast<Journal>();
        Journal::Ptr old;
        if ( !journal->hasRecurrenceId() ) {
          old = mCalendar->journal( journal->uid() );
        } else {
          old = mCalendar->journal( journal->uid(), journal->recurrenceId() );
        }
        if ( old ) {
          if ( journal->revision() > old->revision() ) {
            kDebug() << "updating journal" << journal->uid()
                     << journal->dtStart() << journal->created()
                     << "in calendar";
            mCalendar->deleteJournal( old ); // move old to deleted
            mCalendar->addJournal( journal ); // and replace it with this one
            mCalendar->setNotebook( journal, notebookUid );
          } else {
            journal = old;
          }
        } else {
          kDebug() << "adding journal" << journal->uid()
                   << journal->dtStart() << journal->created()
                   << "in calendar";
          mCalendar->addJournal( journal );
          mCalendar->setNotebook( journal, notebookUid );
        }
        if ( journal != old ) {
          count++; // added into calendar
        }

        if ( useDate && journal->dateTime( Incidence::RoleEnd ).isValid() ) {
          // TODO_ALVARO: journals don't have dtEnd, bug ?
          previous = journal->dateTime( Incidence::RoleEnd );
        } else if ( useDate && journal->dtStart().isValid() ) {
          previous = journal->dtStart();
        } else {
          previous = journal->created();
        }
      }
    }
    sqlite3_reset( stmt2 );
    sqlite3_reset( stmt3 );
    sqlite3_reset( stmt4 );
    sqlite3_reset( stmt5 );
    sqlite3_reset( stmt6 );

    if ( count == limit ) {
      // Stop when we have loaded requested number of new incidences.
      break;
    }
  }
  if ( last ) {
    *last = previous;
  }

  sqlite3_reset( stmt1 );
  sqlite3_finalize( stmt1 );
  sqlite3_finalize( stmt2 );
  sqlite3_finalize( stmt3 );
  sqlite3_finalize( stmt4 );
  sqlite3_finalize( stmt5 );
  sqlite3_finalize( stmt6 );

  if ( !mSem.release() ) {
    kError() << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
  }
  mStorage->setFinished( false, "load completed" );

  return count;

 error:
  if ( !mSem.release() ) {
    kError() << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
  }
  mStorage->setFinished( true, "error loading incidences" );

  return -1;
}
//@endcond

bool SqliteStorage::save()
{
  if ( !d->mIsOpened ) {
    return false;
  }

  if ( !d->mSem.acquire() ) {
    kError() << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    return false;
  }

  if ( !d->saveTimezones() ) {
    kError() << "saving timezones failed";
  }

  int errors = 0;
  const char *query1 = NULL;
  const char *query2 = NULL;
  const char *query3 = NULL;
  const char *query4 = NULL;
  const char *query5 = NULL;
  const char *query6 = NULL;
  const char *query7 = NULL;
  const char *query8 = NULL;
  const char *query9 = NULL;
  const char *query10 = NULL;
  const char *query11 = NULL;

  int qsize1 = 0;
  int qsize2 = 0;
  int qsize3 = 0;
  int qsize4 = 0;
  int qsize5 = 0;
  int qsize6 = 0;
  int qsize7 = 0;
  int qsize8 = 0;
  int qsize9 = 0;
  int qsize10 = 0;
  int qsize11 = 0;

  // Incidences to insert
  if ( !d->mIncidencesToInsert.isEmpty() ) {
    query1 = INSERT_COMPONENTS;
    qsize1 = sizeof( INSERT_COMPONENTS );
    query2 = INSERT_CUSTOMPROPERTIES;
    qsize2 = sizeof( INSERT_CUSTOMPROPERTIES );
    query3 = INSERT_CUSTOMPROPERTIES;
    qsize3 = sizeof( INSERT_CUSTOMPROPERTIES );
    query4 = INSERT_ATTENDEE;
    qsize4 = sizeof( INSERT_ATTENDEE );
    query5 = INSERT_ATTENDEE;
    qsize5 = sizeof( INSERT_ATTENDEE );
    query6 = INSERT_ALARM;
    qsize6 = sizeof( INSERT_ALARM );
    query7 = INSERT_ALARM;
    qsize7 = sizeof( INSERT_ALARM );
    query8 = INSERT_RECURSIVE;
    qsize8 = sizeof( INSERT_RECURSIVE );
    query9 = INSERT_RECURSIVE;
    qsize9 = sizeof( INSERT_RECURSIVE );
    query10 = INSERT_RDATES;
    qsize10 = sizeof( INSERT_RDATES );
    query11 = INSERT_RDATES;
    qsize11 = sizeof( INSERT_RDATES );

    if ( !d->saveIncidences( d->mIncidencesToInsert, DBInsert,
                             query1, qsize1, query2, qsize2, query3, qsize3, query4, qsize4,
                             query5, qsize5, query6, qsize6, query7, qsize7, query8, qsize8,
                             query9, qsize9, query10, qsize10, query11, qsize11 ) ) {
      errors++;
    }
  }

  // Incidences to update
  if ( !d->mIncidencesToUpdate.isEmpty() ) {
    query1 = UPDATE_COMPONENTS;
    qsize1 = sizeof( UPDATE_COMPONENTS );
    query2 = DELETE_CUSTOMPROPERTIES;
    qsize2 = sizeof( DELETE_CUSTOMPROPERTIES );
    query3 = INSERT_CUSTOMPROPERTIES;
    qsize3 = sizeof( INSERT_CUSTOMPROPERTIES );
    query4 = DELETE_ATTENDEE;
    qsize4 = sizeof( DELETE_ATTENDEE );
    query5 = INSERT_ATTENDEE;
    qsize5 = sizeof( INSERT_ATTENDEE );
    query6 = DELETE_ALARM;
    qsize6 = sizeof( DELETE_ALARM );
    query7 = INSERT_ALARM;
    qsize7 = sizeof( INSERT_ALARM );
    query8 = DELETE_RECURSIVE;
    qsize8 = sizeof( DELETE_RECURSIVE );
    query9 = INSERT_RECURSIVE;
    qsize9 = sizeof( INSERT_RECURSIVE );
    query10 = DELETE_RDATES;
    qsize10 = sizeof( DELETE_RDATES );
    query11 = INSERT_RDATES;
    qsize11 = sizeof( INSERT_RDATES );

    if ( !d->saveIncidences( d->mIncidencesToUpdate, DBUpdate,
                             query1, qsize1, query2, qsize2, query3, qsize3, query4, qsize4,
                             query5, qsize5, query6, qsize6, query7, qsize7, query8, qsize8,
                             query9, qsize9, query10, qsize10, query11, qsize11 ) ) {
      errors++;
    }
  }

  // Incidences to delete
  if ( !d->mIncidencesToDelete.isEmpty() ) {
    query1 = DELETE_COMPONENTS;
    qsize1 = sizeof( DELETE_COMPONENTS );
    query2 = DELETE_CUSTOMPROPERTIES;
    qsize2 = sizeof( DELETE_CUSTOMPROPERTIES );
    query4 = DELETE_ATTENDEE;
    qsize4 = sizeof( DELETE_ATTENDEE );
    query6 = DELETE_ALARM;
    qsize6 = sizeof( DELETE_ALARM );
    query8 = DELETE_RECURSIVE;
    qsize8 = sizeof( DELETE_RECURSIVE );
    query10 = DELETE_RDATES;
    qsize10 = sizeof( DELETE_RDATES );

    if ( !d->saveIncidences( d->mIncidencesToDelete, DBDelete,
                             query1, qsize1, query2, qsize2, query3, qsize3, query4, qsize4,
                             query5, qsize5, query6, qsize6, query7, qsize7, query8, qsize8,
                             query9, qsize9, query10, qsize10, query11, qsize11 ) ) {
      errors++;
    }
  }

  if ( !d->mSem.release() ) {
    kError() << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
  }
  d->mChanged.resize( 0 ); // make a change to create signal

  if ( errors == 0 ) {
    setFinished( false, "save completed" );
  } else {
    setFinished( true, "errors saving incidences" );
  }

  return errors == 0;
}

//@cond PRIVATE
bool SqliteStorage::Private::saveIncidences( QHash<QString, Incidence::Ptr> &list,
                                             DBOperation dbop,
                                             const char *query1, int qsize1,
                                             const char *query2, int qsize2,
                                             const char *query3, int qsize3,
                                             const char *query4, int qsize4,
                                             const char *query5, int qsize5,
                                             const char *query6, int qsize6,
                                             const char *query7, int qsize7,
                                             const char *query8, int qsize8,
                                             const char *query9, int qsize9,
                                             const char *query10, int qsize10,
                                             const char *query11, int qsize11 )
{
  int rv = 0;
  int errors = 0;
  sqlite3_stmt *stmt1 = NULL;
  sqlite3_stmt *stmt2 = NULL;
  sqlite3_stmt *stmt3 = NULL;
  sqlite3_stmt *stmt4 = NULL;
  sqlite3_stmt *stmt5 = NULL;
  sqlite3_stmt *stmt6 = NULL;
  sqlite3_stmt *stmt7 = NULL;
  sqlite3_stmt *stmt8 = NULL;
  sqlite3_stmt *stmt9 = NULL;
  sqlite3_stmt *stmt10 = NULL;
  sqlite3_stmt *stmt11 = NULL;
  const char *tail1 = NULL;
  const char *tail2 = NULL;
  const char *tail3 = NULL;
  const char *tail4 = NULL;
  const char *tail5 = NULL;
  const char *tail6 = NULL;
  const char *tail7 = NULL;
  const char *tail8 = NULL;
  const char *tail9 = NULL;
  const char *tail10 = NULL;
  const char *tail11 = NULL;
  const char *operation = ( dbop == DBInsert ) ? "inserting" :
                          ( dbop == DBUpdate ) ? "updating" : "deleting";
  QHash<QString,Incidence::Ptr>::const_iterator it;
  char *errmsg = NULL;
  const char *query = NULL;

  query = BEGIN_TRANSACTION;
  sqlite3_exec( mDatabase );

  sqlite3_prepare_v2( mDatabase, query1, qsize1, &stmt1, &tail1 );
  sqlite3_prepare_v2( mDatabase, query2, qsize2, &stmt2, &tail2 );
  if ( query3 ) {
    sqlite3_prepare_v2( mDatabase, query3, qsize3, &stmt3, &tail3 );
  }
  sqlite3_prepare_v2( mDatabase, query4, qsize4, &stmt4, &tail4 );
  if ( query5 ) {
    sqlite3_prepare_v2( mDatabase, query5, qsize5, &stmt5, &tail5 );
  }
  sqlite3_prepare_v2( mDatabase, query6, qsize6, &stmt6, &tail6 );
  if ( query7 ) {
    sqlite3_prepare_v2( mDatabase, query7, qsize7, &stmt7, &tail7 );
  }
  sqlite3_prepare_v2( mDatabase, query8, qsize8, &stmt8, &tail8 );
  if ( query9 ) {
    sqlite3_prepare_v2( mDatabase, query9, qsize9, &stmt9, &tail9 );
  }
  sqlite3_prepare_v2( mDatabase, query10, qsize10, &stmt10, &tail10 );
  if ( query11 ) {
    sqlite3_prepare_v2( mDatabase, query11, qsize11, &stmt11, &tail11 );
  }

  for ( it = list.constBegin(); it != list.constEnd(); ++it ) {
    QString notebookUid = mCalendar->notebook( *it );
    if ( !mStorage->isValidNotebook( notebookUid ) ) {
      kDebug() << "invalid notebook - not saving incidence" << (*it)->uid();
      continue;
    }

    (*it)->setLastModified( KDateTime::currentUtcDateTime() );
    kDebug() << operation << "incidence" << (*it)->uid() << "notebook" << notebookUid;
    if ( !mFormat->modifyComponents( *it, notebookUid, dbop, stmt1, stmt2, stmt3, stmt4,
                                     stmt5, stmt6, stmt7, stmt8, stmt9, stmt10, stmt11 ) ) {
      kError() << sqlite3_errmsg( mDatabase ) << "for incidence" << (*it)->uid();
      errors++;
    }
    if ( mUseTracker ) {
      // Also save into tracker.
      modifyTracker( *it, dbop, notebookUid );
    }
    if ( dbop == DBDelete ) {
      // Remove all alarms.
      mStorage->clearAlarms( *it );
    } else {
      // Reset all alarms.
      mStorage->resetAlarms( *it );
    }
    sqlite3_reset( stmt1 );
    sqlite3_reset( stmt2 );
    if ( stmt3 ) {
      sqlite3_reset( stmt3 );
    }
    sqlite3_reset( stmt4 );
    if ( stmt5 ) {
      sqlite3_reset( stmt5 );
    }
    sqlite3_reset( stmt6 );
    if ( stmt7 ) {
      sqlite3_reset( stmt7 );
    }
    sqlite3_reset( stmt8 );
    if ( stmt9 ) {
      sqlite3_reset( stmt9 );
    }
    sqlite3_reset( stmt10 );
    if ( stmt11 ) {
      sqlite3_reset( stmt11 );
    }
  }
  list.clear();
  // TODO What if there were errors? Options: 1) rollback 2) best effort.

  sqlite3_finalize( stmt1 );
  sqlite3_finalize( stmt2 );
  if ( stmt3 ) {
    sqlite3_finalize( stmt3 );
  }
  sqlite3_finalize( stmt4 );
  if ( stmt5 ) {
    sqlite3_finalize( stmt5 );
  }
  sqlite3_finalize( stmt6 );
  if ( stmt7 ) {
    sqlite3_finalize( stmt7 );
  }
  sqlite3_finalize( stmt8 );
  if ( stmt9 ) {
    sqlite3_finalize( stmt9 );
  }
  sqlite3_finalize( stmt10 );
  if ( stmt11 ) {
    sqlite3_finalize( stmt11 );
  }

  query = COMMIT_TRANSACTION;
  sqlite3_exec( mDatabase );

  return errors == 0;

 error:
  return false;
}
//@endcond

bool SqliteStorage::cancel()
{
  return true;
}

bool SqliteStorage::close()
{
  if ( d->mIsOpened ) {
    if ( d->mWatcher ) {
      d->mWatcher->removePaths( d->mWatcher->files() );
      // This should work, as storage should be closed before
      // application terminates now. If not, deadlock occurs.
      delete d->mWatcher;
      d->mWatcher = NULL;
    }
    d->mChanged.close();
    sqlite3_close( d->mDatabase );
    d->mDatabase = 0;
    if ( d->mFormat ) {
      delete d->mFormat;
      d->mFormat = 0;
    }
    if ( d->mDBusIf ) {
      delete d->mDBusIf;
      d->mDBusIf = 0;
    }
    d->mIsOpened = false;
  }
  return true;
}

void SqliteStorage::calendarModified( bool modified, Calendar *calendar )
{
  Q_UNUSED( calendar );
  kDebug() << "calendarModified called:" << modified;
}

void SqliteStorage::calendarIncidenceAdded( const Incidence::Ptr &incidence )
{
  if ( !d->mIncidencesToInsert.contains( incidence->uid(), incidence ) && !d->mIsLoading ) {

    QString uid = incidence->uid();

    if ( uid.length() < 7 ) { // We force a minimum length of uid to grant uniqness
#if defined(HAVE_UUID_UUID_H)
      uuid_t uuid;
      char suuid[64];
      uuid_generate_random( uuid );
      uuid_unparse( uuid, suuid );
      kDebug() << "changing" << uid << "to" << suuid;
      uid = QString( suuid );
      incidence->setUid( uid );
#else
//KDAB_TODO:
#ifdef __GNUC__
#warning no uuid support. what to do now?
#endif
#endif
    }

    if ( d->mUidMappings.contains( uid ) ) {
      incidence->setUid( d->mUidMappings.value( incidence->uid() ) );
      kDebug() << "mapping" << uid << "to" << incidence->uid();
    }

    kDebug() << "appending incidence" << incidence->uid() << "for database insert";
    d->mIncidencesToInsert.insert( incidence->uid(), incidence );
//    if ( !uid.isEmpty() ) {
//      d->mUidMappings.insert( uid, incidence->uid() );
//    }
  }
}

void SqliteStorage::calendarIncidenceChanged( const Incidence::Ptr &incidence )
{
  if ( !d->mIncidencesToUpdate.contains( incidence->uid(), incidence ) &&
       !d->mIncidencesToInsert.contains( incidence->uid(), incidence ) &&
       !d->mIsLoading ) {
    kDebug() << "appending incidence" << incidence->uid() << "for database update";
    d->mIncidencesToUpdate.insert( incidence->uid(), incidence );
    d->mUidMappings.insert( incidence->uid(), incidence->uid() );
  }
}

void SqliteStorage::calendarIncidenceDeleted( const Incidence::Ptr &incidence )
{
  if (!d->mIncidencesToDelete.contains( incidence->uid(), incidence ) && !d->mIsLoading ) {
    kDebug() << "appending incidence" << incidence->uid() << "for database delete";
    d->mIncidencesToDelete.insert( incidence->uid(), incidence );
  }
}

void SqliteStorage::calendarIncidenceAdditionCanceled( const Incidence::Ptr &incidence )
{
  if ( d->mIncidencesToInsert.contains( incidence->uid(), incidence ) && !d->mIsLoading ) {
    kDebug() << "duplicate - removing incidence from inserted" << incidence->uid();
    d->mIncidencesToInsert.remove( incidence->uid(), incidence );
  }
}

//@cond PRIVATE
bool SqliteStorage::Private::selectIncidences( Incidence::List *list,
                                               const char *query1, int qsize1,
                                               const char *query2, int qsize2,
                                               const char *query3, int qsize3,
                                               const char *query4, int qsize4,
                                               const char *query5, int qsize5,
                                               const char *query6, int qsize6,
                                               DBOperation dbop, const KDateTime &after,
                                               const QString &notebookUid, const QString &summary )
{
  int rv = 0;
  sqlite3_stmt *stmt1 = NULL;
  sqlite3_stmt *stmt2 = NULL;
  sqlite3_stmt *stmt3 = NULL;
  sqlite3_stmt *stmt4 = NULL;
  sqlite3_stmt *stmt5 = NULL;
  sqlite3_stmt *stmt6 = NULL;
  const char *tail1 = NULL;
  const char *tail2 = NULL;
  const char *tail3 = NULL;
  const char *tail4 = NULL;
  const char *tail5 = NULL;
  const char *tail6 = NULL;
  int index;
  QByteArray n;
  QByteArray s;
  Incidence::Ptr incidence;
  sqlite3_int64 secs;
  QString nbook;

  if ( !mSem.acquire() ) {
    kError() << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
    return false;
  }

  sqlite3_prepare_v2( mDatabase, query1, qsize1, &stmt1, &tail1 );

  kDebug() << "incidences"
           << ( dbop == DBInsert ? "inserted" :
                dbop == DBUpdate ? "updated" :
                dbop == DBDelete ? "deleted" : "" )
           << "since" << after.toString();

  if ( query1 ) {
    if ( after.isValid() ) {
      if ( dbop == DBInsert ) {
        index = 1;
        secs = mStorage->toOriginTime( after );
        sqlite3_bind_int64( stmt1, index, secs );
        if ( !notebookUid.isNull() ) {
          index = 2;
          n = notebookUid.toUtf8();
          sqlite3_bind_text( stmt1, index, n.constData(), n.length(), SQLITE_STATIC );
        }
      }
      if ( dbop == DBUpdate || dbop == DBDelete ) {
        index = 1;
        secs = mStorage->toOriginTime( after );
        sqlite3_bind_int64( stmt1, index, secs );
        index = 2;
        sqlite3_bind_int64( stmt1, index, secs );
        if ( !notebookUid.isNull() ) {
          index = 3;
          n = notebookUid.toUtf8();
          sqlite3_bind_text( stmt1, index, n.constData(), n.length(), SQLITE_STATIC );
        }
      }
      if ( dbop == DBSelect ) {
        index = 1;
        secs = mStorage->toOriginTime( after );
        qDebug() << "QUERY FROM" << secs;
        sqlite3_bind_int64( stmt1, index, secs );
        index = 2;
        s = summary.toUtf8();
        sqlite3_bind_text( stmt1, index, s.constData(), s.length(), SQLITE_STATIC );
        if ( !notebookUid.isNull() ) {
          kDebug() << "notebook" << notebookUid.toUtf8().constData();
          index = 3;
          n = notebookUid.toUtf8();
          sqlite3_bind_text( stmt1, index, n.constData(), n.length(), SQLITE_STATIC );
        }
      }
    } else {
      if ( !notebookUid.isNull() ) {
        index = 1;
        n = notebookUid.toUtf8();
        sqlite3_bind_text( stmt1, index, n.constData(), n.length(), SQLITE_STATIC );
      }
    }
  }
  if ( query2 ) {
    sqlite3_prepare_v2( mDatabase, query2, qsize2, &stmt2, &tail2 );
  }
  if ( query3 ) {
    sqlite3_prepare_v2( mDatabase, query3, qsize3, &stmt3, &tail3 );
  }
  if ( query4 ) {
    sqlite3_prepare_v2( mDatabase, query4, qsize4, &stmt4, &tail4 );
  }
  if ( query5 ) {
    sqlite3_prepare_v2( mDatabase, query5, qsize5, &stmt5, &tail5 );
  }
  if ( query6 ) {
    sqlite3_prepare_v2( mDatabase, query6, qsize6, &stmt6, &tail6 );
  }

  while ( ( incidence =
            mFormat->selectComponents( stmt1, stmt2, stmt3, stmt4, stmt5, stmt6, nbook ) ) ) {
    kDebug() << "adding incidence" << incidence->uid() << "into list"
             << incidence->created() << incidence->lastModified();
    list->append( incidence );
    if ( stmt2 ) {
      sqlite3_reset( stmt2 );
    }
    if ( stmt3 ) {
      sqlite3_reset( stmt3 );
    }
    if ( stmt4 ) {
      sqlite3_reset( stmt4 );
    }
    if ( stmt5 ) {
      sqlite3_reset( stmt5 );
    }
    if ( stmt6 ) {
      sqlite3_reset( stmt6 );
    }
  }
  sqlite3_reset( stmt1 );
  sqlite3_finalize( stmt1 );
  if ( stmt2 ) {
    sqlite3_finalize( stmt2 );
  }
  if ( stmt3 ) {
    sqlite3_finalize( stmt3 );
  }
  if ( stmt4 ) {
    sqlite3_finalize( stmt4 );
  }
  if ( stmt5 ) {
    sqlite3_finalize( stmt5 );
  }
  if ( stmt6 ) {
    sqlite3_finalize( stmt6 );
  }

  if ( !mSem.release() ) {
    kError() << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
  }
  mStorage->setFinished( false, "select completed" );
  return true;

 error:
  if ( !mSem.release() ) {
    kError() << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
  }
  mStorage->setFinished( true, "error selecting incidences" );
  return false;
}
//@endcond

bool SqliteStorage::insertedIncidences( Incidence::List *list, const KDateTime &after,
                                        const QString &notebookUid )
{
  if ( d->mIsOpened && list && after.isValid() ) {
    const char *query1 = NULL;
    const char *query2 = NULL;
    const char *query3 = NULL;
    const char *query4 = NULL;
    const char *query5 = NULL;
    const char *query6 = NULL;
    int qsize1 = 0;
    int qsize2 = 0;
    int qsize3 = 0;
    int qsize4 = 0;
    int qsize5 = 0;
    int qsize6 = 0;

    if ( !notebookUid.isNull() ) {
      query1 = SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK );
    } else {
      query1 = SELECT_COMPONENTS_BY_CREATED;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_CREATED );
    }

    query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
    qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

    query3 = SELECT_ATTENDEE_BY_ID;
    qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

    query4 = SELECT_ALARM_BY_ID;
    qsize4 = sizeof( SELECT_ALARM_BY_ID );

    query5 = SELECT_RECURSIVE_BY_ID;
    qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

    query6 = SELECT_RDATES_BY_ID;
    qsize6 = sizeof( SELECT_RDATES_BY_ID );

    return d->selectIncidences( list, query1, qsize1, query2, qsize2, query3, qsize3,
                                query4, qsize4, query5, qsize5, query6, qsize6,
                                DBInsert, after, notebookUid );
  }
  return false;
}

bool SqliteStorage::modifiedIncidences( Incidence::List *list, const KDateTime &after,
                                        const QString &notebookUid )
{
  if ( d->mIsOpened && list && after.isValid() ) {
    const char *query1 = NULL;
    const char *query2 = NULL;
    const char *query3 = NULL;
    const char *query4 = NULL;
    const char *query5 = NULL;
    const char *query6 = NULL;
    int qsize1 = 0;
    int qsize2 = 0;
    int qsize3 = 0;
    int qsize4 = 0;
    int qsize5 = 0;
    int qsize6 = 0;

    if ( !notebookUid.isNull() ) {
      query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK );
    } else {
      query1 = SELECT_COMPONENTS_BY_LAST_MODIFIED;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_LAST_MODIFIED );
    }

    query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
    qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

    query3 = SELECT_ATTENDEE_BY_ID;
    qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

    query4 = SELECT_ALARM_BY_ID;
    qsize4 = sizeof( SELECT_ALARM_BY_ID );

    query5 = SELECT_RECURSIVE_BY_ID;
    qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

    query6 = SELECT_RDATES_BY_ID;
    qsize6 = sizeof( SELECT_RDATES_BY_ID );

    return d->selectIncidences( list,
                                query1, qsize1, query2, qsize2, query3, qsize3,
                                query4, qsize4, query5, qsize5,  query6, qsize6,
                                DBUpdate, after, notebookUid );
  }
  return false;
}

bool SqliteStorage::deletedIncidences( Incidence::List *list, const KDateTime &after,
                                       const QString &notebookUid )
{
  //if ( d->mIsOpened && list && after.isValid() ) {
  if ( d->mIsOpened && list ) {
    const char *query1 = NULL;
    const char *query2 = NULL;
    const char *query3 = NULL;
    const char *query4 = NULL;
    const char *query5 = NULL;
    const char *query6 = NULL;
    int qsize1 = 0;
    int qsize2 = 0;
    int qsize3 = 0;
    int qsize4 = 0;
    int qsize5 = 0;
    int qsize6 = 0;

    if ( !notebookUid.isNull() ) {
      query1 = SELECT_COMPONENTS_BY_DELETED_AND_NOTEBOOK;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_DELETED_AND_NOTEBOOK );
    } else {
      query1 = SELECT_COMPONENTS_BY_DELETED;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_DELETED );
    }

    return d->selectIncidences( list,
                                query1, qsize1, query2, qsize2, query3, qsize3,
                                query4, qsize4, query5, qsize5, query6, qsize6,
                                DBDelete, after, notebookUid );
  }
  return false;
}

bool SqliteStorage::allIncidences( Incidence::List *list, const QString &notebookUid )
{
  if ( d->mIsOpened && list ) {
    const char *query1 = NULL;
    const char *query2 = NULL;
    const char *query3 = NULL;
    const char *query4 = NULL;
    const char *query5 = NULL;
    const char *query6 = NULL;
    int qsize1 = 0;
    int qsize2 = 0;
    int qsize3 = 0;
    int qsize4 = 0;
    int qsize5 = 0;
    int qsize6 = 0;

    if ( !notebookUid.isNull() ) {
      query1 = SELECT_COMPONENTS_BY_NOTEBOOK;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_NOTEBOOK );
    } else {
      query1 = SELECT_COMPONENTS_ALL;
      qsize1 = sizeof( SELECT_COMPONENTS_ALL );
    }

    query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
    qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

    query3 = SELECT_ATTENDEE_BY_ID;
    qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

    query4 = SELECT_ALARM_BY_ID;
    qsize4 = sizeof( SELECT_ALARM_BY_ID );

    query5 = SELECT_RECURSIVE_BY_ID;
    qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

    query6 = SELECT_RDATES_BY_ID;
    qsize6 = sizeof( SELECT_RDATES_BY_ID );

    return d->selectIncidences( list,
                                query1, qsize1, query2, qsize2, query3, qsize3,
                                query4, qsize4, query5, qsize5, query6, qsize6,
                                DBSelect, KDateTime(), notebookUid );
  }
  return false;
}

bool SqliteStorage::duplicateIncidences( Incidence::List *list, const Incidence::Ptr &incidence,
                                         const QString &notebookUid )
{
  if ( d->mIsOpened && list && incidence ) {
    const char *query1 = NULL;
    const char *query2 = NULL;
    const char *query3 = NULL;
    const char *query4 = NULL;
    const char *query5 = NULL;
    const char *query6 = NULL;
    int qsize1 = 0;
    int qsize2 = 0;
    int qsize3 = 0;
    int qsize4 = 0;
    int qsize5 = 0;
    int qsize6 = 0;
    KDateTime dtStart;

    if ( incidence->dtStart().isValid() ) {
      dtStart = incidence->dtStart();
    } else {
      dtStart = KDateTime();
    }

    if ( !notebookUid.isNull() ) {
      query1 = SELECT_COMPONENTS_BY_DUPLICATE_AND_NOTEBOOK;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_DUPLICATE_AND_NOTEBOOK );
    } else {
      query1 = SELECT_COMPONENTS_BY_DUPLICATE;
      qsize1 = sizeof( SELECT_COMPONENTS_BY_DUPLICATE );
    }

    query2 = SELECT_CUSTOMPROPERTIES_BY_ID;
    qsize2 = sizeof( SELECT_CUSTOMPROPERTIES_BY_ID );

    query3 = SELECT_ATTENDEE_BY_ID;
    qsize3 = sizeof( SELECT_ATTENDEE_BY_ID );

    query4 = SELECT_ALARM_BY_ID;
    qsize4 = sizeof( SELECT_ALARM_BY_ID );

    query5 = SELECT_RECURSIVE_BY_ID;
    qsize5 = sizeof( SELECT_RECURSIVE_BY_ID );

    query6 = SELECT_RDATES_BY_ID;
    qsize6 = sizeof( SELECT_RDATES_BY_ID );

    return d->selectIncidences( list, query1, qsize1, query2, qsize2, query3, qsize3,
                                query4, qsize4, query5, qsize5, query6, qsize6,
                                DBSelect, dtStart, notebookUid, incidence->summary() );
  }
  return false;

}

bool SqliteStorage::loadNotebooks()
{
  const char *query = SELECT_CALENDARS_ALL;
  int qsize = sizeof( SELECT_CALENDARS_ALL );

  int rv = 0;
  sqlite3_stmt *stmt = NULL;
  const char *tail = NULL;

  Notebook::Ptr nb;

  if ( !d->mSem.acquire() ) {
    kError() << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    return false;
  }

  d->mIsLoading = true;

  sqlite3_prepare_v2( d->mDatabase, query, qsize, &stmt, &tail );

  while ( ( nb = d->mFormat->selectCalendars( stmt ) ) ) {
    kDebug() << "loaded notebook" << nb->uid() << nb->name() << "from database";
    if ( !addNotebook( nb ) ) {
      kWarning() << "cannot add notebook" << nb->uid() << nb->name() << "to storage";
      if ( nb ) {
        nb = Notebook::Ptr();
      }
    } else {
      if ( nb->isDefault() ) {
        setDefaultNotebook( nb );
      }
    }
  }
  sqlite3_reset( stmt );
  sqlite3_finalize( stmt );

  if ( !d->mSem.release() ) {
    kError() << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
  }
  d->mIsLoading = false;
  return true;

 error:
  if ( !d->mSem.release() ) {
    kError() << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
  }
  d->mIsLoading = false;
  return false;
}

bool SqliteStorage::reloadNotebooks()
{
  Notebook::List list = notebooks();
  Notebook::List::Iterator it = list.begin();
  d->mIsLoading = true;
  for ( ; it != list.end(); it++ ) {
    deleteNotebook( *it, true );
  }
  d->mIsLoading = false;

  return loadNotebooks();
}

bool SqliteStorage::modifyNotebook( const Notebook::Ptr &nb, DBOperation dbop, bool signal )
{
  int rv = 0;
  bool success = d->mIsLoading; // true if we are currently loading
  const char *query = NULL;
  int qsize = 0;
  sqlite3_stmt *stmt = NULL;
  const char *tail = NULL;
  const char *operation = ( dbop == DBInsert ) ? "inserting" :
                          ( dbop == DBUpdate ) ? "updating" : "deleting";

  if ( !d->mIsLoading ) {
    // Execute database operation.
    if ( dbop == DBInsert ) {
      query = INSERT_CALENDARS;
      qsize = sizeof( INSERT_CALENDARS );
    } else if ( dbop == DBUpdate ) {
      query = UPDATE_CALENDARS;
      qsize = sizeof( UPDATE_CALENDARS );
    } else if ( dbop == DBDelete ) {
      query = DELETE_CALENDARS;
      qsize = sizeof( DELETE_CALENDARS );
    } else {
      return false;
    }

    if ( !d->mSem.acquire() ) {
      kError() << "cannot lock" << d->mDatabaseName << "error" << d->mSem.errorString();
      return false;
    }

    sqlite3_prepare_v2( d->mDatabase, query, qsize, &stmt, &tail );

    if ( ( success = d->mFormat->modifyCalendars( nb, dbop, stmt ) ) ) {
      kDebug() << operation << "notebook" << nb->uid() << nb->name() << "in database";
    }

    sqlite3_reset( stmt );
    sqlite3_finalize( stmt );

    if ( !d->mSem.release() ) {
      kError() << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
    }
  }
  if ( success ) {
    if ( !d->mIsLoading && signal ) {
      d->mChanged.resize( 0 ); // make a change to create signal
    }
  }
  return success;

 error:
  if ( !d->mSem.release() ) {
    kError() << "cannot release lock" << d->mDatabaseName << "error" << d->mSem.errorString();
  }
  return false;
}

bool SqliteStorage::Private::checkVersion()
{
  int rv = 0;
  int index = 1;
  bool success = false;
  sqlite3_stmt *stmt = NULL;
  const char *tail = NULL;
  const char *query = SELECT_VERSION;
  int qsize = sizeof(SELECT_VERSION);
  int major = 0;
  int minor = 0;

  sqlite3_prepare_v2( mDatabase, query, qsize, &stmt, &tail );
  sqlite3_step( stmt );
  if ( rv == SQLITE_ROW ) {
    major = sqlite3_column_int( stmt, 0 );
    minor = sqlite3_column_int( stmt, 1 );
  }
  sqlite3_reset( stmt );
  sqlite3_finalize( stmt );

  if ( major == 0 ) {
    major = VersionMajor;
    minor = VersionMinor;
    query = INSERT_VERSION;
    qsize = sizeof( INSERT_VERSION );
    sqlite3_prepare_v2( mDatabase, query, qsize, &stmt, &tail );
    sqlite3_bind_int( stmt, index, major );
    sqlite3_bind_int( stmt, index, minor );
    sqlite3_step( stmt );
    kDebug() << "inserting version" << major << "." << minor << "in database";
    sqlite3_reset( stmt );
    sqlite3_finalize( stmt );
  }

  if ( major != VersionMajor ) {
    kError() << "database major version changed, new database has to be created";
  } else {
    success = true;
    if ( minor != VersionMinor ) {
      kWarning() << "database version changed";
    }
  }
  return success;

 error:
  return false;
}

bool SqliteStorage::Private::saveTimezones()
{
  int rv = 0;
  int index = 1;
  bool success = false;

  const char *query1 = UPDATE_TIMEZONES;
  int qsize1 = sizeof( UPDATE_TIMEZONES );
  sqlite3_stmt *stmt1 = NULL;
  const char *tail1 = NULL;

  ExtendedCalendar temp( mCalendar->timeSpec() );
  ICalTimeZones *zones = mCalendar->timeZones();
  if ( zones->count() > 0 ) {
    ICalTimeZones *copy = new ICalTimeZones( *zones );
    temp.setTimeZones( copy );
    ICalFormat ical;
    QByteArray data = ical.toString( ExtendedCalendar::Ptr( &temp ), QString() ).toUtf8();

    // Semaphore is already locked here.
    sqlite3_prepare_v2( mDatabase, query1, qsize1, &stmt1, &tail1 );
    sqlite3_bind_text( stmt1, index, data, data.length(), SQLITE_STATIC );
    sqlite3_step( stmt1 );
    success = true;
    kDebug() << "updated timezones in database";

    error:
    sqlite3_reset(stmt1);
    sqlite3_finalize(stmt1);

  } else {
    success = true;     //Zero TZ is not an error
  }

  return success;
}

bool SqliteStorage::Private::loadTimezones()
{
  int rv = 0;
  bool success = false;

  const char *query = SELECT_TIMEZONES;
  int qsize = sizeof( SELECT_TIMEZONES );
  sqlite3_stmt *stmt = NULL;
  const char *tail = NULL;

  sqlite3_prepare_v2( mDatabase, query, qsize, &stmt, &tail );

  if ( !mSem.acquire() ) {
    kError() << "cannot lock" << mDatabaseName << "error" << mSem.errorString();
    return false;
  }

  sqlite3_step( stmt );
  if ( rv == SQLITE_ROW ) {
    QString zoneData = QString::fromUtf8( (const char *)sqlite3_column_text( stmt, 1 ) );
    if ( !zoneData.isEmpty() ) {
      ExtendedCalendar temp( mCalendar->timeSpec() );
      ICalFormat ical;
      if ( ical.fromString( ExtendedCalendar::Ptr( &temp ), zoneData ) ) {
        kDebug() << "loaded timezones from database";
        ICalTimeZones *zones = temp.timeZones();
        ICalTimeZones *copy = new ICalTimeZones( *zones );
        mCalendar->setTimeZones( copy );
      } else {
        kWarning() << "failed to load timezones from database";
      }
    }
  }
  // Return true in any case, unless there was an sql error.
  success = true;

 error:
  sqlite3_reset( stmt );
  sqlite3_finalize( stmt );

  if ( !mSem.release() ) {
    kError() << "cannot release lock" << mDatabaseName << "error" << mSem.errorString();
  }
  return success;
}

bool SqliteStorage::Private::notifyOpened( Incidence::Ptr incidence )
{
  TrackerModify tracker;

  QStringList queryList;

  if ( tracker.notifyOpen( incidence, queryList ) ) {
    QString query = queryList.join( QString() );
#ifndef QT_NO_DEBUG
    // Use cerr to print only queries.
    cerr << endl << query.toAscii().constData() << endl;
    kDebug() << "tracker notify query";
#endif
    QDBusPendingReply<> update = mDBusIf->asyncCall( "SparqlUpdate", query );
#if defined(MKCAL_TRACKER_SYNC)
    update.waitForFinished();
    if ( update.isError() ) {
      kError() << "tracker query error:" << update.error().message();
      return false;
    }
#endif
  }
  return true;
}
//@endcond

void SqliteStorage::fileChanged( const QString &path )
{
  if ( QFileInfo( d->mDatabaseName + ".changed" ).lastModified() == d->mPreWatcherDbTime ) {
    // Invalidate this; mission done, prevented reload when loading database
    kDebug() << "prevented spurious database reload";
    d->mPreWatcherDbTime = QDateTime();
    return;
  }

  if ( !d->loadTimezones() ) {
    kError() << "loading timezones failed";
  }
  if ( !reloadNotebooks() ) {
    kError() << "loading notebooks failed";
  }
  setModified( path );
  kDebug() << path << "has been modified";

  // Based on what we've loaded, re-load things; the calendar observer
  // should be done by now and we should not have ANYTHING loaded.
  if ( d->mLoaded ) {
    if ( d->mLoaded & LOADED_LOAD ) {
      load();
    } else {
      if ( d->mLoaded & LOADED_LOAD_JOURNALS ) {
        loadJournals();
      }
      if ( d->mLoaded & LOADED_LOAD_RECURRING_INCIDENCES ) {
        loadRecurringIncidences();
      }
    }
  }
}

sqlite3_int64 SqliteStorage::toOriginTime( KDateTime dt )
{
  //kDebug() << "toOriginTime" << dt << d->mOriginTime.secsTo_long( dt );
  return d->mOriginTime.secsTo_long(dt);
}

KDateTime SqliteStorage::fromOriginTime( sqlite3_int64 seconds )
{
  //kDebug() << "fromOriginTime" << seconds << d->mOriginTime.addSecs( seconds ).toUtc();
  return d->mOriginTime.addSecs( seconds ).toUtc();
}

KDateTime SqliteStorage::fromOriginTime( sqlite3_int64 seconds, QString zonename )
{
  KDateTime dt;

  if ( seconds != 0 ) {
    if ( !zonename.isEmpty() ) {
      // First try system zones.
      KTimeZone ktimezone = KSystemTimeZones::zone(zonename);
      if ( ktimezone.isValid() ) {
        dt =
          d->mOriginTime.addSecs( seconds ).toUtc().toTimeSpec( KDateTime::Spec( ktimezone ) );
      } else {
        // Then try calendar specific zones.
        ICalTimeZones::ZoneMap zones = d->mCalendar->timeZones()->zones();
        ICalTimeZone icaltimezone = zones.value( zonename );
        if ( icaltimezone.isValid() ) {
          dt =
            d->mOriginTime.addSecs( seconds ).toUtc().toTimeSpec( KDateTime::Spec( icaltimezone ) );
        } else {
          // Invalid zone, fall back to UTC.
          dt = d->mOriginTime.addSecs( seconds ).toUtc();
        }
      }
    } else {
      // Empty zonename, use floating time.
      dt = d->mOriginTime.addSecs( seconds ).toClockTime();
    }
  }
  //kDebug() << "fromOriginTime" << seconds << zonename << dt;
  return dt;
}

bool SqliteStorage::initializeDatabase()
{
  kDebug() << "Storage is empty, initializing";

#if defined(MKCAL_FOR_MEEGO)
  MLocale locale;
  locale.installTrCatalog( "calendar" );
  Notebook::Ptr nbPersonal = Notebook::Ptr(
    new Notebook( QString(), qtTrId( "qtn_caln_personal_caln" ), QString(),
                  "#FF0000", false, true, false, false, true ) );
  addNotebook( nbPersonal, false );
  setDefaultNotebook( nbPersonal );
#else
  Notebook::Ptr nbDefault = Notebook::Ptr(
    new Notebook( QString(), QLatin1String( "Default" ), QString(),
                  "#0000FF", false, true, false, false, true ) );
  addNotebook( nbDefault, false );
  setDefaultNotebook( nbDefault );
#endif
  return true;
}
