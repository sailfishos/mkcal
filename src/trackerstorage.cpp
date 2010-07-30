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
  defines the TrackerStorage class.

  @brief
  This class provides a calendar storage as a content fw database.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/
#include <config-mkcal.h>
#include "trackerstorage.h"
#include "trackerformat.h"
using namespace KCalCore;

#include <kdebug.h>

#if defined(HAVE_UUID_UUID_H)
#include <uuid/uuid.h>
#endif

using namespace mKCal;

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::TrackerStorage::Private
{
  public:
  Private( const ExtendedCalendar::Ptr calendar, TrackerStorage *storage, bool synchronuousMode )
      : mCalendar( calendar ),
        mStorage( storage ),
        mFormat( 0 ),
        mSynchronuousMode( synchronuousMode ),
        mIsLoading( false ),
        mIsOpened( false ),
        mIsSignaled( false ),
        mSetLoadDates( false ),
        mDBusIf( 0 ),
        mOperation( StorageNone ),
        mOperationState( 0 ),
        mOperationError( false )
    {}
    ~Private() { }
    ExtendedCalendar::Ptr mCalendar;
    TrackerStorage *mStorage;
    TrackerFormat *mFormat;
    bool mSynchronuousMode;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToInsert;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToUpdate;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToDelete;
    QHash<QString, QString> mUidMappings;
    bool mIsLoading;
    bool mIsOpened;
    bool mIsSignaled;
    bool mSetLoadDates;
    QDBusInterface *mDBusIf;
    StorageOperation mOperation;
    int mOperationState;
    bool mOperationError;
    QString mOperationErrorMessage;
    QHash<Incidence::Ptr, QString> mOperationList;

    QHash<Incidence::Ptr,QString> filterIncidences( Incidence::List *origList );
};
//@endcond

TrackerStorage::TrackerStorage( const ExtendedCalendar::Ptr &cal, bool synchronuousMode )
  : ExtendedStorage( cal ), d( new Private( cal, this, synchronuousMode ) )
{
  cal->registerObserver( this );
}

TrackerStorage::~TrackerStorage()
{
  calendar()->unregisterObserver( this );
  close();
  delete d;
}

bool TrackerStorage::open()
{
  if ( d->mIsOpened ) {
    return false;
  }

  qRegisterMetaType<QVector<QStringList> >();
  qDBusRegisterMetaType<QVector<QStringList> >();

  QDBusConnection bus = QDBusConnection::sessionBus();

  if ( !bus.isConnected() ) {
    kError() << "DBus connection failed";
    return false;
  }

  bool retried = false;
retry:
  QStringList serviceNames = bus.interface()->registeredServiceNames();
  qDebug() << "DBus service names:" << serviceNames;

  d->mDBusIf = new QDBusInterface( "org.freedesktop.Tracker1",
                                   "/org/freedesktop/Tracker1/Resources",
                                   "org.freedesktop.Tracker1.Resources", bus );
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
  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Event",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsAdded",
               this, SLOT(SubjectsAdded(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Event",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsRemoved",
               this, SLOT(SubjectsRemoved(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Event",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsChanged",
               this, SLOT(SubjectsChanged(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Todo",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsAdded",
               this, SLOT(SubjectsAdded(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Todo",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsRemoved",
               this, SLOT(SubjectsRemoved(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Todo",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsChanged",
               this, SLOT(SubjectsChanged(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Journal",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsAdded",
               this, SLOT(SubjectsAdded(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Journal",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsRemoved",
               this, SLOT(SubjectsRemoved(QStringList)) );

  bus.connect( "", "/org/freedesktop/Tracker1/Resources/Classes/ncal/Journal",
               "org.freedesktop.Tracker1.Resources.Class", "SubjectsChanged",
               this, SLOT(SubjectsChanged(QStringList)) );

  KDateTime::setFromStringDefault( KDateTime::Spec::UTC() );

  d->mFormat = new TrackerFormat( this, d->mDBusIf, d->mSynchronuousMode );

  d->mIsOpened = true;

  return true;
}

bool TrackerStorage::load()
{
  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageLoad;
  d->mIsLoading = true;
  d->mSetLoadDates = true;
  d->mOperationList.clear();

  return d->mFormat->selectComponents( &d->mOperationList, QDate(), QDate(), DBNone,
                                       KDateTime(), QString(), QString(), Incidence::Ptr() );
}

bool TrackerStorage::load( const QString &uid, const KDateTime &recurrenceId )
{
  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageLoad;
  d->mIsLoading = true;
  d->mSetLoadDates = false;
  d->mOperationList.clear();

  QString keyuid = uid;
  if ( !recurrenceId.isNull() ) {
    keyuid.append( "-" );
    keyuid.append( recurrenceId.toString() );
  }
  return d->mFormat->selectComponents( &d->mOperationList, QDate(), QDate(), DBNone,
                                       KDateTime(), QString(), keyuid, Incidence::Ptr() );
}

bool TrackerStorage::load( const QDate &date )
{
  if ( date.isValid() ) {
    return load( date, date.addDays( 1 ) );
  }

  return false;
}

bool TrackerStorage::load( const QDate &start, const QDate &end )
{
  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageLoad;
  d->mIsLoading = true;
  d->mSetLoadDates = true;
  d->mOperationList.clear();

  KDateTime loadStart;
  KDateTime loadEnd;

  if ( getLoadDates( start, end, loadStart, loadEnd ) ) {
    return d->mFormat->selectComponents( &d->mOperationList, loadStart.date(), loadEnd.date(),
                                         DBNone, KDateTime(), QString(), QString(),
                                         Incidence::Ptr() );
  }

  return false;
}

bool TrackerStorage::loadNotebookIncidences( const QString &notebookUid )
{
  Q_UNUSED( notebookUid );

  return load();
}

bool TrackerStorage::loadJournals()
{
  // NOTE - this backend isn't really used
  // so this doesn't hurt anyone, hopefully.
  return load();
}

bool TrackerStorage::loadPlainIncidences()
{
  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageLoad;
  d->mIsLoading = true;
  d->mSetLoadDates = false;
  d->mOperationList.clear();

  return d->mFormat->selectComponents( &d->mOperationList, QDate(), QDate(), DBSelectPlain,
                                       KDateTime(), QString(), QString(), Incidence::Ptr() );

}

bool TrackerStorage::loadRecurringIncidences()
{
  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageLoad;
  d->mIsLoading = true;
  d->mSetLoadDates = false;
  d->mOperationList.clear();

  return d->mFormat->selectComponents( &d->mOperationList, QDate(), QDate(), DBSelectRecurring,
                                       KDateTime(), QString(), QString(), Incidence::Ptr() );

}

bool TrackerStorage::loadGeoIncidences()
{
  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageLoad;
  d->mIsLoading = true;
  d->mSetLoadDates = false;
  d->mOperationList.clear();

  return d->mFormat->selectComponents( &d->mOperationList, QDate(), QDate(), DBSelectGeo,
                                       KDateTime(), QString(), QString(), Incidence::Ptr() );

}

bool TrackerStorage::loadGeoIncidences( float geoLatitude, float geoLongitude,
                                        float diffLatitude, float diffLongitude )
{
  Q_UNUSED( geoLatitude );
  Q_UNUSED( geoLongitude );
  Q_UNUSED( diffLatitude );
  Q_UNUSED( diffLongitude );

  return loadGeoIncidences();
}

bool TrackerStorage::loadAttendeeIncidences()
{
  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageLoad;
  d->mIsLoading = true;
  d->mSetLoadDates = false;
  d->mOperationList.clear();

  return d->mFormat->selectComponents( &d->mOperationList, QDate(), QDate(), DBSelectAttendee,
                                       KDateTime(), QString(), QString(), Incidence::Ptr() );

}

int TrackerStorage::loadUncompletedTodos()
{
  return -1;
}

int TrackerStorage::loadCompletedTodos( bool hasDate, int limit, KDateTime *last )
{
  Q_UNUSED( hasDate );
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return -1;
}

int TrackerStorage::loadIncidences( bool hasDate, int limit, KDateTime *last )
{
  Q_UNUSED( hasDate );
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return -1;
}

int TrackerStorage::loadGeoIncidences( bool hasDate, int limit, KDateTime *last )
{
  Q_UNUSED( hasDate );
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return -1;
}

int TrackerStorage::loadUnreadInvitationIncidences()
{
  return -1;
}

int TrackerStorage::loadOldInvitationIncidences( int limit, KDateTime *last )
{
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return -1;
}

Person::List TrackerStorage::loadContacts()
{
  Person::List list;

  return list;
}

int TrackerStorage::loadContactIncidences( const Person::Ptr &person, int limit, KDateTime *last )
{
  Q_UNUSED( person );
  Q_UNUSED( limit );
  Q_UNUSED( last );

  return -1;
}

void TrackerStorage::loaded( const Incidence::Ptr &incidence )
{
  setProgress( "loaded " + incidence->uid() );
}

void TrackerStorage::loaded( bool error, QString message )
{
  QHash<Incidence::Ptr,QString>::const_iterator it;

  if ( d->mOperation == StorageLoad ) {
    if ( !error ) {
      // Load has been finished.
      for ( it = d->mOperationList.constBegin(); it != d->mOperationList.constEnd(); ++it ) {
        Incidence::Ptr incidence = it.key();
        if ( d->mIncidencesToInsert.contains( incidence->uid(), incidence ) ||
             d->mIncidencesToUpdate.contains( incidence->uid(), incidence ) ||
             d->mIncidencesToDelete.contains( incidence->uid(), incidence ) ) {
          incidence.clear();
        } else {
          if ( incidence->type() == Incidence::TypeEvent ) {
            Event::Ptr event = incidence.staticCast<Event>();
            Event::Ptr old;
            if ( !event->hasRecurrenceId() ) {
              old = calendar()->event( event->uid() );
            } else {
              old = calendar()->event( event->uid(), event->recurrenceId() );
            }
            if ( old ) {
              // Delete old event first.
              calendar()->deleteEvent( old );
            }
            kDebug() << "adding event" << event->uid() << "in calendar";
            calendar()->addEvent( event );
            calendar()->setNotebook( event, it.value() );
          } else if ( incidence->type() == Incidence::TypeTodo ) {
            Todo::Ptr todo = incidence.staticCast<Todo>();
            Todo::Ptr old;
            if ( !todo->hasRecurrenceId() ) {
              old = calendar()->todo( todo->uid() );
            } else {
              old = calendar()->todo( todo->uid(), todo->recurrenceId() );
            }
            if ( old ) {
              // Delete old todo first.
              calendar()->deleteTodo( old );
            }
            kDebug() << "adding todo" << todo->uid() << "in calendar";
            calendar()->addTodo( todo );
            calendar()->setNotebook( todo, it.value() );
          } else if ( incidence->type() == Incidence::TypeJournal ) {
            Journal::Ptr journal = incidence.staticCast<Journal>();
            Journal::Ptr old;
            if ( !journal->hasRecurrenceId() ) {
              old = calendar()->journal( journal->uid() );
            } else {
              old = calendar()->journal( journal->uid(), journal->recurrenceId() );
            }
            if ( old ) {
              // Delete old journal first.
              calendar()->deleteJournal( old );
            }
            kDebug() << "adding journal" << journal->uid() << "in calendar";
            calendar()->addJournal( journal );
            calendar()->setNotebook( journal, it.value() );
          }
        }
        if ( d->mSetLoadDates && incidence ) {
          setLoadDates( incidence->dtStart().date(), incidence->dtStart().date() );
        }
      }
    }
    d->mIsLoading = false;
  }
  // In case of sync operation all is done already.
  d->mOperation = StorageNone;
  setFinished( error, message );
}

bool TrackerStorage::save()
{
  Incidence::List tempList;

  if ( !d->mIsOpened || d->mOperation != StorageNone ) {
    return false;
  }

  d->mOperation = StorageSave;
  d->mOperationError = false;

  if ( d->mSynchronuousMode ) {
    // Incidences to insert
    tempList = d->mIncidencesToInsert.values();
    d->mOperationList = d->filterIncidences( &tempList );
    Incidence::List keyList = d->mOperationList.keys();
    foreach ( const Incidence::Ptr incidence, keyList ) {
      resetAlarms( incidence );
    }
    d->mFormat->modifyComponents( &d->mOperationList, DBInsert );
    if ( !d->mOperationError ) {
      // Incidences to update
      tempList = d->mIncidencesToUpdate.values();
      d->mOperationList = d->filterIncidences( &tempList );
      Incidence::List keyList = d->mOperationList.keys();
      foreach ( const Incidence::Ptr incidence, keyList ) {
        resetAlarms( incidence );
      }
      d->mFormat->modifyComponents( &d->mOperationList, DBUpdate );
    }
    if ( !d->mOperationError ) {
      // Incidences to delete
      tempList = d->mIncidencesToDelete.values();
      d->mOperationList = d->filterIncidences( &tempList );
      Incidence::List keyList = d->mOperationList.keys();
      foreach ( const Incidence::Ptr incidence, keyList ) {
        clearAlarms(incidence);
      }
      d->mFormat->modifyComponents( &d->mOperationList, DBDelete );
    }
    if ( !d->mOperationError ) {
      d->mIncidencesToInsert.clear();
      d->mIncidencesToUpdate.clear();
      d->mIncidencesToDelete.clear();
    }
    d->mOperation = StorageNone;
    setFinished( d->mOperationError, d->mOperationErrorMessage );
  } else {
    d->mOperationState = 1;
    tempList = d->mIncidencesToInsert.values();
    d->mOperationList = d->filterIncidences( &tempList );
    Incidence::List keyList = d->mOperationList.keys();
    foreach ( const Incidence::Ptr incidence, keyList ) {
      resetAlarms( incidence );
    }
    d->mFormat->modifyComponents( &d->mOperationList, DBInsert );
  }
  return !d->mOperationError;
}

void TrackerStorage::saved( const Incidence::Ptr &incidence )
{
  setProgress( "saved " + incidence->uid() );
}

void TrackerStorage::saved( bool error, QString message )
{
  Incidence::List tempList;

  if ( d->mSynchronuousMode ) {
    d->mOperationError = error;
    d->mOperationErrorMessage = message;
  } else {
    switch( d->mOperationState ) {
    case 1:
      if ( !error ) {
        // Incidences to update
        d->mOperationState = 2;
        tempList = d->mIncidencesToUpdate.values();
        d->mOperationList = d->filterIncidences( &tempList );
        Incidence::List keyList = d->mOperationList.keys();
        foreach ( const Incidence::Ptr incidence, keyList ) {
          resetAlarms( incidence );
        }
        d->mFormat->modifyComponents( &d->mOperationList, DBUpdate );
        break;
      } // flowthrough
    case 2:
      if ( !error ) {
        // Incidences to delete
        d->mOperationState = 3;
        tempList = d->mIncidencesToDelete.values();
        d->mOperationList = d->filterIncidences( &tempList );
        Incidence::List keyList = d->mOperationList.keys();
        foreach ( const Incidence::Ptr incidence, keyList ) {
          clearAlarms( incidence );
        }
        d->mFormat->modifyComponents( &d->mOperationList, DBDelete );
        break;
      } // flowthrough
    default:
      if ( !error ) {
        d->mIncidencesToInsert.clear();
        d->mIncidencesToUpdate.clear();
        d->mIncidencesToDelete.clear();
      }
      d->mOperation = StorageNone;
      setFinished( error, message );
      break;
    }
  }
}

bool TrackerStorage::cancel()
{
  if ( d->mIsOpened && d->mOperation ) {
    d->mFormat->cancel();
  }
  return true;
}

bool TrackerStorage::close()
{
  if ( d->mIsOpened ) {
    if ( d->mOperation ) {
      cancel();
    }
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

//@cond PRIVATE
QHash<Incidence::Ptr,QString> TrackerStorage::Private::filterIncidences( Incidence::List *origList )
{
  QHash<Incidence::Ptr,QString> list;
  Incidence::List::Iterator it;

  // Filtering incidences that have runtimeonly or readonly notebook
  for ( it = origList->begin(); it != origList->end(); ++it ) {
    QString notebookUid = mCalendar->notebook( (*it)->uid() );
    if ( !mStorage->isValidNotebook( notebookUid ) ) {
      kDebug() << "invalid notebook - not saving incidence" << (*it)->uid();
      continue;
    }
    list.insert( *it, notebookUid );
  }
  return list;
}
//@endcond

void TrackerStorage::calendarModified( bool modified, Calendar *calendar )
{
  Q_UNUSED( calendar );
  kDebug() << "calendarModified called:" << modified;
}

void TrackerStorage::calendarIncidenceAdded( const Incidence::Ptr &incidence )
{
  if (!d->mIncidencesToInsert.contains( incidence->uid(), incidence ) && !d->mIsLoading ) {
#if defined(HAVE_UUID_UUID_H)
    uuid_t uuid;
    char suuid[64];
    QString uid = incidence->uid();

    if ( uuid_parse( incidence->uid().toLatin1().data(), uuid ) ) {
      // Cannot accept this id, create better one.
      if ( !d->mUidMappings.contains( uid ) ) {
        uuid_generate_random( uuid );
        uuid_unparse( uuid, suuid );
        incidence->setUid( QString( suuid ) );
        kDebug() << "changing" << uid << "to" << incidence->uid();
      } else {
        incidence->setUid( d->mUidMappings.value( uid ) );
        kDebug() << "mapping" << uid << "to" << incidence->uid();
      }
    }
#else
//KDAB_TODO:
#ifdef __GNUC__
#warning no uuid support. what to do now?
#endif
#endif
    kDebug() << "appending incidence" << incidence->uid() << "for tracker insert";
    d->mIncidencesToInsert.insert( incidence->uid(), incidence );
    if ( !uid.isEmpty() ) {
      d->mUidMappings.insert( uid, incidence->uid() );
    }
  }
}

void TrackerStorage::calendarIncidenceChanged( const Incidence::Ptr &incidence )
{
  if ( !d->mIncidencesToUpdate.contains( incidence->uid(), incidence ) &&
       !d->mIncidencesToInsert.contains( incidence->uid(), incidence ) &&
       !d->mIsLoading ) {
    kDebug() << "appending incidence" << incidence->uid() << "for tracker update";
    d->mIncidencesToUpdate.insert( incidence->uid(), incidence );
    d->mUidMappings.insert( incidence->uid(), incidence->uid() );
  }
}

void TrackerStorage::calendarIncidenceDeleted( const Incidence::Ptr &incidence )
{
  if ( !d->mIncidencesToDelete.contains( incidence->uid(), incidence ) &&
       !d->mIsLoading && !d->mIsSignaled ) {
    kDebug() << "appending incidence" << incidence->uid() << "for tracker delete";
    d->mIncidencesToDelete.insert( incidence->uid(), incidence );
  }
}

void TrackerStorage::calendarIncidenceAdditionCanceled( const Incidence::Ptr &incidence )
{
  Q_UNUSED( incidence );
}

bool TrackerStorage::insertedIncidences( Incidence::List *list, const KDateTime &after,
                                         const QString &notebook )
{
  if ( d->mIsOpened && d->mOperation == StorageNone && list ) {
    QHash<Incidence::Ptr,QString> tempList;
    QHash<Incidence::Ptr,QString>::const_iterator it;
    d->mOperation = StorageInserted;
    if ( d->mFormat->selectComponents( &tempList, QDate(), QDate(), DBInsert, after,
                                       notebook, QString(), Incidence::Ptr() ) ) {
      for ( it = tempList.constBegin(); it != tempList.constEnd(); ++it ) {
        list->append( it.key() );
      }
      return true;
    }
  }
  return false;
}

bool TrackerStorage::modifiedIncidences( Incidence::List *list, const KDateTime &after,
                                         const QString &notebook )
{
  if ( d->mIsOpened && d->mOperation == StorageNone && list ) {
    QHash<Incidence::Ptr,QString> tempList;
    QHash<Incidence::Ptr,QString>::const_iterator it;
    d->mOperation = StorageModified;
    if ( d->mFormat->selectComponents( &tempList, QDate(), QDate(), DBUpdate, after,
                                       notebook, QString(), Incidence::Ptr() ) ) {
      for ( it = tempList.constBegin(); it != tempList.constEnd(); ++it ) {
        list->append( it.key() );
      }
      return true;
    }
  }
  return false;
}

bool TrackerStorage::deletedIncidences( Incidence::List *list, const KDateTime &after,
                                        const QString &notebook )
{
  if ( d->mIsOpened && d->mOperation == StorageNone && list ) {
    QHash<Incidence::Ptr,QString> tempList;
    QHash<Incidence::Ptr,QString>::const_iterator it;
    d->mOperation = StorageDeleted;
    if ( d->mFormat->selectComponents( &tempList, QDate(), QDate(), DBDelete, after,
                                       notebook, QString(), Incidence::Ptr() ) ) {
      for ( it = tempList.constBegin(); it != tempList.constEnd(); ++it ) {
        list->append( it.key() );
      }
      return true;
    }
  }
  return false;
}

bool TrackerStorage::allIncidences( Incidence::List *list, const QString &notebook )
{
  if ( d->mIsOpened && d->mOperation == StorageNone && list ) {
    QHash<Incidence::Ptr,QString> tempList;
    QHash<Incidence::Ptr,QString>::const_iterator it;
    d->mOperation = StorageAll;
    if ( d->mFormat->selectComponents(&tempList, QDate(), QDate(), DBSelect, KDateTime(),
                                      notebook, QString(), Incidence::Ptr() ) ) {
      for ( it = tempList.constBegin(); it != tempList.constEnd(); ++it ) {
        list->append( it.key() );
      }
      return true;
    }
  }
  return false;
}

bool TrackerStorage::duplicateIncidences( Incidence::List *list, const Incidence::Ptr &incidence,
                                          const QString &notebook )
{
  if ( d->mIsOpened && d->mOperation == StorageNone && list && incidence ) {
    QHash<Incidence::Ptr,QString> tempList;
    QHash<Incidence::Ptr,QString>::const_iterator it;
    d->mOperation = StorageDuplicate;
    if ( d->mFormat->selectComponents( &tempList, QDate(), QDate(), DBSelect, KDateTime(),
                                       notebook, QString(), incidence ) ) {
      for ( it = tempList.constBegin(); it != tempList.constEnd(); ++it ) {
        list->append( it.key() );
      }
      return true;
    }
  }
  return false;
}

bool TrackerStorage::notifyOpened( const Incidence::Ptr &incidence )
{
  Q_UNUSED( incidence );
  return false;
}

void TrackerStorage::SubjectsAdded( QStringList const &subjects )
{
  kDebug() << "SubjectsAdded" << subjects;

  QStringList uids;

  for ( int i = 0; i < subjects.size(); ++i ) {
    if ( subjects.at( i ).startsWith( QLatin1String( "urn:x-ical:" ) ) ) {
      uids << subjects.at( i ).mid( 11 ); // skip urn:x-ical:
    } else {
      uids << subjects.at( i );
    }
    // Cannot (re)load the incidence here, it might not succeed due
    // to another ongoing operation. Leave loading to the observer.
  }
  setModified( uids.join( " " ) );
}

void TrackerStorage::SubjectsRemoved( QStringList const &subjects )
{
  kDebug() << "SubjectsRemoved" << subjects;

  QStringList uids;

  for ( int i = 0; i < subjects.size(); ++i ) {
    if ( subjects.at( i ).startsWith( QLatin1String( "urn:x-ical:" ) ) ) {
      uids << subjects.at(i).mid(11); // skip urn:x-ical:
    } else {
      uids << subjects.at( i );
    }
    Incidence::Ptr incidence = calendar()->incidence( subjects.at( i ).mid( 11 ) );
    if ( incidence ) {
      // Delete from calendar memory.
      d->mIsSignaled = true;
      calendar()->deleteIncidence( incidence );
      d->mIsSignaled = false;
    }
  }
  setModified( uids.join( " " ) );
}

void TrackerStorage::SubjectsChanged( QStringList const &subjects )
{
  // kDebug() << "SubjectsChanged" << subjects;

  // NOTE this is not implemented since SubjectsAdded seems to be called
  // always anyway, there is no need to update twice

  Q_UNUSED( subjects );
}

bool TrackerStorage::loadNotebooks()
{
  return true;
}

bool TrackerStorage::reloadNotebooks()
{
  return true;
}

bool TrackerStorage::modifyNotebook( const Notebook::Ptr &nb, DBOperation dbop, bool signal )
{
  Q_UNUSED( nb );
  Q_UNUSED( dbop );
  Q_UNUSED( signal );

  return true;
}
