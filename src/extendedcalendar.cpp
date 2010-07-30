/*
  This file is part of the mkcal library.

  Copyright (c) 1998 Preston Brown <pbrown@kde.org>
  Copyright (c) 2001,2003,2004 Cornelius Schumacher <schumacher@kde.org>
  Copyright (C) 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
  Copyright (c) 2009 Alvaro Manera <alvaro.manera@nokia.com>

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
  defines the ExtendedCalendar class.

  @brief
  This class provides a calendar cached into memory.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Preston Brown \<pbrown@kde.org\>
  @author Cornelius Schumacher \<schumacher@kde.org\>
 */

#include "extendedcalendar.h"
#include "sqlitestorage.h"

#include <calfilter.h>
#include <sorting.h>
using namespace KCalCore;

#include <kdebug.h>

#include <QtCore/QDir>

#include <cmath>

using namespace mKCal;
/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::ExtendedCalendar::Private
{
  public:
    Private()
    {
    }
    ~Private()
    {
    }
    QMultiHash<QString,Event::Ptr>mEvents;           // hash on uids of all Events
    QMultiHash<QString,Event::Ptr>mEventsForDate;    // on start dates of non-recurring,
                                                     //   single-day Events

    QMultiHash<QString,Todo::Ptr>mTodos;             // hash on uids of all Todos
    QMultiHash<QString,Todo::Ptr>mTodosForDate;      // on due/start dates for all Todos

    QMultiHash<QString,Journal::Ptr>mJournals;       // hash on uids of all Journals
    QMultiHash<QString,Journal::Ptr>mJournalsForDate;// on dates of all Journals

    Incidence::List mGeoIncidences;                  // list of all Geo Incidences

    QMultiHash<QString,Event::Ptr> mDeletedEvents;     // list of all deleted Events
    QMultiHash<QString,Todo::Ptr> mDeletedTodos;       // list of all deleted Todos
    QMultiHash<QString,Journal::Ptr> mDeletedJournals; // list of all deleted Journals

    QMultiHash<QString,Incidence::Ptr>mAttendeeIncidences; // lists of incidences for attendees

    void insertEvent( const Event::Ptr &event, const KDateTime::Spec &timeSpec );
    void insertTodo( const Todo::Ptr &todo, const KDateTime::Spec &timeSpec );
    void insertJournal( const Journal::Ptr &journal, const KDateTime::Spec &timeSpec );
};

ExtendedCalendar::ExtendedCalendar( const KDateTime::Spec &timeSpec )
  : MemoryCalendar( timeSpec ), d( new mKCal::ExtendedCalendar::Private )
{
}

ExtendedCalendar::ExtendedCalendar( const QString &timeZoneId )
  : MemoryCalendar( timeZoneId ), d( new mKCal::ExtendedCalendar::Private )
{
}

ExtendedCalendar::~ExtendedCalendar()
{
  close();
  delete d;
}

bool ExtendedCalendar::reload()
{
  // Doesn't belong here.
  return false;
}

bool ExtendedCalendar::save()
{
  // Doesn't belong here.
  return false;
}

void ExtendedCalendar::close()
{
  setObserversEnabled( false );

  deleteAllEvents();
  deleteAllTodos();
  deleteAllJournals();

  d->mDeletedEvents.clear();
  d->mDeletedTodos.clear();
  d->mDeletedJournals.clear();

  clearNotebookAssociations();

  setModified( false );

  setObserversEnabled( true );
}

ICalTimeZone ExtendedCalendar::parseZone( MSTimeZone *tz )
{
  ICalTimeZone zone;

  ICalTimeZones *icalZones = timeZones();
  if (icalZones) {
    ICalTimeZoneSource src;
    zone = src.parse( tz, *icalZones );
  }
  return zone;
}

void ExtendedCalendar::doSetTimeSpec( const KDateTime::Spec &timeSpec )
{
  // Reset date based hashes to the new spec.
  d->mEventsForDate.clear();
  d->mTodosForDate.clear();
  d->mJournalsForDate.clear();

  QHashIterator<QString,Event::Ptr>ie( d->mEvents );
  while ( ie.hasNext() ) {
    ie.next();
    d->mEventsForDate.insert(
      ie.value()->dtStart().toTimeSpec( timeSpec ).date().toString(), ie.value() );
  }

  QHashIterator<QString,Todo::Ptr>it( d->mTodos );
  while ( it.hasNext() ) {
    it.next();
    Todo::Ptr todo = it.value();
    if ( todo->hasDueDate() ) {
      d->mTodosForDate.insert( todo->dtDue().toTimeSpec( timeSpec ).date().toString(), todo );
    } else if ( todo->hasStartDate() ) {
      d->mTodosForDate.insert(
        todo->dtStart().toTimeSpec( timeSpec ).date().toString(), todo );
    }
  }

  QHashIterator<QString,Journal::Ptr>ij( d->mJournals );
  while ( ij.hasNext() ) {
    ij.next();
    d->mJournalsForDate.insert(
      ij.value()->dtStart().toTimeSpec( timeSpec ).date().toString(), ij.value() );
  }
}

// Dissociate a single occurrence or all future occurrences from a recurring
// sequence. The new incidence is returned, but not automatically inserted
// into the calendar, which is left to the calling application.
Incidence::Ptr ExtendedCalendar::dissociateSingleOccurrence( const Incidence::Ptr &incidence,
                                                             const KDateTime &dateTime,
                                                             const KDateTime::Spec &spec )
{
  if ( !incidence || !incidence->recurs() ) {
    return Incidence::Ptr();
  }

  if ( !dateTime.isDateOnly() ) {
    if ( !incidence->recursAt( dateTime ) ) {
      return Incidence::Ptr();
    }
  } else {
    if ( !incidence->recursOn( dateTime.date(), spec ) ) {
      return Incidence::Ptr();
    }
  }

  Incidence::Ptr newInc = Incidence::Ptr( incidence->clone() );
  KDateTime nowUTC = KDateTime::currentUtcDateTime();
  incidence->setCreated( nowUTC );
  incidence->setSchedulingID( QString() );
  incidence->setLastModified( nowUTC );

  Recurrence *recur = newInc->recurrence();
  recur->clear();

  // Adjust the date of the incidence
  if ( incidence->type() == Incidence::TypeEvent ) {
    Event::Ptr ev = newInc.staticCast<Event>();
    KDateTime start( ev->dtStart() );
    int secsTo =
      start.toTimeSpec( spec ).dateTime().secsTo( dateTime.toTimeSpec( spec ).dateTime() );
    ev->setDtStart( start.addSecs( secsTo ) );
    ev->setDtEnd( ev->dtEnd().addSecs( secsTo ) );
  } else if ( incidence->type() == Incidence::TypeTodo ) {
    Todo::Ptr td = newInc.staticCast<Todo>();
    bool haveOffset = false;
    int secsTo = 0;
    if ( td->hasDueDate() ) {
      KDateTime due( td->dtDue() );
      secsTo = due.toTimeSpec( spec ).dateTime().secsTo( dateTime.toTimeSpec( spec ).dateTime() );
      td->setDtDue( due.addSecs( secsTo ), true );
      haveOffset = true;
    }
    if ( td->hasStartDate() ) {
      KDateTime start( td->dtStart() );
      if ( !haveOffset ) {
        secsTo =
          start.toTimeSpec( spec ).dateTime().secsTo( dateTime.toTimeSpec( spec ).dateTime() );
      }
      td->setDtStart( start.addSecs( secsTo ) );
      haveOffset = true;
    }
  } else if ( incidence->type() == Incidence::TypeJournal ) {
    Journal::Ptr jr = newInc.staticCast<Journal>();
    KDateTime start( jr->dtStart() );
    int secsTo =
      start.toTimeSpec( spec ).dateTime().secsTo( dateTime.toTimeSpec( spec ).dateTime() );
    jr->setDtStart( start.addSecs( secsTo ) );
  }

  // set recurrenceId for new incidence
  newInc->setRecurrenceId( dateTime );

  recur = incidence->recurrence();
  if ( recur ) {
    if ( dateTime.isDateOnly() ) {
      recur->addExDate( dateTime.date() );
    } else {
      recur->addExDateTime( dateTime );
    }
  }

  return newInc;
}

bool ExtendedCalendar::addEvent( const Event::Ptr &aEvent )
{
  if ( !aEvent ) {
    return false;
  }

  Event::Ptr eventToAdd = aEvent;

  notifyIncidenceAdded( aEvent );

  if ( d->mEvents.contains( aEvent->uid() ) ) {
    Event::Ptr old;
    if ( !aEvent->hasRecurrenceId() ) {
      old = event( aEvent->uid() );
    } else {
      old = event( aEvent->uid(), aEvent->recurrenceId() );
    }
    if ( old ) {
      if ( aEvent->revision() > old->revision() ) {
        deleteEvent( old ); // move old to deleted
      } else {
        notifyIncidenceAdditionCanceled( aEvent );
        eventToAdd = old;
        return true;
      }
    }
  }

  d->insertEvent( eventToAdd, timeSpec() );

  eventToAdd->registerObserver( this );

  setModified( true );

  if ( !defaultNotebook().isEmpty() ) {
    return setNotebook( eventToAdd, defaultNotebook() );
  }

  return true;
}

bool ExtendedCalendar::deleteEvent( const Event::Ptr &event )
{
  const QString uid = event->uid();
  if ( d->mEvents.remove( uid, event ) ) {
    event->unRegisterObserver( this );
    setModified( true );
    notifyIncidenceDeleted( event );
    d->mDeletedEvents.insert( uid, event );

    if ( event->hasGeo() ) {
      d->mGeoIncidences.removeAll( event );
    }

    d->mEventsForDate.remove( event->dtStart().toTimeSpec( timeSpec() ).date().toString(), event );

    // Delete from attendee events.
    Person::Ptr organizer = event->organizer();
    if ( !organizer->isEmpty() ) {
      d->mAttendeeIncidences.remove( organizer->email(), event );
    }
    const Attendee::List &list = event->attendees();
    Attendee::List::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
      d->mAttendeeIncidences.remove( (*it)->email(), event );
    }

    // Delete child-events.
    if ( !event->hasRecurrenceId() ) {
      deleteEventInstances( event );
    }

    KDateTime nowUTC = KDateTime::currentUtcDateTime();
    event->setLastModified( nowUTC );
    return true;
  } else {
    kWarning() << "Event not found.";
    return false;
  }
}

bool ExtendedCalendar::deleteEventInstances( const Event::Ptr &event )
{
  QList<Event::Ptr> values = d->mEvents.values( event->uid() );
  QList<Event::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( (*it)->hasRecurrenceId() ) {
      kDebug() << "deleting child event" << (*it)->uid()
               << (*it)->dtStart() << (*it)->dtEnd()
               << "in calendar";
      deleteEvent( (*it) );
    }
  }

  return true;
}

void ExtendedCalendar::deleteAllEvents()
{
  QHashIterator<QString,Event::Ptr>i( d->mEvents );
  while ( i.hasNext() ) {
    i.next();
    notifyIncidenceDeleted( i.value() );
    // suppress update notifications for the relation removal triggered
    // by the following deletions
    i.value()->startUpdates();
  }
  d->mEvents.clear();
  d->mEventsForDate.clear();
}

Event::Ptr ExtendedCalendar::event( const QString &uid, const KDateTime &recurrenceId ) const
{
  QList<Event::Ptr> values = d->mEvents.values( uid );
  QList<Event::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( recurrenceId.isNull() ) {
      if ( !(*it)->hasRecurrenceId() ) {
        return *it;
      }
    } else {
      if ( (*it)->hasRecurrenceId() && (*it)->recurrenceId() == recurrenceId ) {
        return *it;
      }
    }
  }
  return Event::Ptr();
}

Event::Ptr ExtendedCalendar::deletedEvent( const QString &uid, const KDateTime &recurrenceId ) const
{
  QList<Event::Ptr> values = d->mDeletedEvents.values( uid );
  QList<Event::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( recurrenceId.isNull() ) {
      if ( !(*it)->hasRecurrenceId() ) {
        return *it;
      }
    } else {
      if ( (*it)->hasRecurrenceId() && (*it)->recurrenceId() == recurrenceId ) {
        return *it;
      }
    }
  }
  return Event::Ptr();
}

bool ExtendedCalendar::addTodo( const Todo::Ptr &aTodo )
{
  if ( !aTodo ) {
    return false;
  }

  Todo::Ptr todoToAdd = aTodo;

  notifyIncidenceAdded( aTodo );

  if ( d->mTodos.contains( aTodo->uid() ) ) {
    Todo::Ptr old;
    if ( !aTodo->hasRecurrenceId() ) {
      old = todo( aTodo->uid() );
    } else {
      old = todo( aTodo->uid(), aTodo->recurrenceId() );
    }
    if ( old ) {
      if ( aTodo->revision() > old->revision() ) {
        deleteTodo( old ); // move old to deleted
      } else {
        notifyIncidenceAdditionCanceled( aTodo );
        todoToAdd = old;
        return true;
      }
    }
  }

  d->insertTodo( todoToAdd, timeSpec() );

  todoToAdd->registerObserver( this );

  // Set up sub-to-do relations
  setupRelations( todoToAdd );

  setModified( true );

  if ( !defaultNotebook().isEmpty() ) {
    return setNotebook( todoToAdd, defaultNotebook() );
  }

  return true;
}

//@cond PRIVATE
void ExtendedCalendar::Private::insertTodo( const Todo::Ptr &todo, const KDateTime::Spec &timeSpec )
{
  QString uid = todo->uid();
  if ( !mTodos.contains( uid, todo ) ) {
    mTodos.insert( uid, todo );
    if ( todo->hasDueDate() ) {
      mTodosForDate.insert( todo->dtDue().toTimeSpec(timeSpec).date().toString(), todo );
    } else if ( todo->hasStartDate() ) {
      mTodosForDate.insert( todo->dtStart().toTimeSpec(timeSpec).date().toString(), todo );
    }

    // Insert into attendee todos.
    Person::Ptr organizer = todo->organizer();
    if ( organizer->isEmpty() ) {
      mAttendeeIncidences.insert( organizer->email(), todo );
    }
    const Attendee::List &list = todo->attendees();
    Attendee::List::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
      mAttendeeIncidences.insert( (*it)->email(), todo );
    }
    if ( todo->hasGeo() ) {
      mGeoIncidences.append( todo );
    }
  } else {
#ifndef NDEBUG
    // if we already have an to-do with this UID, it must be the same to-do,
    // otherwise something's really broken
    Q_ASSERT( mTodos.value( uid ) == todo );
#endif
  }
}
//@endcond

bool ExtendedCalendar::deleteTodo( const Todo::Ptr &todo )
{
  // Handle orphaned children
  removeRelations( todo );

  if ( d->mTodos.remove( todo->uid(), todo ) ) {
    todo->unRegisterObserver( this );
    setModified( true );
    notifyIncidenceDeleted( todo );
    d->mDeletedTodos.insert( todo->uid(), todo );

    if ( todo->hasGeo() ) {
      d->mGeoIncidences.removeAll( todo );
    }

    if ( todo->hasDueDate() ) {
      d->mTodosForDate.remove( todo->dtDue().toTimeSpec( timeSpec() ).date().toString(), todo );
    } else if ( todo->hasStartDate() ) {
      d->mTodosForDate.remove( todo->dtStart().toTimeSpec( timeSpec() ).date().toString(), todo );
    }

    // Delete from attendee todos.
    Person::Ptr organizer = todo->organizer();
    if ( !organizer->isEmpty() ) {
      d->mAttendeeIncidences.remove( organizer->email(), todo );
    }
    const Attendee::List &list = todo->attendees();
    Attendee::List::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
      d->mAttendeeIncidences.remove( (*it)->email(), todo );
    }

    // Delete child-todos.
    if ( !todo->hasRecurrenceId() ) {
      deleteTodoInstances(todo);
    }

    KDateTime nowUTC = KDateTime::currentUtcDateTime();
    todo->setLastModified( nowUTC );

    return true;
  } else {
    kWarning() << "Todo not found.";
    return false;
  }
}

bool ExtendedCalendar::deleteTodoInstances( const Todo::Ptr &todo )
{
  QList<Todo::Ptr> values = d->mTodos.values( todo->uid() );
  QList<Todo::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( (*it)->hasRecurrenceId() ) {
      kDebug() << "deleting child todo" << (*it)->uid()
               << (*it)->dtStart() << (*it)->dtDue()
               << "in calendar";
      deleteTodo( (*it) );
    }
  }

  return true;
}

void ExtendedCalendar::deleteAllTodos()
{
  QHashIterator<QString,Todo::Ptr>i( d->mTodos );
  while ( i.hasNext() ) {
    i.next();
    notifyIncidenceDeleted( i.value() );
    // suppress update notifications for the relation removal triggered
    // by the following deletions
    i.value()->startUpdates();
  }
  d->mTodos.clear();
  d->mTodosForDate.clear();
}

Todo::Ptr ExtendedCalendar::todo( const QString &uid, const KDateTime &recurrenceId ) const
{
  QList<Todo::Ptr> values = d->mTodos.values( uid );
  QList<Todo::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( recurrenceId.isNull() ) {
      if ( !(*it)->hasRecurrenceId() ) {
        return *it;
      }
    } else {
      if ( (*it)->hasRecurrenceId() && (*it)->recurrenceId() == recurrenceId ) {
        return *it;
      }
    }
  }
  return Todo::Ptr();
}

Todo::Ptr ExtendedCalendar::deletedTodo( const QString &uid, const KDateTime &recurrenceId ) const
{
  QList<Todo::Ptr> values = d->mDeletedTodos.values( uid );
  QList<Todo::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( recurrenceId.isNull() ) {
      if ( !(*it)->hasRecurrenceId() ) {
        return *it;
      }
    } else {
      if ( (*it)->hasRecurrenceId() && (*it)->recurrenceId() == recurrenceId ) {
        return *it;
      }
    }
  }
  return Todo::Ptr();
}

Todo::List ExtendedCalendar::rawTodos( TodoSortField sortField, SortDirection sortDirection ) const
{
  Todo::List todoList;
  QHashIterator<QString,Todo::Ptr>i( d->mTodos );
  while ( i.hasNext() ) {
    i.next();
    if ( isVisible( i.value() ) ) {
      todoList.append( i.value() );
    }
  }
  return Calendar::sortTodos( todoList, sortField, sortDirection );
}

Todo::List ExtendedCalendar::deletedTodos( TodoSortField sortField,
                                           SortDirection sortDirection ) const
{
  Todo::List todoList;
  QHashIterator<QString,Todo::Ptr>i( d->mDeletedTodos );
  while ( i.hasNext() ) {
    i.next();
    todoList.append( i.value() );
  }
  return Calendar::sortTodos( todoList, sortField, sortDirection );
}

Todo::List ExtendedCalendar::todoInstances( const Incidence::Ptr &todo, TodoSortField sortField,
                                            SortDirection sortDirection ) const
{
  Todo::List list;

  QList<Todo::Ptr> values = d->mTodos.values( todo->uid() );
  QList<Todo::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( (*it)->hasRecurrenceId() ) {
      list.append(*it);
    }
  }
  return Calendar::sortTodos( list, sortField, sortDirection );
}

Todo::List ExtendedCalendar::rawTodosForDate( const QDate &date ) const
{
  Todo::List todoList;
  Todo::Ptr t;

  KDateTime::Spec ts = timeSpec();
  QString dateStr = date.toString();
  QMultiHash<QString,Todo::Ptr>::const_iterator it = d->mTodosForDate.constFind( dateStr );
  while ( it != d->mTodosForDate.constEnd() && it.key() == dateStr ) {
    t = it.value();
    if ( isVisible( t ) ) {
      todoList.append( t );
    }
    ++it;
  }

  // Iterate over all todos. Look for recurring todoss that occur on this date
  QHashIterator<QString,Todo::Ptr>i( d->mTodos );
  while ( i.hasNext() ) {
    i.next();
    t = i.value();
    if ( isVisible( t ) ) {
      if ( t->recurs() ) {
        if ( t->recursOn( date, ts ) ) {
          if (!todoList.contains( t ) ) {
            todoList.append( t );
          }
        }
      }
    }
  }

  return todoList;
}

Todo::List ExtendedCalendar::rawTodos( const QDate &start, const QDate &end,
                                       const KDateTime::Spec &timespec, bool inclusive ) const
{
  Q_UNUSED( inclusive ); // use only exact dtDue/dtStart, not dtStart and dtEnd

  Todo::List todoList;
  KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();
  KDateTime st( start, ts );
  KDateTime nd( end, ts );

  // Get todos
  QHashIterator<QString,Todo::Ptr>i( d->mTodos );
  Todo::Ptr todo;
  while ( i.hasNext() ) {
    i.next();
    todo = i.value();
    if ( !isVisible( todo ) ) {
      continue;
    }

    KDateTime rStart = todo->hasDueDate() ? todo->dtDue() :
                       todo->hasStartDate() ? todo->dtStart() :
                       KDateTime();
    if ( !rStart.isValid() ) {
      continue;
    }

    if ( !todo->recurs() ) { // non-recurring todos
      if ( nd.isValid() && nd < rStart ) {
        continue;
      }
      if ( st.isValid() && rStart < st ) {
        continue;
      }
    } else { // recurring events
      switch( todo->recurrence()->duration() ) {
      case -1: // infinite
        break;
      case 0: // end date given
      default: // count given
        KDateTime rEnd( todo->recurrence()->endDate(), ts );
        if ( !rEnd.isValid() ) {
          continue;
        }
        if ( st.isValid() && rEnd < st ) {
          continue;
        }
        break;
      } // switch(duration)
    } //if(recurs)

    todoList.append( todo );
  }

  return todoList;
}

Alarm::List ExtendedCalendar::alarmsTo( const KDateTime &to ) const
{
  return alarms( KDateTime( QDate( 1900, 1, 1 ) ), to );
}

Alarm::List ExtendedCalendar::alarms( const KDateTime &from, const KDateTime &to ) const
{
  Alarm::List alarmList;
  QHashIterator<QString,Event::Ptr>ie( d->mEvents );
  Event::Ptr e;
  while ( ie.hasNext() ) {
    ie.next();
    e = ie.value();
    if ( e->recurs() ) {
      appendRecurringAlarms( alarmList, e, from, to );
    } else {
      appendAlarms( alarmList, e, from, to );
    }
  }

  QHashIterator<QString,Todo::Ptr>it( d->mTodos );
  Todo::Ptr t;
  while ( it.hasNext() ) {
    it.next();
    t = it.value();
    if ( !t->isCompleted() ) {
      appendAlarms( alarmList, t, from, to );
    }
  }

  return alarmList;
}

//@cond PRIVATE
void ExtendedCalendar::Private::insertEvent( const Event::Ptr &event,
                                             const KDateTime::Spec &timeSpec )
{
  QString uid = event->uid();
  if ( !mEvents.contains( uid, event ) ) {

    mEvents.insert( uid, event );
    if ( !event->recurs() && !event->isMultiDay() ) {
      mEventsForDate.insert( event->dtStart().toTimeSpec( timeSpec ).date().toString(), event );
    }

    // Insert into attendee events.
    Person ::Ptr organizer = event->organizer();
    if ( !organizer->isEmpty() ) {
      mAttendeeIncidences.insert( organizer->email(), event );
    }
    const Attendee::List &list = event->attendees();
    Attendee::List::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
      mAttendeeIncidences.insert( (*it)->email(), event );
    }
    if ( event->hasGeo() ) {
      mGeoIncidences.append( event );
    }
  } else {
#ifdef NDEBUG
    // if we already have an event with this UID, it must be the same event,
    // otherwise something's really broken
    Q_ASSERT( mEvents.value( uid ) == event );
#endif
  }
}
//@endcond

void ExtendedCalendar::incidenceUpdate( const QString &uid )
{
  // The static_cast is ok as the ExtendedCalendar only observes Incidence objects
  Incidence::Ptr incidence = this->incidence( uid );

  if ( !incidence ) {
    return;
  }

  // Remove attendee incidence.
  Person::Ptr organizer = incidence->organizer();
  if ( !organizer->isEmpty() ) {
    d->mAttendeeIncidences.remove( organizer->email(), incidence );
  }
  const Attendee::List &list = incidence->attendees();
  Attendee::List::ConstIterator it;
  for ( it = list.begin(); it != list.end(); ++it ) {
    d->mAttendeeIncidences.remove( (*it)->email(), incidence );
  }

  if ( incidence->type() == Incidence::TypeEvent ) {
    Event::Ptr event = incidence.staticCast<Event>();
    d->mEvents.remove( event->uid(), event );
    if ( !event->dtStart().isNull() ) { // Not mandatory to have dtStart
      d->mEventsForDate.remove(
        event->dtStart().toTimeSpec( timeSpec() ).date().toString(), event );
    }
    if ( event->hasGeo() ) {
      d->mGeoIncidences.removeAll( event );
    }
  } else if ( incidence->type() == Incidence::TypeTodo ) {
    Todo::Ptr todo = incidence.staticCast<Todo>();
    d->mTodos.remove( todo->uid(), todo );
    if ( todo->hasDueDate() ) {
      d->mTodosForDate.remove( todo->dtDue().toTimeSpec( timeSpec() ).date().toString(), todo );
    } else if ( todo->hasStartDate() ) {
      d->mTodosForDate.remove(
        todo->dtStart().toTimeSpec( timeSpec() ).date().toString(), todo );
    }
    if ( todo->hasGeo() ) {
      d->mGeoIncidences.removeAll( todo );
    }
  } else if ( incidence->type() == Incidence::TypeJournal ) {
    Journal::Ptr journal = incidence.staticCast<Journal>();
    d->mJournals.remove( journal->uid(), journal );
    d->mJournalsForDate.remove(
      journal->dtStart().toTimeSpec( timeSpec() ).date().toString(), journal );
  } else {
    Q_ASSERT( false );
  }
}

void ExtendedCalendar::incidenceUpdated( const QString &uid )
{

  Incidence::Ptr incidence = this->incidence( uid );

  if ( !incidence ) {
    return;
  }

  KDateTime nowUTC = KDateTime::currentUtcDateTime();
  incidence->setLastModified( nowUTC );
  // we should probably update the revision number here,
  // or internally in the Event itself when certain things change.
  // need to verify with ical documentation.

  // Insert into attendee incidences.
  Person::Ptr organizer = incidence->organizer();
  if ( !organizer->isEmpty() ) {
    d->mAttendeeIncidences.insert( organizer->email(), incidence );
  }
  const Attendee::List &list = incidence->attendees();
  Attendee::List::ConstIterator it;
  for ( it = list.begin(); it != list.end(); ++it ) {
    d->mAttendeeIncidences.insert( (*it)->email(), incidence );
  }

  if ( incidence->type() == Incidence::TypeEvent ) {
    Event::Ptr event = incidence.staticCast<Event>();
    d->mEvents.insert( event->uid(), event );
    if ( !event->recurs() && !event->isMultiDay() ) {
      d->mEventsForDate.insert(
        event->dtStart().toTimeSpec( timeSpec() ).date().toString(), event );
    }
    if ( event->hasGeo() ) {
      d->mGeoIncidences.append( event );
    }
  } else if ( incidence->type() == Incidence::TypeTodo ) {
    Todo::Ptr todo = incidence.staticCast<Todo>();
    d->mTodos.insert( todo->uid(), todo );
    if ( todo->hasDueDate() ) {
      d->mTodosForDate.insert(
        todo->dtDue().toTimeSpec( timeSpec() ).date().toString(), todo );
    } else if ( todo->hasStartDate() ) {
      d->mTodosForDate.insert(
        todo->dtStart().toTimeSpec( timeSpec() ).date().toString(), todo );
    }
    if ( todo->hasGeo() ) {
      d->mGeoIncidences.append( todo );
    }
  } else if ( incidence->type() == Incidence::TypeJournal ) {
    Journal::Ptr journal = incidence.staticCast<Journal>();
    d->mJournals.insert( journal->uid(), journal );
    d->mJournalsForDate.insert(
      journal->dtStart().toTimeSpec( timeSpec() ).date().toString(), journal );
  } else {
    Q_ASSERT( false );
  }

  notifyIncidenceChanged( incidence );

  setModified( true );
}

Event::List ExtendedCalendar::rawEventsForDate( const QDate &date,
                                                const KDateTime::Spec &timespec,
                                                EventSortField sortField,
                                                SortDirection sortDirection ) const
{
  Event::List eventList;
  Event::Ptr ev;

  // Find the hash for the specified date
  QString dateStr = date.toString();
  QMultiHash<QString,Event::Ptr>::const_iterator it = d->mEventsForDate.constFind( dateStr );
  // Iterate over all non-recurring, single-day events that start on this date
  KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();
  KDateTime kdt( date, ts );
  while ( it != d->mEventsForDate.constEnd() && it.key() == dateStr ) {
    ev = it.value();
    if ( isVisible( ev ) ) {
      KDateTime end( ev->dtEnd().toTimeSpec( ev->dtStart() ) );
      if ( ev->allDay() ) {
        end.setDateOnly( true );
      }
      //kDebug() << dateStr << kdt << ev->summary() << ev->dtStart() << end;
      if ( end >= kdt ) {
        eventList.append( ev );
      }
    }
    ++it;
  }

  // Iterate over all events. Look for recurring events that occur on this date
  QHashIterator<QString,Event::Ptr>i( d->mEvents );
  while ( i.hasNext() ) {
    i.next();
    ev = i.value();
    if ( isVisible( ev ) ) {
      if ( ev->recurs() ) {
        if ( ev->isMultiDay() ) {
          int extraDays = ev->dtStart().date().daysTo( ev->dtEnd().date() );
          for ( int i = 0; i <= extraDays; ++i ) {
            if ( ev->recursOn( date.addDays( -i ), ts ) ) {
              eventList.append( ev );
              break;
            }
          }
        } else {
          if ( ev->recursOn( date, ts ) ) {
            eventList.append( ev );
          }
        }
      } else {
        if ( ev->isMultiDay() ) {
          if ( ev->dtStart().date() <= date && ev->dtEnd().date() >= date ) {
            eventList.append( ev );
          }
        }
      }
    }
  }

  return Calendar::sortEvents( eventList, sortField, sortDirection );
}

Event::List ExtendedCalendar::rawEvents( const QDate &start, const QDate &end,
                                         const KDateTime::Spec &timespec, bool inclusive ) const
{
  Event::List eventList;
  KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();
  KDateTime st( start, ts );
  KDateTime nd( end, ts );

  // Get non-recurring events
  QHashIterator<QString,Event::Ptr>i( d->mEvents );
  Event::Ptr event;
  while ( i.hasNext() ) {
    i.next();
    event = i.value();
    if ( !isVisible( event ) ) {
      continue;
    }

    KDateTime rStart = event->dtStart();
    if ( nd.isValid() && nd < rStart ) {
      continue;
    }
    if ( inclusive && st.isValid() && rStart < st ) {
      continue;
    }

    if ( !event->recurs() ) { // non-recurring events
      KDateTime rEnd = event->dtEnd();
      if ( st.isValid() && rEnd < st ) {
        continue;
      }
      if ( inclusive && nd.isValid() && nd < rEnd ) {
        continue;
      }
    } else { // recurring events
      switch( event->recurrence()->duration() ) {
      case -1: // infinite
        if ( inclusive ) {
          continue;
        }
        break;
      case 0: // end date given
      default: // count given
        KDateTime rEnd( event->recurrence()->endDate(), ts );
        if ( !rEnd.isValid() ) {
          continue;
        }
        if ( st.isValid() && rEnd < st ) {
          continue;
        }
        if ( inclusive && nd.isValid() && nd < rEnd ) {
          continue;
        }
        break;
      } // switch(duration)
    } //if(recurs)

    eventList.append( event );
  }

  return eventList;
}

Event::List ExtendedCalendar::rawEventsForDate( const KDateTime &kdt ) const
{
  return rawEventsForDate( kdt.date(), kdt.timeSpec() );
}

Event::List ExtendedCalendar::rawEvents( EventSortField sortField,
                                         SortDirection sortDirection ) const
{
  Event::List eventList;
  QHashIterator<QString,Event::Ptr>i( d->mEvents );
  while ( i.hasNext() ) {
    i.next();
    if ( isVisible( i.value() ) ) {
      eventList.append( i.value() );
    }
  }
  return Calendar::sortEvents( eventList, sortField, sortDirection );
}

Event::List ExtendedCalendar::deletedEvents( EventSortField sortField,
                                             SortDirection sortDirection ) const
{
  Event::List eventList;
  QHashIterator<QString,Event::Ptr>i( d->mDeletedEvents );
  while ( i.hasNext() ) {
    i.next();
    eventList.append( i.value() );
  }
  return Calendar::sortEvents( eventList, sortField, sortDirection );
}

Event::List ExtendedCalendar::eventInstances( const Incidence::Ptr &event,
                                              EventSortField sortField,
                                              SortDirection sortDirection ) const
{
  Event::List list;

  QList<Event::Ptr> values = d->mEvents.values( event->uid() );
  QList<Event::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( (*it)->hasRecurrenceId() ) {
      list.append( *it );
    }
  }

  return Calendar::sortEvents( list, sortField, sortDirection );
}

bool ExtendedCalendar::addJournal( const Journal::Ptr &aJournal )
{
  if ( !aJournal ) {
    return false;
  }

  Journal::Ptr journalToAdd = aJournal;

  notifyIncidenceAdded( aJournal );

  if ( d->mJournals.contains( aJournal->uid() ) ) {
    Journal::Ptr old;
    if ( !aJournal->hasRecurrenceId() ) {
      old = journal( aJournal->uid() );
    } else {
      old = journal( aJournal->uid(), aJournal->recurrenceId() );
    }
    if ( old ) {
      if ( aJournal->revision() > old->revision() ) {
        deleteJournal( old ); // move old to deleted
      } else {
        notifyIncidenceAdditionCanceled( aJournal );
        journalToAdd = old;
        return true;
      }
    }
  }

  d->insertJournal( aJournal, timeSpec() );

  journalToAdd->registerObserver( this );

  setModified( true );

  if ( !defaultNotebook().isEmpty() ) {
    return setNotebook( journalToAdd, defaultNotebook() );
  }

  return true;
}

//@cond PRIVATE
void ExtendedCalendar::Private::insertJournal( const Journal::Ptr &journal,
                                               const KDateTime::Spec &timeSpec )
{
  QString uid = journal->uid();
  if ( !mJournals.contains( uid, journal ) ) {
    mJournals.insert( uid, journal );
    mJournalsForDate.insert( journal->dtStart().toTimeSpec(timeSpec).date().toString(), journal );

    // Insert into attendee journals.
    Person::Ptr organizer = journal->organizer();
    if ( !organizer->isEmpty() ) {
      mAttendeeIncidences.insert( organizer->email(), journal );
    }
    const Attendee::List &list = journal->attendees();
    Attendee::List::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
      mAttendeeIncidences.insert( (*it)->email(), journal );
    }
  } else {
#ifndef NDEBUG
    // if we already have an journal with this UID, it must be the same journal,
    // otherwise something's really broken
    Q_ASSERT( mJournals.value( uid ) == journal );
#endif
  }
}
//@endcond

bool ExtendedCalendar::deleteJournal( const Journal::Ptr &journal )
{
  if ( d->mJournals.remove( journal->uid(), journal ) ) {
    journal->unRegisterObserver( this );
    setModified( true );
    notifyIncidenceDeleted( journal );
    d->mDeletedJournals.insert( journal->uid(), journal );

    d->mJournalsForDate.remove(
      journal->dtStart().toTimeSpec( timeSpec() ).date().toString(), journal );

    // Delete from attendee journals.
    Person::Ptr organizer = journal->organizer();
    if ( !organizer->isEmpty() ) {
      d->mAttendeeIncidences.remove( organizer->email(), journal );
    }
    const Attendee::List &list = journal->attendees();
    Attendee::List::ConstIterator it;
    for ( it = list.begin(); it != list.end(); ++it ) {
      d->mAttendeeIncidences.remove( (*it)->email(), journal );
    }

    // Delete child-journals.
    if ( !journal->hasRecurrenceId() ) {
      deleteJournalInstances( journal );
    }

    KDateTime nowUTC = KDateTime::currentUtcDateTime();
    journal->setLastModified( nowUTC );

    return true;
  } else {
    kWarning() << "Journal not found.";
    return false;
  }
}

bool ExtendedCalendar::deleteJournalInstances( const Journal::Ptr &journal )
{
  QList<Journal::Ptr> values = d->mJournals.values( journal->uid() );
  QList<Journal::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( (*it)->hasRecurrenceId() ) {
      kDebug() << "deleting child journal" << (*it)->uid()
               << (*it)->dtStart()
               << "in calendar";
      deleteJournal( (*it) );
    }
  }

  return true;
}

void ExtendedCalendar::deleteAllJournals()
{
  QHashIterator<QString,Journal::Ptr>i( d->mJournals );
  while ( i.hasNext() ) {
    i.next();
    notifyIncidenceDeleted( i.value() );
    // suppress update notifications for the relation removal triggered
    // by the following deletions
    i.value()->startUpdates();
  }
  d->mJournals.clear();
  d->mJournalsForDate.clear();
}

Journal::Ptr ExtendedCalendar::journal( const QString &uid, const KDateTime &recurrenceId ) const
{
  QList<Journal::Ptr> values = d->mJournals.values( uid );
  QList<Journal::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( recurrenceId.isNull() ) {
      if ( !(*it)->hasRecurrenceId() ) {
        return *it;
      }
    } else {
      if ( (*it)->hasRecurrenceId() && (*it)->recurrenceId() == recurrenceId ) {
        return *it;
      }
    }
  }
  return Journal::Ptr();
}

Journal::Ptr ExtendedCalendar::deletedJournal( const QString &uid,
                                               const KDateTime &recurrenceId ) const
{
  QList<Journal::Ptr> values = d->mDeletedJournals.values( uid );
  QList<Journal::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( recurrenceId.isNull() ) {
      if ( !(*it)->hasRecurrenceId() ) {
        return *it;
      }
    } else {
      if ( (*it)->hasRecurrenceId() && (*it)->recurrenceId() == recurrenceId ) {
        return *it;
      }
    }
  }
  return Journal::Ptr();
}

Journal::List ExtendedCalendar::rawJournals( JournalSortField sortField,
                                             SortDirection sortDirection ) const
{
  Journal::List journalList;
  QHashIterator<QString,Journal::Ptr>i( d->mJournals );
  while ( i.hasNext() ) {
    i.next();
    if ( isVisible( i.value() ) ) {
      journalList.append( i.value() );
    }
  }
  return Calendar::sortJournals( journalList, sortField, sortDirection );
}

Journal::List ExtendedCalendar::deletedJournals( JournalSortField sortField,
                                                 SortDirection sortDirection ) const
{
  Journal::List journalList;
  QHashIterator<QString,Journal::Ptr>i( d->mDeletedJournals );
  while ( i.hasNext() ) {
    i.next();
    journalList.append( i.value() );
  }
  return Calendar::sortJournals( journalList, sortField, sortDirection );
}

Journal::List ExtendedCalendar::journalInstances( const Incidence::Ptr &journal,
                                                  JournalSortField sortField,
                                                  SortDirection sortDirection ) const
{
  Journal::List list;

  QList<Journal::Ptr> values = d->mJournals.values( journal->uid() );
  QList<Journal::Ptr>::const_iterator it;
  for ( it = values.constBegin(); it != values.constEnd(); ++it ) {
    if ( (*it)->hasRecurrenceId() ) {
      list.append( *it );
    }
  }
  return Calendar::sortJournals( list, sortField, sortDirection );
}

Journal::List ExtendedCalendar::rawJournalsForDate( const QDate &date ) const
{
  Journal::List journalList;
  Journal::Ptr j;

  QString dateStr = date.toString();
  QMultiHash<QString,Journal::Ptr>::const_iterator it = d->mJournalsForDate.constFind( dateStr );

  while ( it != d->mJournalsForDate.constEnd() && it.key() == dateStr ) {
    j = it.value();
    if ( isVisible( j ) ) {
      journalList.append( j );
    }
    ++it;
  }
  return journalList;
}

Journal::List ExtendedCalendar::rawJournals( const QDate &start, const QDate &end,
                                             const KDateTime::Spec &timespec, bool inclusive ) const
{
  Q_UNUSED( inclusive );
  Journal::List journalList;
  KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();
  KDateTime st( start, ts );
  KDateTime nd( end, ts );

  // Get journals
  QHashIterator<QString,Journal::Ptr>i( d->mJournals );
  Journal::Ptr journal;
  while ( i.hasNext() ) {
    i.next();
    journal = i.value();
    if ( !isVisible( journal ) ) {
      continue;
    }

    KDateTime rStart = journal->dtStart();
    if ( nd.isValid() && nd < rStart ) {
      continue;
    }
    if ( inclusive && st.isValid() && rStart < st ) {
      continue;
    }

    if ( !journal->recurs() ) { // non-recurring journals
      // TODO_ALVARO: journals don't have endDt, bug?
      KDateTime rEnd = journal->dateTime( Incidence::RoleEnd );
      if ( st.isValid() && rEnd < st ) {
        continue;
      }
      if ( inclusive && nd.isValid() && nd < rEnd ) {
        continue;
      }
    } else { // recurring journals
      switch( journal->recurrence()->duration() ) {
      case -1: // infinite
        if ( inclusive ) {
          continue;
        }
        break;
      case 0: // end date given
      default: // count given
        KDateTime rEnd( journal->recurrence()->endDate(), ts );
        if ( !rEnd.isValid() ) {
          continue;
        }
        if ( st.isValid() && rEnd < st ) {
          continue;
        }
        if ( inclusive && nd.isValid() && nd < rEnd ) {
          continue;
        }
        break;
      } // switch(duration)
    } //if(recurs)

    journalList.append( journal );
  }

  return journalList;
}

QStringList ExtendedCalendar::attendees()
{
  return d->mAttendeeIncidences.uniqueKeys();
}

Incidence::List ExtendedCalendar::attendeeIncidences( const QString &email )
{
  return d->mAttendeeIncidences.values( email );
}

Incidence::List ExtendedCalendar::geoIncidences()
{
  return d->mGeoIncidences;
}

Incidence::List ExtendedCalendar::geoIncidences( float geoLatitude, float geoLongitude,
                                                 float diffLatitude, float diffLongitude )
{
  Incidence::List list;

  QList<Incidence::Ptr> values = incidences( QString() );
  QList<Incidence::Ptr>::const_iterator it;
  for ( it = values.begin(); it != values.end(); ++it ) {
    float lat = (*it)->geoLatitude();
    float lon = (*it)->geoLongitude();

    if ( fabs( lat - geoLatitude ) <= diffLatitude &&
         fabs( lon - geoLongitude ) <= diffLongitude ) {
      list.append( *it );
    }
  }
  return list;
}

void ExtendedCalendar::deleteAllIncidences()
{
  deleteAllEvents();
  deleteAllTodos();
  deleteAllJournals();
}

Incidence::List ExtendedCalendar::sortIncidences( Incidence::List *incidenceList,
                                                  IncidenceSortField sortField,
                                                  SortDirection sortDirection )
{
  Incidence::List incidenceListSorted;
  Incidence::List tempList;
  Incidence::List::Iterator sortIt;
  Incidence::List::Iterator iit;

  switch ( sortField ) {
  case IncidenceSortUnsorted:
    incidenceListSorted = *incidenceList;
    break;

  case IncidenceSortDate:
    incidenceListSorted = *incidenceList;
    if ( sortDirection == SortDirectionAscending ) {
      qSort( incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::dateLessThan );
    } else {
      qSort( incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::dateMoreThan );
    }
    break;

  case IncidenceSortCreated:
    incidenceListSorted = *incidenceList;
    if ( sortDirection == SortDirectionAscending ) {
      qSort( incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::createdLessThan );
    } else {
      qSort( incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::createdMoreThan );
    }
    break;

  }
  return incidenceListSorted;
}

static bool expandedIncidenceSortLessThan( const ExtendedCalendar::ExpandedIncidence &e1,
                                           const ExtendedCalendar::ExpandedIncidence &e2 )
{
  if ( e1.first < e2.first ) {
    return true;
  }
  if ( e1.first > e2.first ) {
    return false;
  }
  // e1 == e2 => perform secondary check based on uuid (hopefully this
  // brings us consistent sorting that doesn't change order randomly
  return e1.second->uid() < e2.second->uid();
}

ExtendedCalendar::ExpandedIncidenceList ExtendedCalendar::expandRecurrences(
  Incidence::List *incidenceList, const KDateTime &dtStart, const KDateTime &dtEnd, int maxExpand )
{
  ExtendedCalendar::ExpandedIncidenceList returnList;
  Incidence::List::Iterator iit;

  for ( iit = incidenceList->begin(); iit != incidenceList->end(); ++iit ) {
    KDateTime dt = (*iit)->dtStart();
// PENDING(kdab) Review
#ifdef KDAB_TEMPORARILY_REMOVED
    KDateTime dte = (*iit)->dtEnd();
#else
    KDateTime dte = (*iit)->dateTime( IncidenceBase::RoleEndRecurrenceBase );
#endif
    int appended = 0;

    if ( (*iit)->type() == Incidence::TypeTodo ) {
      Todo::Ptr todo = (*iit).staticCast<Todo>();
      if ( todo->hasDueDate() ) {
        dt = todo->dtDue();
      }
    }

    if ( !dt.isValid() ) {
      // Just leave the dateless incidences there (they will be
      // sorted out)
      returnList.append( ExpandedIncidence( dt.toLocalZone().dateTime(), *iit ) );
      continue;
    }

    // Fix the non-valid dte to be dt+1
    if ( dte.isValid() && dte <= dt ) {
      dte = dt.addSecs( 1 );
    }

    // Then insert the current; only if it (partially) fits within
    // the [dtStart, dtEnd[ window. (note that dtEnd is not really
    // included; similarly, the last second of events is not
    // counted as valid. This is because (for example) all-day
    // events in ical are typically stored as whole day+1 events
    // (that is, the first second of next day is where it ends),
    // and due to that otherwise date-specific queries won't work
    // nicely.

    // Mandatory conditions:
    // [1] dt < dtEnd <> start period early enough iff dtEnd specified
    // [2] dte > dtStart <> end period late enough iff dte set

    // Note: This algorithm implies that events that are only
    // partially within the desired [dtStart, dtEnd] range are
    // also included.

    if ( ( !dtEnd.isValid() || dt < dtEnd ) && ( !dte.isValid() || dte > dtStart ) ) {
      kDebug() << "---appending" << (*iit)->summary() << dt;
      returnList.append( ExpandedIncidence( dt.toLocalZone().dateTime(), *iit ) );
      appended++;
    }

    if ( (*iit)->recurs() ) {
      KDateTime dtr = dt, dtr2;
      KDateTime dtre;

      // If the original entry wasn't part of the time window, try to get more
      // appropriate first item to add. Else, start the next-iteration from the 'dt'
      // (=current item).
      if ( !appended ) {
        dtr = (*iit)->recurrence()->getPreviousDateTime( dtStart );
        if ( dtr.isValid() ) {
          dtr2 = (*iit)->recurrence()->getPreviousDateTime( dtr );
          if ( dtr2.isValid() ) {
            dtr = dtr2;
          }
        } else {
          dtr = dt;
        }
      }

      int duration = 0;
      if ( dte.isValid() ) {
        duration = dte.toTime_t() - dt.toTime_t();
        if ( duration < 1 ) {
          duration = 1;
        }
      }
      while ( appended < maxExpand ) {
        dtr = (*iit)->recurrence()->getNextDateTime( dtr );
        if ( !dtr.isValid() || ( dtEnd.isValid() && dtr >= dtEnd ) ) {
          break;
        }

        // Calculate the end time for this repetition
        dtre = dtr;

        if ( duration ) {
          dtre = dtr.addSecs( duration );
        }

        // As incidences are in sorted order, the [1] condition was
        // already met as we're still iterating. Have to check [2].
        if ( !dtre.isValid() || dtre > dtStart ) {
          kDebug() << "---appending(recurrence)" << (*iit)->summary() << dtr;
          returnList.append( ExpandedIncidence( dtr.toLocalZone().dateTime(), *iit ) );
          appended++;
        }
      }
    }
  }
  qSort( returnList.begin(), returnList.end(), expandedIncidenceSortLessThan );
  return returnList;
}

Incidence::List ExtendedCalendar::incidences( const QDate &start, const QDate &end )
{
  return mergeIncidenceList( events( start, end ), todos( start, end ), journals( start ) );
}

ExtendedStorage *ExtendedCalendar::defaultStorage()
{
  // This is the place where we configure our default backend
  QString dbFile = QLatin1String( qgetenv( "SQLITESTORAGEDB" ) );
  if ( dbFile.isEmpty() ) {
    dbFile = QDir::homePath() + QLatin1String( "/.calendardb" );
  }

  SqliteStorage *ss = new SqliteStorage( ExtendedCalendar::Ptr( this ), dbFile, true );

  return ss;
}

Todo::List ExtendedCalendar::uncompletedTodos( bool hasDate, int hasGeo )
{
  Todo::List list;

  QHashIterator<QString,Todo::Ptr>i( d->mTodos );
  while ( i.hasNext() ) {
    i.next();
    Todo::Ptr todo = i.value();
    if ( !todo->isCompleted() ) {
      if ( ( hasDate && todo->hasDueDate() ) || ( !hasDate && !todo->hasDueDate() ) ) {
        if ( hasGeo < 0 || ( hasGeo && todo->hasGeo() ) || ( !hasGeo && !todo->hasGeo() ) ) {
          list.append( todo );
        }
      }
    }
  }
  return list;
}

Todo::List ExtendedCalendar::completedTodos( bool hasDate, int hasGeo,
                                             const KDateTime &start, const KDateTime &end )
{
  Todo::List list;

  QHashIterator<QString,Todo::Ptr>i( d->mTodos );
  while ( i.hasNext() ) {
    i.next();
    Todo::Ptr todo = i.value();
    if ( todo->isCompleted() ) {
      if ( hasDate && todo->hasDueDate() ) {
        if ( hasGeo < 0 || ( hasGeo && todo->hasGeo() ) || ( !hasGeo && !todo->hasGeo() ) ) {
          if ( !todo->recurs() ) { // non-recurring todos
            if ( ( !start.isValid() || start <= todo->dtDue() ) &&
                 ( !end.isValid() || end >= todo->dtDue() ) ) {
              list.append( todo );
            }
          } else { // recurring todos
            switch( todo->recurrence()->duration() ) {
            case -1: // infinite
              list.append( todo );
              break;
            case 0: // end date given
            default: // count given
              KDateTime rEnd = todo->recurrence()->endDateTime();
              if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
                // append if last recurrence is smaller than given start
                // this is not perfect as there may not be any occurrences
                // inside given start and end, but this is fast to check
                list.append( todo );
              }
              break;
            }
          }
        }
      } else if ( !hasDate && !todo->hasDueDate() ) { // todos without due date
        if ( hasGeo < 0 || ( hasGeo && todo->hasGeo() ) || ( !hasGeo && !todo->hasGeo() ) ) {
          if ( ( !start.isValid() || start <= todo->created() ) &&
               ( !end.isValid() || end >= todo->created() ) ) {
            list.append( todo );
          }
        }
      }
    }
  }
  return list;
}

Incidence::List ExtendedCalendar::incidences( bool hasDate,
                                              const KDateTime &start, const KDateTime &end )
{
  Incidence::List list;

  QHashIterator<QString,Todo::Ptr>i1( d->mTodos );
  while ( i1.hasNext() ) {
    i1.next();
    Todo::Ptr todo = i1.value();
    if ( hasDate && todo->hasDueDate() ) { // todos with due date
      if ( !todo->recurs() ) { // non-recurring todos
        if ( ( !start.isValid() || start <= todo->dtDue() ) &&
             ( !end.isValid() || end >= todo->dtDue() ) ) {
          list.append( todo );
        }
      } else { // recurring todos
        switch( todo->recurrence()->duration() ) {
        case -1: // infinite
          list.append( todo );
          break;
        case 0: // end date given
        default: // count given
          KDateTime rEnd = todo->recurrence()->endDateTime();
          if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
            // append if last recurrence is smaller than given start
            // this is not perfect as there may not be any occurrences
            // inside given start and end, but this is fast to check
            list.append( todo );
          }
          break;
        }
      }
    } else if ( !hasDate && !todo->hasDueDate() ) { // todos without due date
      if ( ( !start.isValid() || start <= todo->created() ) &&
           ( !end.isValid() || end >= todo->created() ) ) {
        list.append( todo );
      }
    }
  }
  QHashIterator<QString,Event::Ptr>i2( d->mEvents );
  while ( i2.hasNext() ) {
    i2.next();
    Event::Ptr event = i2.value();
    if ( hasDate && // events with end and start dates
         event->dtStart().isValid() && event->dtEnd().isValid() ) {
      if ( !event->recurs() ) { // non-recurring events
        if ( ( !start.isValid() || start <= event->dtEnd() ) &&
             ( !end.isValid() || end >= event->dtStart() ) ) {
          list.append( event );
        }
      } else { // recurring events
        switch( event->recurrence()->duration() ) {
        case -1: // infinite
          list.append( event );
          break;
        case 0: // end date given
        default: // count given
          KDateTime rEnd = event->recurrence()->endDateTime();
          if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
            // append if last recurrence is smaller than given start
            // this is not perfect as there may not be any occurrences
            // inside given start and end, but this is fast to check
            list.append( event );
          }
          break;
        }
      }
    } else if ( !hasDate && // events without valid dates
                ( !event->dtStart().isValid() || !event->dtEnd().isValid() ) ) {
      if ( ( !start.isValid() || start <= event->created() ) &&
           ( !end.isValid() || end >= event->created() ) ) {
        list.append( event );
      }
    }
  }
  QHashIterator<QString,Journal::Ptr>i3( d->mJournals );
  while ( i3.hasNext() ) {
    i3.next();
    Journal::Ptr journal = i3.value();
    if ( hasDate && // journals with end and start dates
// PENDING(kdab) Review
#ifdef KDAB_TEMPORARILY_REMOVED
         journal->dtStart().isValid() && journal->dtEnd().isValid() ) {
#else
         journal->dtStart().isValid() ) {
#endif
      if ( !journal->recurs() ) { // non-recurring journals
// PENDING(kdab) Review
#ifdef KDAB_TEMPORARILY_REMOVED
        if ( ( !start.isValid() || start <= journal->dtEnd() ) &&
             ( !end.isValid() || end >= journal->dtStart() ) ) {
#else
        if ( !start.isValid() ||
             ( !end.isValid() || end >= journal->dtStart() ) ) {
#endif
          list.append( journal );
        }
      } else { // recurring journals
        switch( journal->recurrence()->duration() ) {
        case -1: // infinite
          list.append( journal );
          break;
        case 0: // end date given
        default: // count given
          KDateTime rEnd = journal->recurrence()->endDateTime();
          if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
            // append if last recurrence is smaller than given start
            // this is not perfect as there may not be any occurrences
            // inside given start and end, but this is fast to check
            list.append( journal );
          }
          break;
        }
      }
    } else if ( !hasDate && // journals without valid dates
// PENDING(kdab) Review
#ifdef KDAB_TEMPORARILY_REMOVED
                ( !journal->dtStart().isValid() || !journal->dtEnd().isValid() ) ) {
#else
                !journal->dtStart().isValid() ) {
#endif
      if ( ( !start.isValid() || start <= journal->created() ) &&
           ( !end.isValid() || end >= journal->created() ) ) {
        list.append( journal );
      }
    }
  }
  return list;
}

Incidence::List ExtendedCalendar::geoIncidences( bool hasDate,
                                                 const KDateTime &start, const KDateTime &end )
{
  Incidence::List list;

  QHashIterator<QString,Todo::Ptr>i1( d->mTodos );
  while ( i1.hasNext() ) {
    i1.next();
    Todo::Ptr todo = i1.value();
    if ( todo->hasGeo() ) {
      if ( hasDate && todo->hasDueDate() ) { // todos with due date
        if ( !todo->recurs() ) { // non-recurring todos
          if ( ( !start.isValid() || start <= todo->dtDue() ) &&
               ( !end.isValid() || end >= todo->dtDue() ) ) {
            list.append( todo );
          }
        } else { // recurring todos
          switch( todo->recurrence()->duration() ) {
          case -1: // infinite
            list.append( todo );
            break;
          case 0: // end date given
          default: // count given
            KDateTime rEnd = todo->recurrence()->endDateTime();
            if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
              // append if last recurrence is smaller than given start
              // this is not perfect as there may not be any occurrences
              // inside given start and end, but this is fast to check
              list.append( todo );
            }
            break;
          }
        }
      } else if ( !hasDate && !todo->hasDueDate() ) { // todos without due date
        if ( ( !start.isValid() || start <= todo->created() ) &&
             ( !end.isValid() || end >= todo->created() ) ) {
          list.append( todo );
        }
      }
    }
  }
  QHashIterator<QString,Event::Ptr>i2( d->mEvents );
  while ( i2.hasNext() ) {
    i2.next();
    Event::Ptr event = i2.value();
    if ( event->hasGeo() ) {
      if ( hasDate && // events with end and start dates
           event->dtStart().isValid() && event->dtEnd().isValid() ) {
        if ( !event->recurs() ) { // non-recurring events
          if ( ( !start.isValid() || start <= event->dtEnd() ) &&
               ( !end.isValid() || end >= event->dtStart() ) ) {
            list.append( event );
          }
        } else { // recurring events
          switch( event->recurrence()->duration() ) {
          case -1: // infinite
            list.append( event );
            break;
          case 0: // end date given
          default: // count given
            KDateTime rEnd = event->recurrence()->endDateTime();
            if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
              // append if last recurrence is smaller than given start
              // this is not perfect as there may not be any occurrences
              // inside given start and end, but this is fast to check
              list.append( event );
            }
            break;
          }
        }
      } else if ( !hasDate && // events without valid dates
                  ( !event->dtStart().isValid() || !event->dtEnd().isValid() ) ) {
        if ( ( !start.isValid() || start <= event->created() ) &&
             ( !end.isValid() || end >= event->created() ) ) {
          list.append( event );
        }
      }
    }
  }
  return list;
}

#if 0
Incidence::List ExtendedCalendar::unreadInvitationIncidences( Person *person )
{
  Incidence::List list;

  QHashIterator<QString,Todo::Ptr>i1( d->mTodos );
  while ( i1.hasNext() ) {
    i1.next();
    Todo::Ptr todo = i1.value();
    if ( todo->invitationStatus() == IncidenceBase::StatusUnread ) {
      if ( !person || person->email() == todo->organizer().email() ||
           todo->attendeeByMail( person->email() ) ) {
        list.append( todo );
      }
    }
  }
  QHashIterator<QString,Event::Ptr>i2( d->mEvents );
  while ( i2.hasNext() ) {
    i2.next();
    Event::Ptr event = i2.value();
    if ( event->invitationStatus() == IncidenceBase::StatusUnread ) {
      if ( !person || person->email() == event->organizer().email() ||
           event->attendeeByMail( person->email() ) ) {
        list.append( event );
      }
    }
  }
  QHashIterator<QString,Journal::Ptr>i3( d->mJournals );
  while ( i3.hasNext() ) {
    i3.next();
    Journal::Ptr journal = i3.value();
    if ( journal->invitationStatus() == IncidenceBase::StatusUnread ) {
      if ( person || person->email() == journal->organizer().email() ||
           journal->attendeeByMail( person->email() ) ) {
        list.append( journal );
      }
    }
  }
  return list;
}

Incidence::List ExtendedCalendar::oldInvitationIncidences( const KDateTime &start,
                                                           const KDateTime &end )
{
  Incidence::List list;

  QHashIterator<QString,Todo::Ptr>i1( d->mTodos );
  while ( i1.hasNext() ) {
    i1.next();
    Todo::Ptr todo = i1.value();
    if ( todo->invitationStatus() > IncidenceBase::StatusUnread ) {
      if ( ( !start.isValid() || start <= todo->created() ) &&
           ( !end.isValid() || end >= todo->created() ) ) {
        list.append( todo );
      }
    }
  }
  QHashIterator<QString,Event::Ptr>i2( d->mEvents );
  while ( i2.hasNext() ) {
    i2.next();
    Event::Ptr *event = i2.value();
    if ( event->invitationStatus() > IncidenceBase::StatusUnread ) {
      if ( ( !start.isValid() || start <= event->created() ) &&
           ( !end.isValid() || end >= event->created() ) ) {
        list.append( event );
      }
    }
  }
  QHashIterator<QString,Journal::Ptr>i3( d->mJournals );
  while ( i3.hasNext() ) {
    i3.next();
    Journal::Ptr journal = i3.value();
    if ( journal->invitationStatus() > IncidenceBase::StatusUnread ) {
      if ( ( !start.isValid() || start <= journal->created() ) &&
           ( !end.isValid() || end >= journal->created() ) ) {
        list.append( journal );
      }
    }
  }
  return list;
}
#endif

Incidence::List ExtendedCalendar::contactIncidences( const Person::Ptr &person,
                                                     const KDateTime &start, const KDateTime &end )
{
  Incidence::List list;
  Incidence::List::Iterator it;
  Incidence::List values = d->mAttendeeIncidences.values( person->email() );
  for ( it = values.begin(); it != values.end(); ++it ) {
    Incidence::Ptr incidence = *it;
    if ( incidence->type() == Incidence::TypeEvent ) {
      Event::Ptr event = incidence.staticCast<Event>();
      if ( event->dtStart().isValid() && event->dtEnd().isValid() ) {
        if ( !event->recurs() ) { // non-recurring events
          if ( ( !start.isValid() || start <= event->dtEnd() ) &&
               ( !end.isValid() || end >= event->dtStart() ) ) {
            list.append( event );
          }
        } else { // recurring events
          switch( event->recurrence()->duration() ) {
          case -1: // infinite
            list.append( event );
            break;
          case 0: // end date given
          default: // count given
            KDateTime rEnd = event->recurrence()->endDateTime();
            if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
              // append if last recurrence is smaller than given start
              // this is not perfect as there may not be any occurrences
              // inside given start and end, but this is fast to check
              list.append( event );
            }
            break;
          }
        }
      } else {
        if ( ( !start.isValid() || start <= event->created() ) &&
             ( !end.isValid() || end >= event->created() ) ) {
          list.append( event );
        }
      }
    } else if ( incidence->type() == Incidence::TypeTodo ) {
      Todo::Ptr todo = incidence.staticCast<Todo>();
      if ( todo->hasDueDate() ) {
        if ( !todo->recurs() ) { // non-recurring todos
          if ( ( !start.isValid() || start <= todo->dtDue() ) &&
               ( !end.isValid() || end >= todo->dtDue() ) ) {
            list.append( todo );
          }
        } else { // recurring todos
          switch( todo->recurrence()->duration() ) {
          case -1: // infinite
            list.append( todo );
            break;
          case 0: // end date given
          default: // count given
            KDateTime rEnd = todo->recurrence()->endDateTime();
            if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
              // append if last recurrence is smaller than given start
              // this is not perfect as there may not be any occurrences
              // inside given start and end, but this is fast to check
              list.append( todo );
            }
            break;
          }
        }
      } else {
        if ( ( !start.isValid() || start <= todo->created() ) &&
             ( !end.isValid() || end >= todo->created() ) ) {
          list.append( todo );
        }
      }
    } else if ( incidence->type() == Incidence::TypeJournal ) {
      Journal::Ptr journal = incidence.staticCast<Journal>();
      if ( journal->dtStart().isValid() ) {
        if ( !journal->recurs() ) { // non-recurring journals
          if ( ( !start.isValid() || start <= journal->dtStart() ) &&
               ( !end.isValid() || end >= journal->dtStart() ) ) {
            list.append( journal );
          }
        } else { // recurring journals
          switch( journal->recurrence()->duration() ) {
          case -1: // infinite
            list.append( journal );
            break;
          case 0: // end date given
          default: // count given
            KDateTime rEnd = journal->recurrence()->endDateTime();
            if ( rEnd.isValid() && ( !start.isValid() || start <= rEnd ) ) {
              // append if last recurrence is smaller than given start
              // this is not perfect as there may not be any occurrences
              // inside given start and end, but this is fast to check
              list.append( journal );
            }
            break;
          }
        }
      } else {
        if ( ( !start.isValid() || start <= journal->created() ) &&
             ( !end.isValid() || end >= journal->created() ) ) {
          list.append( journal );
        }
      }
    }
  }
  return list;
}

Incidence::List ExtendedCalendar::addIncidences( Incidence::List *incidenceList,
                                                 const QString &notebookUid,
                                                 bool duplicateRemovalEnabled )
{
    Incidence::List returnList;
    Incidence::List duplicatesList;
    Incidence::List::Iterator iit;
    Incidence::List::Iterator dit;

    for ( iit = incidenceList->begin(); iit != incidenceList->end(); ++iit ) {
      duplicatesList = duplicates( *iit );
      if ( !duplicatesList.isEmpty() ) {
        if ( duplicateRemovalEnabled ) {
          for ( dit = duplicatesList.begin(); dit != duplicatesList.end(); ++dit ) {
            deleteIncidence( *dit );
          }
        } else {
          continue;
        }
      }

      addIncidence( *iit );
      setNotebook( *iit, notebookUid );
      returnList.append( *iit );
    }

    return returnList;
}

void ExtendedCalendar::storageModified( ExtendedStorage *storage, const QString &info )
{
    Q_UNUSED( storage );
    Q_UNUSED( info );

    // Despite the strange name, close() method does exactly what we
    // want - clears the in-memory contents of the calendar.
    close();
}