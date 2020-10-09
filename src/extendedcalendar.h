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
#include <extendedstorageobserver.h>

namespace mKCal {

class ExtendedStorage;
class Notebook;

/**
  @brief
  This class provides a calendar cached into memory.
*/
class MKCAL_EXPORT ExtendedCalendar : public KCalendarCore::MemoryCalendar,
    public ExtendedStorageObserver
{
public:
    /**
      Incidence sort keys.
      See calendar.h for Event, Todo or Journal specific sorts.
    */
    enum IncidenceSortField {
        IncidenceSortUnsorted,   /**< Do not sort Incidences */
        IncidenceSortDate,       /**< Sort Incidence chronologically,
                                  Events by start date,
                                  Todos by due date,
                                  Journals by date */
        IncidenceSortCreated     /* < Sort Incidences based on creation time */
    };

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
      Calendar::reload()
    */
    bool reload();

    /**
      @copydoc
      Calendar::save()
    */
    bool save();

    /**
      @copydoc
      Calendar::close()
    */
    void close();

     /**
      Creates an ICalTimeZone instance and adds it to calendar's ICalTimeZones
      collection or returns an existing instance for the MSTimeZone component.

      @param tz the MSTimeZone structure to parse

      @see ICalTimeZoneSource::parse( MSTimeZone *, ICalTimeZones & )
    */
    /* KCalendarCore::ICalTimeZone parseZone(KCalendarCore::MSTimeZone *tz); */

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

    /**
      Returns the earliest date after @a date on which an event occurs, or an invalid date if
      there is no next date.
     */
    QDate nextEventsDate(const QDate &, const QTimeZone &timespec = QTimeZone());

    /**
      Returns the latest date before @a date on which an event occurs, or an invalid date if
      there is no previous date.
     */
    QDate previousEventsDate(const QDate &, const QTimeZone &timespec = QTimeZone());


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
      Notify the IncidenceBase::Observer that the incidence will be updated.

      @param uid to the Incidence to be updated.
    */
    void incidenceUpdate(const QString &uid, const QDateTime &recurrenceId);

    /**
      Notify the IncidenceBase::Observer that the incidence has been updated.

      @param uid to the Incidence just updated.
    */
    void incidenceUpdated(const QString &uid, const QDateTime &recurrenceId);

    // Incidence Specific Methods, also see Calendar.h for more //

    /**
      List all attendees currently in the memory.
      Attendees are persons that are associated to calendar incidences.

      @return list of attendees
    */
    QStringList attendees();

    /**
      List all attendee related incidences.

      @param email attendee email address
      @return list of incidences for the attendee
    */
    KCalendarCore::Incidence::List attendeeIncidences(const QString &email);

    /**
      List all incidences with geographic information in the memory.

      @return list of incidences
    */
    KCalendarCore::Incidence::List geoIncidences();

    /**
      List incidences with geographic information in the memory.

      @param geoLatitude latitude center
      @param geoLongitude longitude center
      @param diffLatitude maximum latitudinal difference
      @param diffLongitude maximum longitudinal difference
      @return list of incidences
    */
    KCalendarCore::Incidence::List geoIncidences(float geoLatitude, float geoLongitude,
                                            float diffLatitude, float diffLongitude);

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

    /**
      Sort a list of Incidences.

      @param list is a pointer to a list of Incidences.
      @param sortField specifies the IncidenceSortField (see this header).
      @param sortDirection specifies the KCalendarCore::SortDirection (see calendar.h).

      @return a list of Incidences sorted as specified.
    */
    static KCalendarCore::Incidence::List sortIncidences(
        KCalendarCore::Incidence::List *list,
        IncidenceSortField sortField = IncidenceSortDate,
        KCalendarCore::SortDirection sortDirection = KCalendarCore::SortDirectionAscending);

    /**
       Single expanded incidence validity struct.  The first field contains the
       time in local timezone when the (recurrent) incidence starts.
       The second field contains time in local timezone when the (recurrent) incidence ends.
    */
    typedef struct ExpandedIncidenceValidity {
        QDateTime dtStart;
        QDateTime dtEnd;
    } ExpandedIncidenceValidity;

    /**
       Single expanded incidence tuple.  The first field contains the
       time in local timezone when the (recurrent) incidence starts.
       The second field contains a pointer to the actual Incidence
       instance.
    */
    typedef QPair<ExpandedIncidenceValidity, KCalendarCore::Incidence::Ptr> ExpandedIncidence;

    /**
       List of ExpandedIncidences.
    */
    typedef QVector<ExpandedIncidence> ExpandedIncidenceList;
    typedef QVectorIterator<ExpandedIncidence> ExpandedIncidenceIterator;

    /**
      Expand recurring incidences in a list.

      The returned list contains ExpandedIncidence QPairs which
      contain both the time of the incidence, as well as pointer to
      the incidence itself.

      @param list is a pointer to a list of Incidences.
      @param start start time for expansion+filtering
      @param end end time for expansion+filtering
      @param maxExpand maximum expanded entries per incidence
      @param expandLimitHit (if available) is set to true if and only
      if we hit maxExpand when expanding some incidence

      @return a list of ExpandedIncidences sorted by start time (the
      first item in the ExpandedIncidence tuple) in ascending order.
    */
    ExpandedIncidenceList expandRecurrences(KCalendarCore::Incidence::List *list,
                                            const QDateTime &start,
                                            const QDateTime &end,
                                            int maxExpand = 1000,
                                            bool *expandLimitHit = 0);

    /**
      Returns a list of expanded events that occur on dates between start and end inclusive.
      @param startInclusive if true, only events that begin on or after start are included, otherwise
             events that begin before start, but continue into start will also be included.
      @param endInclusive if true, only events that end on or before end are included.
    */
    ExpandedIncidenceList rawExpandedEvents(const QDate &start, const QDate &end,
                                            bool startInclusive = false, bool endInclusive = false,
                                            const QTimeZone &timeZone = QTimeZone()) const;

    /**
      Expand multiday incidences in a list.

      This call expands the multiday events within the given list so
      that there's an event for each day. The start and end parameters
      are used for filtering for the expansion filtering range, and
      days falling outside the [startDate, endDate] range won't be
      expanded.

      Note that both startDate and endDate are optional, and if so,
      'as much as contained within the individual incidences' will be
      expanded.

      @param list is a pointer to a list of Incidences.
      @param startDate start date for expansion+filtering
      @param endDate end date for expansion+filtering
      @param maxExpand maximum number of days single multiday event
      instance can be expanded into
      @param merge whether the results should be merged to the list or not.
      @return a list of ExpandedIncidences sorted by start time in
      ascending order. The list may contain the initial parameter list
      if merge is true, or only the bonus multiday incidences if merge
      is false.
     */
    ExpandedIncidenceList expandMultiDay(const ExpandedIncidenceList &list,
                                         const QDate &startDate,
                                         const QDate &endDate,
                                         int maxExpand = 1000,
                                         bool merge = true,
                                         bool *expandLimitHit = 0);

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

    // Smart Loading Methods, see ExtendedStorage.h for more //

    /**
      Get all uncompleted todos. Todos may or may not have due date and
      they may or may not have geo location.

      @param hasDate true to get todos that have due date
      @param hasGeo value -1 = don't care, 0 = no geo, 1 = geo defined
      @return list of uncompleted todos
    */
    KCalendarCore::Todo::List uncompletedTodos(bool hasDate, int hasGeo);

    /**
      Get completed todos between given time.

      @param hasDate true to get todos that have due date
      @param hasGeo value -1 = don't care, 0 = no geo, 1 = geo defined
      @param start start datetime
      @param end end datetime
      @return list of completed todos
    */
    KCalendarCore::Todo::List completedTodos(bool hasDate, int hasGeo,
                                        const QDateTime &start, const QDateTime &end);

    /**
      Get incidences based on start/due dates or creation dates.

      @param hasDate true to get incidences that have due/start date
      @param start start datetime
      @param end end datetime
      @return list of incidences
    */
    KCalendarCore::Incidence::List incidences(bool hasDate, const QDateTime &start,
                                         const QDateTime &end);

    /**
      Get incidences that have geo location defined.

      @param hasDate true to get incidences that have due/start date
      @param start start datetime
      @param end end datetime
      @return list of geo incidences
    */
    KCalendarCore::Incidence::List geoIncidences(bool hasDate, const QDateTime &start,
                                            const QDateTime &end);

    /**
      Get all incidences that have unread invitation status.

      @param person if given only for this person
      @return list of unread incidences
      @see IncidenceBase::setInvitationStatus()
    */
    KCalendarCore::Incidence::List unreadInvitationIncidences(
        const KCalendarCore::Person &person = KCalendarCore::Person());

    /**
      Get incidences that have read/sent invitation status.

      @param start start datetime
      @param end end datetime
      @return list of old incidences
      @see IncidenceBase::setInvitationStatus()
    */
    KCalendarCore::Incidence::List oldInvitationIncidences(const QDateTime &start,
                                                      const QDateTime &end);

    /**
      Get incidences related to given contact. Relation is determined
      by the email address of the person, name doesn't matter.

      @param person for this person
      @param start start datetime
      @param end end datetime
      @return list of related incidences
    */
    KCalendarCore::Incidence::List contactIncidences(const KCalendarCore::Person &person,
                                                const QDateTime &start, const QDateTime &end);

    using KCalendarCore::Calendar::journals;

    /**
      Get journals between given times.

      @param start start datetime
      @param end end datetime
      @return list of journals
    */
    KCalendarCore::Journal::List journals(const QDate &start, const QDate &end);

    /**
      Add incidences into calendar from a list of Incidences.

      @param list is a pointer to a list of Incidences.
      @param notebookUid is uid of notebook of all incidences in list
      @param duplicateRemovalEnabled default value true
      if true and duplicates are found duplicates are deleted and new incidence is added
      if false and duplicates are found new incidence is ignored

      @return a list of Incidences added into calendar memory.
    */
    KCalendarCore::Incidence::List addIncidences(KCalendarCore::Incidence::List *list,
                                            const QString &notebookUid,
                                            bool duplicateRemovalEnabled = true);

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

protected:

    /**
       Implement the storageModified to clear ExtendedCalendar
       contents on the storage change.
     */
    virtual void storageModified(ExtendedStorage *storage, const QString &info);
    virtual void storageProgress(ExtendedStorage *storage, const QString &info);
    virtual void storageFinished(ExtendedStorage *storage, bool error, const QString &info);


private:
    //@cond PRIVATE
    Q_DISABLE_COPY(ExtendedCalendar)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond
};

}

#endif
