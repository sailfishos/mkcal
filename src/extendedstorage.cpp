/*
  This file is part of the mkcal library.

  Copyright (c) 2002,2003 Cornelius Schumacher <schumacher@kde.org>
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
  defines the ExtendedStorage abstract base class.

  @brief
  An abstract base class that provides a calendar storage interface.

  @author Cornelius Schumacher \<schumacher@kde.org\>
*/
#include <config-mkcal.h>
#include "extendedstorage.h"

#include <exceptions.h>
#include <calendar.h>
using namespace KCalCore;

#include <kdebug.h>

#if defined(HAVE_UUID_UUID_H)
#include <uuid/uuid.h>
#endif

#if defined(MKCAL_FOR_MEEGO)
#include <QtCore/QMap>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>
#endif

#ifdef TIMED_SUPPORT
#include <timed/interface>
#include <timed/event>
#include <timed/exception>
using namespace Maemo;
#endif

using namespace mKCal;

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::ExtendedStorage::Private
{
  public:
  Private( const ExtendedCalendar::Ptr &cal, bool validateNotebooks )
      : mCalendar( cal ),
        mValidateNotebooks( validateNotebooks ),
        mIsUncompletedTodosLoaded( false ),
        mIsCompletedTodosDateLoaded( false ),
        mIsCompletedTodosCreatedLoaded( false ),
        mIsDateLoaded( false ),
        mIsCreatedLoaded( false ),
        mIsGeoDateLoaded( false ),
        mIsGeoCreatedLoaded( false ),
        mIsUnreadIncidencesLoaded ( false ),
        mIsInvitationIncidencesLoaded ( false ),
        mIsJournalsLoaded( false ),
        mDefaultNotebook( 0 )
    {}
    ExtendedCalendar::Ptr mCalendar;
    bool mValidateNotebooks;
    QDate mStart;
    QDate mEnd;
    bool mIsUncompletedTodosLoaded;
    bool mIsCompletedTodosDateLoaded;
    bool mIsCompletedTodosCreatedLoaded;
    bool mIsDateLoaded;
    bool mIsCreatedLoaded;
    bool mIsGeoDateLoaded;
    bool mIsGeoCreatedLoaded;
    bool mIsUnreadIncidencesLoaded;
    bool mIsInvitationIncidencesLoaded;
    bool mIsJournalsLoaded;
    QList<StorageObserver*> mObservers;
    QHash<QString,Notebook::Ptr> mNotebooks; // uid to notebook
    Notebook::Ptr mDefaultNotebook;
};
//@endcond

ExtendedStorage::ExtendedStorage( const ExtendedCalendar::Ptr &cal, bool validateNotebooks )
  : CalStorage( cal ),
    d( new ExtendedStorage::Private ( cal, validateNotebooks ) )
{
  // Add the calendar as observer
  cal->registerObserver( this );
}

ExtendedStorage::~ExtendedStorage()
{
  // Unregister as observer; if we don't, when we terminate bad things happen
  calendar()->unregisterObserver( this );
  delete d;
}

void ExtendedStorage::clearLoaded()
{
  d->mStart = QDate();
  d->mEnd = QDate();
  d->mIsUncompletedTodosLoaded = false;
  d->mIsCompletedTodosDateLoaded = false;
  d->mIsCompletedTodosCreatedLoaded = false;
  d->mIsDateLoaded = false;
  d->mIsCreatedLoaded = false;
  d->mIsGeoDateLoaded = false;
  d->mIsGeoCreatedLoaded = false;
  d->mIsUnreadIncidencesLoaded  = false;
  d->mIsInvitationIncidencesLoaded  = false;
  d->mIsJournalsLoaded = false;
}

bool ExtendedStorage::getLoadDates( const QDate &start, const QDate &end,
                                    KDateTime &loadStart, KDateTime &loadEnd )
{
  // Check the need to load from db.
  if ( start.isValid() && d->mStart.isValid() && start >= d->mStart &&
       end.isValid() && d->mEnd.isValid() && end <= d->mEnd ) {
    return false;
  }

  // Set load dates to load only what's necessary.
  if ( start.isValid() && d->mStart.isValid() && start >= d->mStart ) {
    loadStart.setDate( d->mEnd );
  } else {
    loadStart.setDate( start ); // may be null if start is not valid
  }

  if ( end.isValid() && d->mEnd.isValid() && end <= d->mEnd ) {
    loadEnd.setDate( d->mStart );
  } else {
    loadEnd.setDate( end ); // may be null if end is not valid
  }

  loadStart.setTimeSpec( calendar()->timeSpec() );
  loadEnd.setTimeSpec( calendar()->timeSpec() );

  kDebug() << "get load dates" << start << end << loadStart << loadEnd;

  return true;
}

void ExtendedStorage::setLoadDates( const QDate &start, const QDate &end )
{
  // Set dates.
  if ( start.isValid() && ( !d->mStart.isValid() || start < d->mStart ) ) {
    d->mStart = start;
  }
  if ( end.isValid() && ( !d->mEnd.isValid() || end > d->mEnd ) ) {
    d->mEnd = end;
  }

  kDebug() << "set load dates" << d->mStart << d->mEnd;
}

bool ExtendedStorage::isUncompletedTodosLoaded()
{
  return d->mIsUncompletedTodosLoaded;
}

void ExtendedStorage::setIsUncompletedTodosLoaded( bool loaded )
{
  d->mIsUncompletedTodosLoaded = loaded;
}

bool ExtendedStorage::isCompletedTodosDateLoaded()
{
  return d->mIsCompletedTodosDateLoaded;
}

void ExtendedStorage::setIsCompletedTodosDateLoaded( bool loaded )
{
  d->mIsCompletedTodosDateLoaded = loaded;
}

bool ExtendedStorage::isCompletedTodosCreatedLoaded()
{
  return d->mIsCompletedTodosCreatedLoaded;
}

void ExtendedStorage::setIsCompletedTodosCreatedLoaded( bool loaded )
{
  d->mIsCompletedTodosCreatedLoaded = loaded;
}

bool ExtendedStorage::isDateLoaded()
{
  return d->mIsDateLoaded;
}

void ExtendedStorage::setIsDateLoaded( bool loaded )
{
  d->mIsDateLoaded = loaded;
}

bool ExtendedStorage::isJournalsLoaded()
{
  return d->mIsJournalsLoaded;
}

void ExtendedStorage::setIsJournalsLoaded( bool loaded )
{
  d->mIsJournalsLoaded = loaded;
}

bool ExtendedStorage::isCreatedLoaded()
{
  return d->mIsCreatedLoaded;
}

void ExtendedStorage::setIsCreatedLoaded( bool loaded )
{
  d->mIsCreatedLoaded = loaded;
}

bool ExtendedStorage::isGeoDateLoaded()
{
  return d->mIsGeoDateLoaded;
}

void ExtendedStorage::setIsGeoDateLoaded( bool loaded )
{
  d->mIsGeoDateLoaded = loaded;
}

bool ExtendedStorage::isGeoCreatedLoaded()
{
  return d->mIsGeoCreatedLoaded;
}

void ExtendedStorage::setIsGeoCreatedLoaded( bool loaded )
{
  d->mIsGeoCreatedLoaded = loaded;
}

bool ExtendedStorage::isUnreadIncidencesLoaded()
{
  return d->mIsUnreadIncidencesLoaded;
}

void ExtendedStorage::setIsUnreadIncidencesLoaded( bool loaded )
{
  d->mIsUnreadIncidencesLoaded = loaded;
}

bool ExtendedStorage::isInvitationIncidencesLoaded()
{
  return d->mIsInvitationIncidencesLoaded;
}

void ExtendedStorage::setIsInvitationIncidencesLoaded( bool loaded )
{
  d->mIsInvitationIncidencesLoaded = loaded;
}

ExtendedStorage::StorageObserver::~StorageObserver()
{
}

#if 0
void ExtendedStorage::StorageObserver::storageModified( ExtendedStorage *storage,
                                                        const QString &info )
{
  Q_UNUSED( storage );
  Q_UNUSED( info );
}

void ExtendedStorage::StorageObserver::storageProgress( ExtendedStorage *storage,
                                                        const QString &info )
{
  Q_UNUSED( storage );
  Q_UNUSED( info );
}

void ExtendedStorage::StorageObserver::storageFinished( ExtendedStorage *storage,
                                                        bool error, const QString &info )
{
  Q_UNUSED( storage );
  Q_UNUSED( error );
  Q_UNUSED( info );
}
#endif

void ExtendedStorage::registerObserver( StorageObserver *observer )
{
  if ( !d->mObservers.contains( observer ) ) {
    d->mObservers.append( observer );
  }
}

void ExtendedStorage::unregisterObserver( StorageObserver *observer )
{
  d->mObservers.removeAll( observer );
}

void ExtendedStorage::setModified( const QString &info )
{
  // Clear all smart loading variables
  d->mStart = QDate();
  d->mEnd = QDate();
  d->mIsUncompletedTodosLoaded = false;
  d->mIsCompletedTodosDateLoaded = false;
  d->mIsCompletedTodosCreatedLoaded = false;
  d->mIsGeoDateLoaded = false;
  d->mIsGeoCreatedLoaded = false;
  d->mIsUnreadIncidencesLoaded = false;
  d->mIsInvitationIncidencesLoaded = false;

  foreach ( StorageObserver *observer, d->mObservers ) {
    observer->storageModified( this, info );
  }
}

void ExtendedStorage::setProgress( const QString &info )
{
  foreach ( StorageObserver *observer, d->mObservers ) {
    observer->storageProgress( this, info );
  }
}

void ExtendedStorage::setFinished( bool error, const QString &info )
{
  foreach ( StorageObserver *observer, d->mObservers ) {
    observer->storageFinished( this, error, info );
  }
}

bool ExtendedStorage::addNotebook( const Notebook::Ptr &nb, bool signal )
{
#if defined(HAVE_UUID_UUID_H)
  uuid_t uuid;
  char suuid[64];
  if ( uuid_parse( nb->uid().toLatin1().data(), uuid ) ) {
    // Cannot accept this id, create better one.
    uuid_generate_random( uuid );
    uuid_unparse( uuid, suuid );
    nb->setUid( QString( suuid ) );
  }
#else
//KDAB_TODO:
#ifdef __GNUC__
#warning no uuid support. what to do now?
#endif
#endif

  if ( !nb || d->mNotebooks.contains( nb->uid() ) ) {
    return false;
  }

  if ( !calendar()->addNotebook( nb->uid(), nb->isVisible() ) ) {
    kError() << "cannot add notebook" << nb->uid() << "to calendar";
    return false;
  }

  if ( !modifyNotebook( nb, DBInsert, signal ) ) {
    calendar()->deleteNotebook( nb->uid() );
    return false;
  }
  d->mNotebooks.insert( nb->uid(), nb );

  return true;
}

bool ExtendedStorage::updateNotebook( const Notebook::Ptr &nb )
{
  if ( !nb || !d->mNotebooks.contains( nb->uid() ) ||
       d->mNotebooks.value( nb->uid() ) != nb ) {
    return false;
  }

  if ( !calendar()->updateNotebook( nb->uid(), nb->isVisible() ) ) {
    kError() << "cannot update notebook" << nb->uid() << "in calendar";
    return false;
  }
  if ( !modifyNotebook( nb, DBUpdate ) ) {
    return false;
  }

  return true;
}

bool ExtendedStorage::deleteNotebook( const Notebook::Ptr &nb, bool onlyMemory )
{
  if ( !nb || !d->mNotebooks.contains( nb->uid() ) ) {
    return false;
  }

  if ( !modifyNotebook( nb, DBDelete ) ) {
    return false;
  }

  // delete all notebook incidences from calendar
  if ( !onlyMemory ) {
    Incidence::List list;
    Incidence::List::Iterator it;
    if ( allIncidences( &list, nb->uid() ) ) {
      kDebug() << "deleting" << list.size() << "notes of notebook" << nb->name();
      for ( it = list.begin(); it != list.end(); ++it ) {
        Incidence::Ptr incidence = *it;
        load ( incidence->uid(), incidence->recurrenceId() );
        Incidence::Ptr toDelete = calendar()->incidence( incidence->uid() );
        calendar()->deleteIncidence( toDelete );
      }
      if ( !list.isEmpty() ) {
        save();
      }
    } else {
      kError() << "error when loading incidences for notebook" << nb->uid();
      return false;
    }
  }

  if ( !calendar()->deleteNotebook( nb->uid() ) ) {
    kError() << "cannot delete notebook" << nb->uid() << "from calendar";
    return false;
  }

  d->mNotebooks.remove( nb->uid() );

  if ( d->mDefaultNotebook == nb ) {
    d->mDefaultNotebook = Notebook::Ptr();
  }

  return true;
}

bool ExtendedStorage::setDefaultNotebook( const Notebook::Ptr &nb )
{
  if ( !nb || !d->mNotebooks.contains( nb->uid() ) ) {
    return false;
  }

  if (d->mDefaultNotebook) {
    d->mDefaultNotebook->setIsDefault( false );
    if ( !modifyNotebook( d->mDefaultNotebook, DBUpdate, false ) ) {
      return false;
    }
  }

  d->mDefaultNotebook = nb;
  d->mDefaultNotebook->setIsDefault( true );
  if ( !modifyNotebook( d->mDefaultNotebook, DBUpdate ) ) {
    return false;
  }

  calendar()->setDefaultNotebook( nb->uid() );

  return true;
}

Notebook::Ptr ExtendedStorage::defaultNotebook()
{
  if ( d->mDefaultNotebook ) {
    return d->mDefaultNotebook;
  } else {
    return Notebook::Ptr();
  }
}

Notebook::List ExtendedStorage::notebooks()
{
  return d->mNotebooks.values();
}

Notebook::Ptr ExtendedStorage::notebook( const QString &uid )
{
  if ( d->mNotebooks.contains( uid ) ) {
    return d->mNotebooks.value( uid );
  } else {
    return Notebook::Ptr();
  }
}

void ExtendedStorage::setValidateNotebooks( bool validateNotebooks )
{
  d->mValidateNotebooks = validateNotebooks;
}

bool ExtendedStorage::validateNotebooks()
{
  return d->mValidateNotebooks;
}

bool ExtendedStorage::isValidNotebook( const QString &notebookUid )
{
  Notebook::Ptr nb = notebook( notebookUid );
  if ( nb ) {
    if ( nb->isRunTimeOnly() || nb->isReadOnly() ) {
      kDebug() << "notebook" << notebookUid << "isRunTimeOnly or isReadOnly";
      return false;
    }
  } else if ( validateNotebooks() ) {
    kDebug() << "notebook" << notebookUid << "is not valid for this storage";
    return false;
  } else if ( calendar()->hasValidNotebook( notebookUid ) ) {
    kDebug() << "notebook" << notebookUid << "is saved by another storage";
    return false;
  }
  return true;
}

void ExtendedStorage::resetAlarms( const Incidence::Ptr &incidence )
{
#if defined(TIMED_SUPPORT)
  Timed::Interface timed;
  if ( !timed.isValid() ) {
    kError() << "cannot reset alarms for" << incidence->uid()
             << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" )
             << "alarm interface is not valid" << timed.lastError();
    return;
  }
  clearAlarms( incidence );

  Alarm::List alarms = incidence->alarms();
  foreach ( const Alarm::Ptr alarm, alarms ) {
    if ( !alarm->enabled() ) {
      continue;
    }

    KDateTime now = KDateTime::currentLocalDateTime();
    KDateTime alarmTime = alarm->nextTime( now, true );
    if ( !alarmTime.isValid() ) {
      continue;
    }

    if ( now.addSecs( 60 ) > alarmTime ) {
      // don't allow alarms at the same minute -> take next alarm if so
      alarmTime = alarm->nextTime( now.addSecs( 60 ), true );
      if ( !alarmTime.isValid() ) {
        continue;
      }
    }
    Timed::Event e;
    if ( alarmTime.isUtc() ) {
      e.setTicker( alarmTime.toTime_t() );
    } else {
      QDate date = alarmTime.date();
      QTime time = alarmTime.time();

      try {
        e.setTime(date.year(), date.month(), date.day(), time.hour(), time.minute());
      } catch (Timed::Exception &e) {
        qDebug() << "Got Maemo::Timed::Exception" << e.message();
        return;
      }

      if ( !alarmTime.isClockTime() ) {
        e.setTimezone( alarmTime.timeZone().name() );
      }
    }
    // The code'll crash (=exception) iff the content is empty. So
    // we have to check here.
    QString s;

    s = incidence->summary();
    // Timed braindeath: Required field, BUT if empty, it asserts
    if ( s.isEmpty() ) {
      s = ' ';
    }
    e.setAttribute( "TITLE", s );
    e.setAttribute( "PLUGIN", "libCalendarReminder" );
    e.setAttribute( "APPLICATION", "libextendedkcal" );
    //e.setAttribute( "translation", "organiser" );
    // This really has to exist or code is badly broken
    Q_ASSERT( !incidence->uid().isEmpty() );
    e.setAttribute( "uid", incidence->uid() );
    if ( !incidence->location().isEmpty() ) {
      e.setAttribute( "location", incidence->location() );
    }
    if ( incidence->recurs() ) {
      e.setAttribute( "recurs", "true" );
    }

    // TODO - consider this how it should behave for recurrence
    if ( ( incidence->type() == Incidence::TypeTodo ) ) {
        Todo::Ptr todo = incidence.staticCast<Todo>();

        if ( todo->hasDueDate() ) {
            e.setAttribute( "time", todo->dtDue( true ).toString() );
        }
    } else if ( incidence->dtStart().isValid() ) {
      e.setAttribute( "time", incidence->dtStart().toString() );
    }

    if ( incidence->hasRecurrenceId() ) {
      e.setAttribute( "recurrenceId", incidence->recurrenceId().toString() );
    }
    e.setAttribute( "notebook", calendar()->notebook( incidence->uid() ) );

    Timed::Event::Button &s15 = e.addButton();
    s15.setAttribute( "snooze_value", "15" );
    s15.setSnooze( 900 );
    s15.setAttribute( "label", "Snooze 15 minutes" );

    Timed::Event::Button &s10 = e.addButton();
    s10.setAttribute( "snooze_value", "10" );
    s10.setSnooze( 600 );
    s10.setAttribute( "label", "Snooze 10 minutes" );

    Timed::Event::Button &s05 = e.addButton();
    s05.setAttribute( "snooze_value", "5" );
    s05.setSnooze( 300 );
    s05.setAttribute( "label", "Snooze 5 minutes" );

    Timed::Event::Button &open = e.addButton();
    open.setAttribute( "label", "Close" );

    e.hideSnoozeButton1();
    e.setAlignedSnoozeFlag();

    QDBusReply < uint > reply = timed.add_event_sync( e );
    if ( reply.isValid() ) {
      uint32_t cookie = reply.value();
      if ( !cookie ) {
        kError() << "failed to add alarm for" << incidence->uid()
                 << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" )
                 << "at" << alarmTime;
      } else {
        kDebug() << "adding alarm" << cookie << incidence->uid()
                 << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" )
                 << "at" << alarmTime;
      }
    } else {
      kError() << "failed to add alarm for" << incidence->uid()
               << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" )
               << timed.lastError();
    }
  }
#else
  Q_UNUSED( incidence );
#endif
}

void ExtendedStorage::clearAlarms( const Incidence::Ptr &incidence )
{
#if defined(TIMED_SUPPORT)
  QMap<QString,QVariant> map;
  map["APPLICATION"] = "libextendedkcal";
  map["uid"] = incidence->uid();
  if ( incidence->hasRecurrenceId() ) {
    map["recurrenceId"] = incidence->recurrenceId().toString();
  }

  Timed::Interface timed;
  if ( !timed.isValid() ) {
    kError() << "cannot clear alarms for" << incidence->uid()
             << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" )
             << "alarm interface is not valid" << timed.lastError();
    return;
  }
  QDBusReply<QList<QVariant> > reply = timed.query_sync( map );
  if ( !reply.isValid() ) {
    kError() << "cannot clear alarms for" << incidence->uid()
             << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" )
             << timed.lastError();
    return;
  }

  const QList<QVariant> &result = reply.value();
  for ( int i = 0; i < result.size(); i++ ) {
    uint32_t cookie = result[i].toUInt();
    kDebug() << "removing alarm" << cookie << incidence->uid()
             << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" );
    QDBusReply<bool> reply = timed.cancel_sync( cookie );
    if ( !reply.isValid() || !reply.value() ) {
      kError() << "cannot remove alarm" << cookie << incidence->uid()
               << ( incidence->hasRecurrenceId() ? incidence->recurrenceId().toString() : "-" )
               << reply.value() << timed.lastError();
    }
  }
#else
  Q_UNUSED( incidence );
#endif
}

void ExtendedStorage::clearAlarms( const QString &nb )
{
#if defined(TIMED_SUPPORT)
  QMap<QString,QVariant> map;
  map["APPLICATION"] = "libextendedkcal";
  map["notebook"] = nb;

  Timed::Interface timed;
  if ( !timed.isValid() ) {
    kError() << "cannot clear alarms for" << nb
             << "alarm interface is not valid" << timed.lastError();
    return;
  }
  QDBusReply<QList<QVariant> > reply = timed.query_sync( map );
  if ( !reply.isValid() ) {
    kError() << "cannot clear alarms for" << nb << timed.lastError();
    return;
  }
  const QList<QVariant> &result = reply.value();
  for ( int i = 0; i < result.size(); i++ ) {
    uint32_t cookie = result[i].toUInt();
    kDebug() << "removing alarm" << cookie << nb;
    QDBusReply<bool> reply = timed.cancel_sync( cookie );
    if ( !reply.isValid() || !reply.value() ) {
      kError() << "cannot remove alarm" << cookie << nb;
    }
  }
#else
  Q_UNUSED( nb );
#endif
}

Incidence::Ptr ExtendedStorage::checkAlarm( const QString &uid, const QString &recurrenceId,
                                            bool loadAlways )
{
  KDateTime rid;

  if ( !recurrenceId.isEmpty() ) {
    rid = KDateTime::fromString( recurrenceId );
  }
  Incidence::Ptr incidence = calendar()->incidence( uid, rid );
  if ( !incidence || loadAlways ) {
    load( uid, rid );
    incidence = calendar()->incidence( uid, rid );
  }
  if ( incidence && incidence->hasEnabledAlarms() ) {
    // Return incidence if it exists and has active alarms.
    return incidence;
  }
  return Incidence::Ptr();
}


Notebook::Ptr ExtendedStorage::createDefaultNotebook( QString name, QString color )
{
#ifdef MKCAL_FOR_MEEGO
  if (name.isEmpty()) {
    MLocale locale;
    locale.installTrCatalog("calendar");
    MLocale::setDefault(locale);
    name = qtTrId("qtn_caln_personal_caln");
  }
  if (color.isEmpty())
      color = "#63B33B";
#else
  if (name.isEmpty())
    name = "Default";
  if (color.isEmpty())
    color = "#0000FF";
#endif
  Notebook::Ptr nbDefault = Notebook::Ptr( new Notebook(QString(), name, QString(), color, false, true, false, false, true) );
  addNotebook(nbDefault, false);
  setDefaultNotebook(nbDefault);
  return nbDefault;
}
