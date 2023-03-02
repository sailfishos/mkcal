/*
  This file is part of the mkcal library.

  Copyright (c) 1998 Preston Brown <pbrown@kde.org>
  Copyright (c) 2001,2003 Cornelius Schumacher <schumacher@kde.org>
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
  defines the ExtendedCalendar class.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Preston Brown \<pbrown@kde.org\>
  @author Cornelius Schumacher \<schumacher@kde.org\>
 */

/**
  @mainpage

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>

  @section copyright Copyright

  Copyright (c) 1998 Preston Brown <pbrown@kde.org>

  Copyright (c) 2001,2003 Cornelius Schumacher <schumacher@kde.org>

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

  @section introduction Introduction

  mKCal library extends KCalendarCore library to add a persistant storage
  in a sqlite database.

  This two libraries come from the original KCal from KDE. They have been
  split refactored, and some functionality has been added.

  @section architecture Architecture

  There are two important base classes.
  <ul>
  <li>Abstract class <b>Calendar</b> from KCalendarCore contain incidences
  (events, todos, journals) and methods for adding/deleting/querying them.
  It is implemented by <b>ExtendedCalendar</b> in mKCal.

  <li>Abstract class <b>ExtendedStorage</b> defines the interface to load
  and save calendar incidences into permanent storage. It is implemented
  by a few diffent storages, like <b>SqliteStorage</b> which is the default
  MeeGo storage. But more can be added implementing this interface.
  </ul>

  Calendars can exist without storages but storages must always be linked
  to a calendar. The normal way to create calendar and default storage is:
  @code
  ExtendedCalendar::Ptr cal =
    ExtendedCalendar::Ptr ( new ExtendedCalendar( QLatin1String( "UTC" ) ) );
  ExtendedStorage::Ptr storage = calendar.defaultStorage( cal );
  storage->open();
  @endcode

  Initially calendar is empty unless you create new incidences into it,
  or use the storage to load incidences into the calendar.
  Original KCal provided only means for loading all incidences at once,
  but that may be too costly for an embedded device (loading could take
  tens of seconds for very large calendars). ExtendedStorage added
  methods for smart loading only those incidences that must be shown
  to the user at that time.

  When you add incidences to the calendar, or modify/delete existing
  ones, you must call storage->save() to write incidences into persistent
  storage. Calendar methods always operate in memory only, you can make
  a series of changes in a calendar and then 'commit' them all at once
  by calling storage->save().

  Another extension to original KCal are multiple notebooks (calendars
  for the user). Every incidence is assigned to a notebook.
  The correct way to create an incidence (simple todo in this case) is this:
  @code
  QDateTime now = QDateTime::currentDateTime();
  Notebook::Ptr personal( "Personal" );

  Todo::Ptr todo = Todo::Ptr( new Todo() );
  todo->setSummary( "A todo for testing" );
  todo->setDtDue( now.addDays(1), true );
  todo->setHasDueDate( true );
  calendar.addTodo( todo, personal );
  storage.save();
  @endcode

  Notice that we first create the incidence, then add it to the calendar
  and then tell calendar which notebook the incidence belongs to. Finally
  the note is saved to the storage.

  Notebooks are created/modified/deleted through storages.
  Notebooks are referred by their name only in the calendar.
  To create it, see this example:
  @code
  storage.addNotebook( notebook );
  @endcode

  @section environment Environment

  Here is an ascii art diagram of the typical organiser environment.

  There are usually two processes using libmkcal, one is the calendar
  application visible to the user, and the other is synchronization daemon
  that updates calendar data periodically on the background.

  Change notifications (through the observer classes) are sent when incidences
  are saved in either process.
  Also during save storage sets possible alarms to alarm daemon (if compiled
  with TIMED_SUPPORT) which sets an alarm to be shown by other reminder service.
  Based on user action the alarm may be canceled, snoozed, or opened to the user
  in calendar application.

  @code
       +---------------+                                         +---------------+
       |               |                                         |               |
  +--->| Calendar  UI  |                                         |  Sync Service |
  |    |               |                                         |               |
  |    =================            +---------------+            =================
  |    |               |---save---->|               |<--save-----|               |
  |    |libmkcal       |<--load-----|ExtendedStorage|---load---->|   libmkcal    |
  |    |               |<--changed--|               |---changed->|               |
  |    =================            +---------------+            =================
  |    |    libtimed   |                                         |    libtimed   |
  |    +---------------+                                         +---------------+
  |           |                                                       |
  |           |                                                       |
  |           |                    +--------------+                   |
  |           +----sets alarms---->|              |<----sets alarms---+
  +--------triggers alarms---------| Alarm Daemon |
              +----user action---->|              |---display dialog--+
              |                    +--------------+                   |
              |                                                       |
              |                    +--------------+                   |
              +--------------------| Alarm Dialog |<------------------+
                                   +--------------+
  @endcode

  @section Correct usage
  When one incidence is added into a calendar, every time one property is modified
  all the observers are notfied of the change. So in case lots of modifications are
  being done it can be a very expensive operation.
  The correct way to avoid is
  @code
  incidence->startUpdates();
  //do modifications here
  incidence->endUpdates();
  @code
  Also the call calendar->load() is a very expensive operation. Use it with care and only
  load a notebook, a date range, etc.

  Calling the storage->save() is also an expensive operation. So if adding lots of incidences
  do not call save every time.

 */
#ifndef MKCAL_EXTENDEDCALENDAR_H
#define MKCAL_EXTENDEDCALENDAR_H

#include "mkcal_export.h"

#include <KCalendarCore/MemoryCalendar>

namespace mKCal {

class ExtendedStorage;
class Notebook;

/**
  @brief
  This class provides a calendar cached into memory.
*/
class MKCAL_EXPORT ExtendedCalendar : public KCalendarCore::MemoryCalendar
{
public:
    /**
      A shared pointer to a ExtendedCalendar
    */
    typedef QSharedPointer<ExtendedCalendar> Ptr;

    /**
      @copydoc
      Calendar::Calendar(const QTimeZone &)
    */
    explicit ExtendedCalendar(const QTimeZone &timeZone);

    /**
      @copydoc
      Calendar::Calendar(const QByteArray &)
    */
    explicit ExtendedCalendar(const QByteArray &timeZoneId);

    /**
      @copydoc
      Calendar::~Calendar()
     */
    ~ExtendedCalendar();

    /**
      @copydoc
      Calendar::reload()
    */
    bool reload();

    /**
      @copydoc
      Calendar::save()
    */
    bool save();

    /**
      Dissociate only one single Incidence from a recurring Incidence.
      Incidence for the specified @a date will be dissociated and returned.

      @param incidence is a pointer to a recurring Incidence.
      @param dateTime is the QDateTime within the recurring Incidence on which
      the dissociation will be performed.
      @param spec is the spec in which the @a dateTime is formulated.
      @return a pointer to a dissociated Incidence
    */
    KCalendarCore::Incidence::Ptr dissociateSingleOccurrence(const KCalendarCore::Incidence::Ptr &incidence,
                                                             const QDateTime &dateTime);

    /**
      @copydoc
      Calendar::addIncidence()
    */
    bool addIncidence(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::addIncidence()

      @param notebookUid The notebook uid where you want to add the incidence to.

      @warning There is no check if the notebookUid is valid or not. If it is not
      valid you can corrupt the DB. Check before with storage::isValidNotebook()
    */
    bool addIncidence(const KCalendarCore::Incidence::Ptr &incidence, const QString &notebookUid);

    /**
      @copydoc
      Calendar::deleteIncidence()
    */
    bool deleteIncidence(const KCalendarCore::Incidence::Ptr &incidence);

    // Event Specific Methods //

    /**
      @copydoc
      Calendar::addEvent()
    */
    bool addEvent(const KCalendarCore::Event::Ptr &event);

    /**
      @copydoc
      Calendar::addEvent()

      @param notebookUid The notebook uid where you want to add the event to.

      @warning There is now check if the notebookUid is valid or not. If it is not
      valid you can corrupt the DB. Check before with storage::isValidNotebook()
    */
    bool addEvent(const KCalendarCore::Event::Ptr &event, const QString &notebookUid);

    /**
      @copydoc
      Calendar::deleteEvent()

      @warning This call deletes based on the pointer given, and it is using QSharedPointer
      so if you have to Calendars with the same event, the pointer isn't the same for
      both. The deleting in the second one will fail.
    */
    bool deleteEvent(const KCalendarCore::Event::Ptr &event);

    // To-do Specific Methods //

    /**
      @copydoc
      Calendar::addTodo()
    */
    bool addTodo(const KCalendarCore::Todo::Ptr &todo);

    /**
      @copydoc
      Calendar::addTodo()

      @param notebookUid The notebook uid where you want to add the Todo to.

      @warning There is now check if the notebookUid is valid or not. If it is not
      valid you can corrupt the DB. Check before with storage::isValidNotebook()
    */
    bool addTodo(const KCalendarCore::Todo::Ptr &todo, const QString &notebookUid);

    /**
      @copydoc
      Calendar::deleteTodo()
      @warning This call deletes based on the pointer given, and it is using QSharedPointer
      so if you have to Calendars with the same event, the pointer isn't the same for
      both. The deleting in the second one will fail.
    */
    bool deleteTodo(const KCalendarCore::Todo::Ptr &todo);

    // Journal Specific Methods //

    /**
      @copydoc
      Calendar::addJournal()
    */
    bool addJournal(const KCalendarCore::Journal::Ptr &journal);

    /**
      @copydoc
      Calendar::addJournal()

      @param notebookUid The notebook uid where you want to add the Journal to.

      @warning There is now check if the notebookUid is valid or not. If it is not
      valid you can corrupt the DB. Check before with storage::isValidNotebook()
    */
    bool addJournal(const KCalendarCore::Journal::Ptr &journal, const QString &notebookUid);

    /**
      @copydoc
      Calendar::deleteJournal()
      @warning This call deletes based on the pointer given, and it is using QSharedPointer
      so if you have to Calendars with the same event, the pointer isn't the same for
      both. The deleting in the second one will fail.
    */
    bool deleteJournal(const KCalendarCore::Journal::Ptr &journal);

    using KCalendarCore::Calendar::rawJournals;
    /**
      Returns an unfiltered list of all Journals occurring within a date range.

      @param start is the starting date
      @param end is the ending date
      @param timespec time zone etc. to interpret @p start and @p end,
                      or the calendar's default time spec if none is specified
      @param inclusive if true only Journals which are completely included
      within the date range are returned.

      @return the list of unfiltered Journals occurring within the specified
      date range.
    */
    KCalendarCore::Journal::List rawJournals(
        const QDate &start, const QDate &end,
        const QTimeZone &timespec = QTimeZone(),
        bool inclusive = false) const;

    /**
      Returns a filtered list of all Incidences which occur on the given date.

      @param date request filtered Incidence list for this QDate only.
      @param types request filtered Incidence list for these types only.

      @return the list of filtered Incidences occurring on the specified date.
    */
    virtual KCalendarCore::Incidence::List incidences(const QDate &date,
                                                 const QList<KCalendarCore::Incidence::IncidenceType> &types);

    /**
      Delete all incidences from the memory cache. They will be deleted from
      database when save is called.
    */
    void deleteAllIncidences();

    using KCalendarCore::Calendar::incidences;

    /**
      Returns a filtered list of all Incidences occurring within a date range.

      @param start is the starting date
      @param end is the ending date

      @return the list of filtered Incidences occurring within the specified
      date range.
    */
    KCalendarCore::Incidence::List incidences(const QDate &start, const QDate &end);

    /**
      Creates the default Storage Object used in Maemo.
      The Storage is already linked to this calendar object.

      @param The parent calendar that you want to use with the storage
      @return Object used as storage in Maemo. It should be transparent
      to the user which type it is.
      @warning A new storage is created with each call.
    */
    static QSharedPointer<ExtendedStorage> defaultStorage(const ExtendedCalendar::Ptr
                                                          &calendar);   //No typedef to avoid cyclic includes

    using KCalendarCore::Calendar::journals;

    /**
      Get journals between given times.

      @param start start datetime
      @param end end datetime
      @return list of journals
    */
    KCalendarCore::Journal::List journals(const QDate &start, const QDate &end);

    /**
          Return the count of event incidences.

          @param notebookUid is uid of a notebook for which to return the count (all notebooks if empty)
          @return count of incidences
        */
    int eventCount(const QString &notebookUid = QString());

    /**
      Return the count of todo incidences.

      @param notebookUid is uid of a notebook for which to return the count (all notebooks if empty)
      @return count of incidences
    */
    int todoCount(const QString &notebookUid = QString());

    /**
      Return the count of journal incidences.

      @param notebookUid is uid of a notebook for which to return the count (all notebooks if empty)
      @return count of incidences
    */
    int journalCount(const QString &notebookUid = QString());

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(ExtendedCalendar)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond
};

}

#endif
