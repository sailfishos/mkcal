/*
  This file is part of the mkcal library.

  Copyright (c) 1998 Preston Brown <pbrown@kde.org>
  Copyright (c) 2001,2003,2004 Cornelius Schumacher <schumacher@kde.org>
  Copyright (C) 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
  Copyright (c) 2009 Alvaro Manera <alvaro.manera@nokia.com>
  Copyright (c) 2014-2019 Jolla Ltd.
  Copyright (c) 2019 Open Mobile Platform LLC.

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
#include "logging_p.h"

#include <calfilter.h>
#include <sorting.h>
using namespace KCalCore;

#include <kdebug.h>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <cmath>

// #ifdef to control expensive/spammy debug stmts
#undef DEBUG_EXPANSION

namespace {
    // KDateTime::toClockTime() has the semantic that the input is first
    // converted to the local system timezone, before having its timezone
    // information stripped.
    // In many cases in mkcal, we use clock-time to mean "floating"
    // i.e. irrespective of timezone, and thus when converting to or from
    // clock time, we don't want any conversion to the local system timezone
    // to occur as part of that operation.
    KDateTime kdatetimeAsTimeSpec(const KDateTime &input, const KDateTime::Spec &spec) {
        if (input.isClockTime() || spec.type() == KDateTime::ClockTime) {
            return KDateTime(input.date(), input.time(), spec);
        } else {
            return input.toTimeSpec(spec);
        }
    }
}

using namespace mKCal;
/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE

/**
  Make a QHash::value that returns a QVector.
*/
template <typename K, typename V>
QVector<V> values(const QMultiHash<K, V> &c, const K &x)
{
    QVector<V> v;
    typename QMultiHash<K, V>::const_iterator it = c.find(x);
    while (it != c.end() && it.key() == x) {
        v.push_back(it.value());
        ++it;
    }
    return v;
}

class mKCal::ExtendedCalendar::Private
{
public:
    Private()
    {
    }
    ~Private()
    {
    }
    Incidence::List mGeoIncidences;                  // list of all Geo Incidences
    QMultiHash<QString, Incidence::Ptr>mAttendeeIncidences; // lists of incidences for attendees

    void addIncidenceToLists(const Incidence::Ptr &incidence);
    void removeIncidenceFromLists(const Incidence::Ptr &incidence);

    /**
     * Figure when particular recurrence of an incidence starts.
     *
     * The start is a hint that is used as rough approximation (it may
     * be result of expandMultiDay).
     */
    static QDateTime incidenceRecurrenceStart(const KCalCore::Incidence::Ptr &incidence, const QDateTime start);

    /**
     * Figure appropriate end time for incidence.
     *
     * This functon should be used only on results of expandMultiDay,
     * and therefore the start may or may not be incidence start time.
     *
     * param start The start time gotten from say, expandMultiDay
     * param endWithinDay Whether we should try to end within the day or not.
     *
     * return The time when the event ends. While this function returns
     * QDateTime, the date will be always same as start.date(); this is
     * just convenience API. Only exception is all-day case, where date
     * = start.date()+1, and time is 0,0,0.
     */
    static QDateTime incidenceEndTime(const KCalCore::Incidence::Ptr &incidence,
                                      const QDateTime start,
                                      bool endWithinDay);
};

ExtendedCalendar::ExtendedCalendar(const KDateTime::Spec &timeSpec)
    : MemoryCalendar(timeSpec), d(new mKCal::ExtendedCalendar::Private)
{
}

ExtendedCalendar::ExtendedCalendar(const QString &timeZoneId)
    : MemoryCalendar(timeZoneId), d(new mKCal::ExtendedCalendar::Private)
{
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
    d->mGeoIncidences.clear();
    d->mAttendeeIncidences.clear();
    MemoryCalendar::close();
}

ICalTimeZone ExtendedCalendar::parseZone(MSTimeZone *tz)
{
    ICalTimeZone zone;

    ICalTimeZones *icalZones = timeZones();
    if (icalZones) {
        ICalTimeZoneSource src;
        zone = src.parse(tz, *icalZones);
    }
    return zone;
}

// Dissociate a single occurrence or all future occurrences from a recurring
// sequence. The new incidence is returned, but not automatically inserted
// into the calendar, which is left to the calling application.
Incidence::Ptr ExtendedCalendar::dissociateSingleOccurrence(const Incidence::Ptr &incidence,
                                                            const KDateTime &dateTime,
                                                            const KDateTime::Spec &spec)
{
    if (!incidence || !incidence->recurs()) {
        return Incidence::Ptr();
    }

    if (!incidence->allDay()) {
        if (!incidence->recursAt(dateTime)) {
            return Incidence::Ptr();
        }
    } else {
        if (!incidence->recursOn(dateTime.date(), spec)) {
            return Incidence::Ptr();
        }
    }

    Incidence::Ptr newInc = Incidence::Ptr(incidence->clone());
    KDateTime nowUTC = KDateTime::currentUtcDateTime();
    newInc->setCreated(nowUTC);
    newInc->setSchedulingID(QString());
    incidence->setLastModified(nowUTC);

    Recurrence *recur = newInc->recurrence();
    if (recur)
        newInc->clearRecurrence();

    // Adjust the date of the incidence
    if (incidence->type() == Incidence::TypeEvent) {
        Event::Ptr ev = newInc.staticCast<Event>();
        KDateTime start(ev->dtStart());
        int secsTo =
            start.toTimeSpec(spec).dateTime().secsTo(dateTime.toTimeSpec(spec).dateTime());
        ev->setDtStart(start.addSecs(secsTo));
        ev->setDtEnd(ev->dtEnd().addSecs(secsTo));
    } else if (incidence->type() == Incidence::TypeTodo) {
        Todo::Ptr td = newInc.staticCast<Todo>();
        bool haveOffset = false;
        int secsTo = 0;
        if (td->hasDueDate()) {
            KDateTime due(td->dtDue());
            secsTo = due.toTimeSpec(spec).dateTime().secsTo(dateTime.toTimeSpec(spec).dateTime());
            td->setDtDue(due.addSecs(secsTo), true);
            haveOffset = true;
        }
        if (td->hasStartDate()) {
            KDateTime start(td->dtStart());
            if (!haveOffset) {
                secsTo =
                    start.toTimeSpec(spec).dateTime().secsTo(dateTime.toTimeSpec(spec).dateTime());
            }
            td->setDtStart(start.addSecs(secsTo));
            haveOffset = true;
        }
    } else if (incidence->type() == Incidence::TypeJournal) {
        Journal::Ptr jr = newInc.staticCast<Journal>();
        KDateTime start(jr->dtStart());
        int secsTo =
            start.toTimeSpec(spec).dateTime().secsTo(dateTime.toTimeSpec(spec).dateTime());
        jr->setDtStart(start.addSecs(secsTo));
    }

    // set recurrenceId for new incidence
    newInc->setRecurrenceId(dateTime);

    recur = incidence->recurrence();
    if (recur) {
        if (incidence->allDay()) {
            recur->addExDate(dateTime.date());
        } else {
            recur->addExDateTime(dateTime);
        }
    }

    return newInc;
}

bool ExtendedCalendar::addIncidence(const Incidence::Ptr &incidence)
{
    // Need to by-pass the override done in MemoryCalendar to get back
    // the genericity of the call implemented in the Calendar class.
    return Calendar::addIncidence(incidence);
}

bool ExtendedCalendar::deleteIncidence(const Incidence::Ptr &incidence)
{
    // Need to by-pass the override done in MemoryCalendar to get back
    // the genericity of the call implemented in the Calendar class.
    return Calendar::deleteIncidence(incidence);
}

bool ExtendedCalendar::addEvent(const Event::Ptr &aEvent)
{
    return addEvent(aEvent, defaultNotebook());
}

bool ExtendedCalendar::addEvent(const Event::Ptr &aEvent, const QString &notebookUid)
{
    if (!aEvent) {
        return false;
    }

    if (notebookUid.isEmpty()) {
        qCWarning(lcMkcal) << "ExtendedCalendar::addEvent(): NotebookUid empty";
        return false;
    }

    if (MemoryCalendar::event(aEvent->uid(), aEvent->recurrenceId())) {
        qCDebug(lcMkcal) << "Duplicate found, event was not added";
        return false;
    }

    if (MemoryCalendar::addIncidence(aEvent)) {
        d->addIncidenceToLists(aEvent);
        return setNotebook(aEvent, notebookUid);
    } else {
        return false;
    }
}

bool ExtendedCalendar::deleteEvent(const Event::Ptr &event)
{
    if (MemoryCalendar::deleteIncidence(event)) {
        event->unRegisterObserver(this);
        d->removeIncidenceFromLists(event);
        return true;
    } else {
        return false;
    }
}

bool ExtendedCalendar::addTodo(const Todo::Ptr &aTodo)
{
    return addTodo(aTodo, defaultNotebook());
}

bool ExtendedCalendar::addTodo(const Todo::Ptr &aTodo, const QString &notebookUid)
{
    if (!aTodo) {
        return false;
    }

    if (notebookUid.isEmpty()) {
        qCWarning(lcMkcal) << "ExtendedCalendar::addTodo(): NotebookUid empty";
        return false;
    }

    Todo::Ptr old = MemoryCalendar::todo(aTodo->uid(), aTodo->recurrenceId());
    if (old) {
        if (aTodo->revision() > old->revision()) {
            deleteTodo(old);   // move old to deleted
        } else {
            qCDebug(lcMkcal) << "Duplicate found, todo was not added";
            return false;
        }
    }

    if (MemoryCalendar::addIncidence(aTodo)) {
        d->addIncidenceToLists(aTodo);
        return setNotebook(aTodo, notebookUid);
    } else {
        return false;
    }
}

bool ExtendedCalendar::deleteTodo(const Todo::Ptr &todo)
{
    if (MemoryCalendar::deleteIncidence(todo)) {
        todo->unRegisterObserver(this);
        d->removeIncidenceFromLists(todo);
        return true;
    } else {
        return false;
    }
}

Todo::List ExtendedCalendar::rawTodos(TodoSortField sortField, SortDirection sortDirection) const
{
    Todo::List todoList = MemoryCalendar::rawTodos(sortField, sortDirection);
    // Need to filter out non visible todos for compatibility reasons with
    // older implementations of ExtendedCalendar::rawTodos().
    todoList.erase(std::remove_if(todoList.begin(), todoList.end(),
                                  [this] (const Todo::Ptr &todo) {return !isVisible(todo);}),
                   todoList.end());

    return todoList;
}

Todo::List ExtendedCalendar::rawTodosForDate(const QDate &date) const
{
    Todo::List todoList = MemoryCalendar::rawTodosForDate(date);
    // Need to filter out non visible todos for compatibility reasons with
    // older implementations of ExtendedCalendar::rawTodosForDate().
    todoList.erase(std::remove_if(todoList.begin(), todoList.end(),
                                  [this] (const Todo::Ptr &todo) {return !isVisible(todo);}),
                   todoList.end());

    return todoList;
}

void ExtendedCalendar::incidenceUpdate(const QString &uid, const KDateTime &recurrenceId)
{
    // The static_cast is ok as the ExtendedCalendar only observes Incidence objects
    Incidence::Ptr incidence = this->incidence(uid, recurrenceId);

    if (!incidence) {
        return;
    }

    d->removeIncidenceFromLists(incidence);
    MemoryCalendar::incidenceUpdate(uid, recurrenceId);
}

void ExtendedCalendar::incidenceUpdated(const QString &uid, const KDateTime &recurrenceId)
{
    Incidence::Ptr incidence = this->incidence(uid, recurrenceId);

    if (!incidence) {
        return;
    }

    d->addIncidenceToLists(incidence);
    MemoryCalendar::incidenceUpdated(uid, recurrenceId);
}

Event::List ExtendedCalendar::rawEventsForDate(const QDate &date,
                                               const KDateTime::Spec &timespec,
                                               EventSortField sortField,
                                               SortDirection sortDirection) const
{
    Event::List eventList = MemoryCalendar::rawEventsForDate(date, timespec, sortField, sortDirection);
    // Need to filter out non visible events for compatibility reasons with
    // older implementations of ExtendedCalendar::rawEventsForDate().
    eventList.erase(std::remove_if(eventList.begin(), eventList.end(),
                                   [this] (const Event::Ptr &event) {return !isVisible(event);}),
                    eventList.end());

    return eventList;
}

ExtendedCalendar::ExpandedIncidenceList ExtendedCalendar::rawExpandedEvents(const QDate &start, const QDate &end,
                                                                            bool startInclusive, bool endInclusive,
                                                                            const KDateTime::Spec &timespec) const
{
    ExpandedIncidenceList eventList;

    KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();
    KDateTime ksdt(start, ts);
    KDateTime kedt = KDateTime(end.addDays(1), QTime(0, 0, 0), ts);

    // Iterate over all events. Look for recurring events that occur on this date
    const Event::List events(rawEvents());
    for (const Event::Ptr &ev: events) {
        const bool asClockTime = ev->dtStart().isClockTime() || ts.type() == KDateTime::ClockTime;
        const KDateTime startTime = ev->dtStart();
        const KDateTime endTime = ev->dtEnd();
        const KDateTime rangeStartTime = (ev->allDay() && asClockTime)
            ? KDateTime(ksdt.date(), QTime(), KDateTime::ClockTime)
            : ev->allDay() ? ksdt
            : KDateTime(ksdt.date(), QTime(0,0,0), ksdt.timeSpec());
        const KDateTime rangeEndTime = (ev->allDay() && asClockTime)
            ? KDateTime(kedt.date(), QTime(), KDateTime::ClockTime)
            : kedt;
        const KDateTime tsRangeStartTime = kdatetimeAsTimeSpec(rangeStartTime, ts);
        const KDateTime tsRangeEndTime = kdatetimeAsTimeSpec(rangeEndTime, ts);
        if (ev->recurs()) {
            int extraDays = (ev->isMultiDay() && !startInclusive)
                ? startTime.date().daysTo(endTime.date())
                : (ev->allDay() ? 1 : 0);
            const KDateTime tsAdjustedRangeStartTime(tsRangeStartTime.addDays(-extraDays));
            const DateTimeList times = ev->recurrence()->timesInInterval(tsAdjustedRangeStartTime, tsRangeEndTime);
            for (const KDateTime &timeInInterval : times) {
                const KDateTime tsStartTime = kdatetimeAsTimeSpec(timeInInterval, ts);
                const KDateTime tsEndTime = Duration(startTime, endTime).end(tsStartTime);
                if (tsStartTime >= tsRangeEndTime
                    || tsEndTime <= tsAdjustedRangeStartTime
                    || (endInclusive && (tsEndTime > tsRangeEndTime))) {
                    continue;
                }
                ExpandedIncidenceValidity eiv = {tsStartTime.dateTime(),
                                                 tsEndTime.dateTime()};
                eventList.append(qMakePair(eiv, ev.dynamicCast<Incidence>()));
            }
        } else {
            const KDateTime tsStartTime = kdatetimeAsTimeSpec(startTime, ts);
            const KDateTime tsEndTime = kdatetimeAsTimeSpec(endTime, ts);
            if (ev->isMultiDay()) {
                if ((startInclusive == false || tsStartTime >= tsRangeStartTime) &&
                    tsStartTime <= tsRangeEndTime && tsEndTime >= tsRangeStartTime &&
                    (endInclusive == false || tsEndTime <= tsRangeEndTime)) {
                    ExpandedIncidenceValidity eiv = {
                                                     tsStartTime.dateTime(),
                                                     tsEndTime.dateTime()
                    };
                    eventList.append(qMakePair(eiv, ev.dynamicCast<Incidence>()));
                }
            } else {
                if (tsStartTime >= tsRangeStartTime && tsStartTime <= tsRangeEndTime) {
                    ExpandedIncidenceValidity eiv = {
                                                     tsStartTime.dateTime(),
                                                     tsEndTime.dateTime()
                    };
                    eventList.append(qMakePair(eiv, ev.dynamicCast<Incidence>()));
                }
            }
        }
    }

    return eventList;
}

Event::List ExtendedCalendar::rawEvents(EventSortField sortField,
                                        SortDirection sortDirection) const
{
    Event::List eventList = MemoryCalendar::rawEvents(sortField, sortDirection);
    // Need to filter out non visible events for compatibility reasons with
    // older implementations of ExtendedCalendar::rawEvents().
    eventList.erase(std::remove_if(eventList.begin(), eventList.end(),
                                   [this] (const Event::Ptr &event) {return !isVisible(event);}),
                    eventList.end());

    return eventList;
}

QDate ExtendedCalendar::nextEventsDate(const QDate &date, const KDateTime::Spec &timespec)
{
    KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();

    KDateTime kdt(date, ts);
    KDateTime tomorrow = kdt.addDays(1);
    KDateTime almostTomorrow = tomorrow;
    almostTomorrow.setDateOnly(false);
    almostTomorrow = almostTomorrow.addSecs(-1);

    KDateTime rv;

    const Event::List &events(rawEvents());
    for (const Event::Ptr &ev: events) {
        if (ev->recurs()) {
            if (ev->isMultiDay()) {
                int extraDays = ev->dtStart().date().daysTo(ev->dtEnd().date());
                for (int i = 0; i <= extraDays; ++i) {
                    if (ev->recursOn(date.addDays(1 - i), ts))
                        return tomorrow.toTimeSpec(ts).date();
                }
            }

            KDateTime next = ev->recurrence()->getNextDateTime(almostTomorrow);
            next.setDateOnly(true);

            if (!rv.isValid() || next < rv)
                rv = next;
        } else if (ev->isMultiDay()) {
            KDateTime edate = ev->dtStart();
            edate.setDateOnly(true);
            if (edate > kdt) {
                if (!rv.isValid() || edate < rv)
                    rv = edate;
            } else {
                edate = ev->dtEnd();
                edate.setDateOnly(true);
                if (edate > kdt)
                    rv = tomorrow;
            }
        } else {
            KDateTime edate = ev->dtStart();
            edate.setDateOnly(true);
            if (edate > kdt && (!rv.isValid() || edate < rv))
                rv = edate;
        }

        if (rv == tomorrow)
            break; // Bail early - you can't beat tomorrow
    }

    if (!rv.isValid())
        return QDate();
    else
        return rv.toTimeSpec(ts).date();
}

QDate ExtendedCalendar::previousEventsDate(const QDate &date, const KDateTime::Spec &timespec)
{
    KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();

    KDateTime kdt(date, ts);
    KDateTime yesterday = kdt.addDays(-1);

    KDateTime rv;

    const Event::List events(rawEvents());
    for (const Event::Ptr &ev: events) {
        if (ev->recurs()) {
            KDateTime prev = ev->recurrence()->getPreviousDateTime(kdt);
            prev.setDateOnly(true);

            if (ev->isMultiDay()) {
                prev = prev.addDays(ev->dtStart().date().daysTo(ev->dtEnd().date()));
                if (prev >= kdt)
                    return yesterday.toTimeSpec(ts).date();
            }

            if (!rv.isValid() || prev > rv)
                rv = prev;
        } else if (ev->isMultiDay()) {
            KDateTime edate = ev->dtEnd();
            edate.setDateOnly(true);
            if (edate < kdt) {
                if (!rv.isValid() || edate > rv)
                    rv = edate;
            } else {
                edate = ev->dtStart();
                edate.setDateOnly(true);
                if (edate < kdt)
                    rv = yesterday;
            }
        } else {
            KDateTime edate = ev->dtStart();
            edate.setDateOnly(true);
            if (edate < kdt && (!rv.isValid() || edate > rv))
                rv = edate;
        }

        if (rv == yesterday)
            break; // Bail early - you can't beat tomorrow
    }

    if (!rv.isValid())
        return QDate();
    else
        return rv.toTimeSpec(ts).date();
}


bool ExtendedCalendar::addJournal(const Journal::Ptr &aJournal)
{
    return addJournal(aJournal, defaultNotebook());
}

bool ExtendedCalendar::addJournal(const Journal::Ptr &aJournal, const QString &notebookUid)
{
    if (!aJournal) {
        return false;
    }

    if (notebookUid.isEmpty()) {
        qCWarning(lcMkcal) << "ExtendedCalendar::addJournal(): NotebookUid empty";
        return false;
    }

    Journal::Ptr old = journal(aJournal->uid(), aJournal->recurrenceId());
    if (old) {
        if (aJournal->revision() > old->revision()) {
            deleteJournal(old);   // move old to deleted
        } else {
            qCDebug(lcMkcal) << "Duplicate found, journal was not added";
            return false;
        }
    }

    if (MemoryCalendar::addIncidence(aJournal)) {
        d->addIncidenceToLists(aJournal);
        return setNotebook(aJournal, notebookUid);
    } else {
        return false;
    }
}

bool ExtendedCalendar::deleteJournal(const Journal::Ptr &journal)
{
    if (MemoryCalendar::deleteIncidence(journal)) {
        journal->unRegisterObserver(this);
        d->removeIncidenceFromLists(journal);
        return true;
    } else {
        return false;
    }
}

Journal::List ExtendedCalendar::rawJournals(JournalSortField sortField,
                                            SortDirection sortDirection) const
{
    Journal::List journalList = MemoryCalendar::rawJournals(sortField, sortDirection);
    // Need to filter out non visible journalss for compatibility reasons with
    // older implementations of ExtendedCalendar::rawJournals().
    journalList.erase(std::remove_if(journalList.begin(), journalList.end(),
                                     [this] (const Journal::Ptr &journal) {return !isVisible(journal);}),
                      journalList.end());

    return journalList;
}

Journal::List ExtendedCalendar::rawJournalsForDate(const QDate &date) const
{
    Journal::List journalList = MemoryCalendar::rawJournalsForDate(date);
    // Need to filter out non visible journalss for compatibility reasons with
    // older implementations of ExtendedCalendar::rawJournalsForDate().
    journalList.erase(std::remove_if(journalList.begin(), journalList.end(),
                                     [this] (const Journal::Ptr &journal) {return !isVisible(journal);}),
                      journalList.end());

    return journalList;
}

Journal::List ExtendedCalendar::rawJournals(const QDate &start, const QDate &end,
                                            const KDateTime::Spec &timespec, bool inclusive) const
{
    Q_UNUSED(inclusive);
    Journal::List journalList;
    KDateTime::Spec ts = timespec.isValid() ? timespec : timeSpec();
    KDateTime st(start, ts);
    KDateTime nd(end, ts);

    // Get journals
    const Journal::List journals(rawJournals());
    for (const Journal::Ptr &journal: journals) {
        KDateTime rStart = journal->dtStart();
        if (nd.isValid() && nd < rStart) {
            continue;
        }
        if (inclusive && st.isValid() && rStart < st) {
            continue;
        }

        if (!journal->recurs()) {   // non-recurring journals
            // TODO_ALVARO: journals don't have endDt, bug?
            KDateTime rEnd = journal->dateTime(Incidence::RoleEnd);
            if (st.isValid() && rEnd < st) {
                continue;
            }
            if (inclusive && nd.isValid() && nd < rEnd) {
                continue;
            }
        } else { // recurring journals
            switch (journal->recurrence()->duration()) {
            case -1: // infinite
                if (inclusive) {
                    continue;
                }
                break;
            case 0: // end date given
            default: // count given
                KDateTime rEnd(journal->recurrence()->endDate(), ts);
                if (!rEnd.isValid()) {
                    continue;
                }
                if (st.isValid() && rEnd < st) {
                    continue;
                }
                if (inclusive && nd.isValid() && nd < rEnd) {
                    continue;
                }
                break;
            } // switch(duration)
        } //if(recurs)

        journalList.append(journal);
    }

    return journalList;
}

QStringList ExtendedCalendar::attendees()
{
    return d->mAttendeeIncidences.uniqueKeys();
}

Incidence::List ExtendedCalendar::attendeeIncidences(const QString &email)
{
    return values(d->mAttendeeIncidences, email);
}

Incidence::List ExtendedCalendar::geoIncidences()
{
    return d->mGeoIncidences;
}

Incidence::List ExtendedCalendar::geoIncidences(float geoLatitude, float geoLongitude,
                                                float diffLatitude, float diffLongitude)
{
    Incidence::List list;

    Incidence::List values = incidences(QString());
    Incidence::List::const_iterator it;
    for (it = values.begin(); it != values.end(); ++it) {
        float lat = (*it)->geoLatitude();
        float lon = (*it)->geoLongitude();

        if (fabs(lat - geoLatitude) <= diffLatitude &&
                fabs(lon - geoLongitude) <= diffLongitude) {
            list.append(*it);
        }
    }
    return list;
}

Incidence::List ExtendedCalendar::incidences(const QDate &date,
                                             const QList<KCalCore::Incidence::IncidenceType> &types)
{
    Event::List elist;
    Todo::List tlist;
    Journal::List jlist;

    if (types.contains(Incidence::TypeEvent)) {
        elist = events(date);
    }

    if (types.contains(Incidence::TypeTodo)) {
        tlist = todos(date);
    }

    if (types.contains(Incidence::TypeJournal)) {
        jlist = journals(date);
    }

    return mergeIncidenceList(elist, tlist, jlist);
}

void ExtendedCalendar::deleteAllIncidences()
{
    deleteAllEvents();
    deleteAllTodos();
    deleteAllJournals();
}

Incidence::List ExtendedCalendar::sortIncidences(Incidence::List *incidenceList,
                                                 IncidenceSortField sortField,
                                                 SortDirection sortDirection)
{
    Incidence::List incidenceListSorted;
    Incidence::List tempList;
//  Incidence::List::Iterator sortIt;
//  Incidence::List::Iterator iit;

    switch (sortField) {
    case IncidenceSortUnsorted:
        incidenceListSorted = *incidenceList;
        break;

    case IncidenceSortDate:
        incidenceListSorted = *incidenceList;
        if (sortDirection == SortDirectionAscending) {
            qSort(incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::dateLessThan);
        } else {
            qSort(incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::dateMoreThan);
        }
        break;

    case IncidenceSortCreated:
        incidenceListSorted = *incidenceList;
        if (sortDirection == SortDirectionAscending) {
            qSort(incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::createdLessThan);
        } else {
            qSort(incidenceListSorted.begin(), incidenceListSorted.end(), Incidences::createdMoreThan);
        }
        break;

    }
    return incidenceListSorted;
}

//@cond PRIVATE
void ExtendedCalendar::Private::addIncidenceToLists(const Incidence::Ptr &incidence)
{
    const Person::Ptr organizer = incidence->organizer();
    if (organizer && !organizer->isEmpty()) {
        mAttendeeIncidences.insert(organizer->email(), incidence);
    }
    const Attendee::List &list = incidence->attendees();
    Attendee::List::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        mAttendeeIncidences.insert((*it)->email(), incidence);
    }
    if (incidence->hasGeo()) {
        mGeoIncidences.append(incidence);
    }
}

void ExtendedCalendar::Private::removeIncidenceFromLists(const Incidence::Ptr &incidence)
{
    const Person::Ptr organizer = incidence->organizer();
    if (organizer && !organizer->isEmpty()) {
        mAttendeeIncidences.remove(organizer->email(), incidence);
    }
    const Attendee::List &list = incidence->attendees();
    Attendee::List::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        mAttendeeIncidences.remove((*it)->email(), incidence);
    }
    if (incidence->hasGeo()) {
        mGeoIncidences.removeAll(incidence);
    }
}

QDateTime ExtendedCalendar::Private::incidenceRecurrenceStart(const KCalCore::Incidence::Ptr &incidence,
                                                              const QDateTime ost)
{
    if (!incidence)
        return QDateTime();

    if (!incidence->recurs())
        return incidence->dtStart().toLocalZone().dateTime();

    // Then figure how far off from the start of this recurrence we are
    KDateTime dt(ost.addSecs(1));
    return incidence->recurrence()->getPreviousDateTime(dt).toLocalZone().dateTime();
}

QDateTime ExtendedCalendar::Private::incidenceEndTime(const KCalCore::Incidence::Ptr &incidence,
                                                      const QDateTime ost,
                                                      bool endWithinDay)
{
    if (!incidence)
        return QDateTime();

    // First off, figure how long the initial event is
    KDateTime dtS(incidence->dtStart());
    KDateTime dtE(incidence->dateTime(Incidence::RoleEnd));
    int duration = dtE.toTime_t() - dtS.toTime_t();

    KDateTime dt(ost);
    KDateTime start(incidenceRecurrenceStart(incidence, ost));

    int duration0 = dt.toTime_t() - start.toTime_t();

    int left = duration - duration0;

    QDateTime r = dt.addSecs(left).dateTime();
    if (r.date() != ost.date()) {
        r = QDateTime(r.date(), QTime(0, 0, 0));
        if (endWithinDay) {
            r = r.addDays(1);
            r = r.addSecs(-1);
        }
    }
    return r;
}
//@endcond

static bool expandedIncidenceSortLessThan(const ExtendedCalendar::ExpandedIncidence &e1,
                                          const ExtendedCalendar::ExpandedIncidence &e2)
{
    if (e1.first.dtStart < e2.first.dtStart) {
        return true;
    }
    if (e1.first.dtStart > e2.first.dtStart) {
        return false;
    }
    // e1 == e2 => perform secondary check based on created date
    return e1.second->created() < e2.second->created();
}

ExtendedCalendar::ExpandedIncidenceList ExtendedCalendar::expandRecurrences(
    Incidence::List *incidenceList, const KDateTime &dtStart, const KDateTime &dtEnd, int maxExpand, bool *expandLimitHit)
{
    ExtendedCalendar::ExpandedIncidenceList returnList;
    Incidence::List::Iterator iit;
    KDateTime brokenDtStart = dtStart.addSecs(-1);
    KDateTime::Spec ts = timeSpec();
    ExpandedIncidenceValidity validity;

    // used for comparing with entries that have broken dtEnd => we use
    // dtStart and compare it against this instead. As this is allocated
    // only once per iteration, it should result in significan net
    // savings

//  qCDebug(lcMkcal) << "expandRecurrences"
//           << incidenceList->size()
//           << dtStart.toString() << dtStart.isValid()
//           << dtEnd.toString() << dtEnd.isValid();

    if (expandLimitHit)
        *expandLimitHit = false;

    for (iit = incidenceList->begin(); iit != incidenceList->end(); ++iit) {
        KDateTime dt = (*iit)->dtStart().toLocalZone();
        KDateTime dte = (*iit)->dateTime(IncidenceBase::RoleEndRecurrenceBase);
        int appended = 0;
        int skipped = 0;
        bool brokenEnd = false;

        if ((*iit)->type() == Incidence::TypeTodo) {
            Todo::Ptr todo = (*iit).staticCast<Todo>();
            if (todo->hasDueDate()) {
                dt = todo->dtDue().toLocalZone();
            }
        }

        if (!dt.isValid()) {
            // Just leave the dateless incidences there (they will be
            // sorted out)
            validity.dtStart = dt.dateTime();
            validity.dtEnd = d->incidenceEndTime(*iit, dt.dateTime(), true);
            returnList.append(ExpandedIncidence(validity, *iit));
            continue;
        }

        // Fix the non-valid dte to be dt+1
        if (dte.isValid() && dte <= dt) {
            brokenEnd = true;
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

        if ((!dtEnd.isValid() || dt < dtEnd)
                && (!dte.isValid()
                    || (!brokenEnd && dte > dtStart)
                    || (brokenEnd && dt > brokenDtStart))) {
#ifdef DEBUG_EXPANSION
            qCDebug(lcMkcal) << "---appending" << (*iit)->summary() << dt.toString();
#endif /* DEBUG_EXPANSION */
            if ((*iit)->recurs()) {
                if (!(*iit)->allDay()) {
                    if (!(*iit)->recursAt((*iit)->dtStart())) {
#ifdef DEBUG_EXPANSION
                        qCDebug(lcMkcal) << "--not recurring at" << (*iit)->dtStart() << (*iit)->summary();
#endif /* DEBUG_EXPANSION */
                    } else {
                        validity.dtStart = dt.dateTime();
                        validity.dtEnd = d->incidenceEndTime(*iit, dt.dateTime(), true);
                        returnList.append(ExpandedIncidence(validity, *iit));
                        appended++;
                    }
                } else {
                    if (!(*iit)->recursOn((*iit)->dtStart().date(), ts)) {
#ifdef DEBUG_EXPANSION
                        qCDebug(lcMkcal) << "--not recurring on" << (*iit)->dtStart() << (*iit)->summary();
#endif /* DEBUG_EXPANSION */
                    } else {
                        validity.dtStart = dt.dateTime();
                        validity.dtEnd = d->incidenceEndTime(*iit, dt.dateTime(), true);
                        returnList.append(ExpandedIncidence(validity, *iit));
                        appended++;
                    }
                }
            } else {
                validity.dtStart = dt.dateTime();
                validity.dtEnd = d->incidenceEndTime(*iit, dt.dateTime(), true);
                returnList.append(ExpandedIncidence(validity, *iit));
                appended++;
            }
        } else {
#ifdef DEBUG_EXPANSION
            qCDebug(lcMkcal) << "-- no match" << dt.toString() << dte.toString() << dte.isValid() << brokenEnd;
#endif /* DEBUG_EXPANSION */
        }

        if ((*iit)->recurs()) {
            KDateTime dtr = dt;
            KDateTime dtro;

            // If the original entry wasn't part of the time window, try to
            // get more appropriate first item to add. Else, start the
            // next-iteration from the 'dt' (=current item).
            if (!appended) {
                dtr = (*iit)->recurrence()->getPreviousDateTime(dtStart);
                if (dtr.isValid()) {
                    KDateTime dtr2 = (*iit)->recurrence()->getPreviousDateTime(dtr);
                    if (dtr2.isValid()) {
                        dtr = dtr2;
                    }
                } else {
                    dtr = dt;
                }
            }

            int duration = 0;
            if (brokenEnd)
                duration = 1;
            else if (dte.isValid())
                duration = dte.toTime_t() - dt.toTime_t();

            // Old logic had us keeping around [recur-start, recur-end[ > dtStart
            // As recur-end = recur-start + duration, we can rewrite the conditions
            // as recur-start[ > dtStart - duration
            KDateTime dtStartMinusDuration = dtStart.addSecs(-duration);

            while (appended < maxExpand) {
                dtro = dtr;
                dtr = (*iit)->recurrence()->getNextDateTime(dtr).toLocalZone();
                if (!dtr.isValid() || (dtEnd.isValid() && dtr >= dtEnd)) {
                    break;
                }

                /* If 'next' results in wrong date, give up. We have
                * to be moving forward. */
                if (dtr <= dtro) {
                    qCDebug(lcMkcal) << "--getNextDateTime broken - " << dtr << (*iit);
                    break;
                }

                // As incidences are in sorted order, the [1] condition was
                // already met as we're still iterating. Have to check [2].
                if (dtr > dtStartMinusDuration) {
#ifdef DEBUG_EXPANSION
                    qCDebug(lcMkcal) << "---appending(recurrence)"
                             << (*iit)->summary() << dtr.toString();
#endif /* DEBUG_EXPANSION */
                    validity.dtStart = dtr.dateTime();
                    validity.dtEnd = d->incidenceEndTime(*iit, dtr.dateTime(), true);
                    returnList.append(ExpandedIncidence(validity, *iit));
                    appended++;
                } else {
#ifdef DEBUG_EXPANSION
                    qCDebug(lcMkcal) << "---skipping(recurrence)"
                             << skipped << (*iit)->summary()
                             << duration << dtr.toString();
#endif /* DEBUG_EXPANSION */
                    if (skipped++ >= 100) {
                        qCDebug(lcMkcal) << "--- skip count exceeded, breaking loop";
                        break;
                    }
                }
            }
            if (appended == maxExpand && expandLimitHit) {
                qCDebug(lcMkcal) << "!!! HIT LIMIT" << maxExpand;
                *expandLimitHit = true;
            }
        }
    }
    qSort(returnList.begin(), returnList.end(), expandedIncidenceSortLessThan);
    return returnList;
}

ExtendedCalendar::ExpandedIncidenceList
ExtendedCalendar::expandMultiDay(const ExtendedCalendar::ExpandedIncidenceList &list,
                                 const QDate &startDate,
                                 const QDate &endDate,
                                 int maxExpand,
                                 bool merge,
                                 bool *expandLimitHit)
{
    ExtendedCalendar::ExpandedIncidenceList returnList;
    int i;
    ExpandedIncidenceValidity validity;

    if (expandLimitHit)
        *expandLimitHit = false;

    qCDebug(lcMkcal) << "expandMultiDay" << startDate.toString()
             << endDate.toString() << maxExpand << merge;
    foreach (const ExtendedCalendar::ExpandedIncidence &ei, list) {
        // If not event, not interested
        Incidence::Ptr inc = ei.second;
        Event::Ptr e = inc.staticCast<Event>();

        if (inc->type() != Incidence::TypeEvent || !e->isMultiDay()) {
            if (merge) {
                QDate d = ei.first.dtStart.date();
                if ((!startDate.isValid() || startDate <= d)
                        && (!endDate.isValid() || endDate >= d)) {
                    returnList.append(ei);
                }
            }
            continue;
        }

        KDateTime dts = inc->dtStart().toLocalZone();
        KDateTime dte = inc->dateTime(IncidenceBase::RoleEndRecurrenceBase).toLocalZone().addSecs(
                            -1); // inclusive, all-day events end on first sec of next day

        int days = 1;
        while (dts.date() < dte.date()) {
            days++;
            dte = dte.addDays(-1);
        }

        // Initialize dts/dte to the current recurrence (if any)
        dts = KDateTime(ei.first.dtStart.date(), dts.time());
        dte = KDateTime(ei.first.dtStart.date().addDays(1), QTime(0, 0, 0));

        int added = 0;
        for (i = 0 ; i < days ; i++) {
            if (i || merge) {
                // Possibly add the currently iterated one.
                // Have to check it against time boundaries using the dts/dte, though
                if ((!startDate.isValid() || startDate < dte.date())
                        && (!endDate.isValid() || endDate >= dts.date())) {
                    validity.dtStart = dts.dateTime();
                    validity.dtEnd = d->incidenceEndTime(inc, dts.dateTime(), true);
                    returnList.append(ExtendedCalendar::ExpandedIncidence(validity, inc));
                    if (added++ == maxExpand) {
                        if (expandLimitHit)
                            *expandLimitHit = true;
                        break;
                    }
                }
            }
            dts = dte;
            dte = dts.addDays(1);
        }
    }
    qSort(returnList.begin(), returnList.end(), expandedIncidenceSortLessThan);
    return returnList;
}


Incidence::List ExtendedCalendar::incidences(const QDate &start, const QDate &end)
{
    return mergeIncidenceList(events(start, end), todos(start, end), journals(start, end));
}

// QDir::isReadable() doesn't support group permissions, only user permissions.
bool directoryIsRW(const QString &dirPath)
{
    QFileInfo databaseDirInfo(dirPath);
    return (databaseDirInfo.permission(QFile::ReadGroup | QFile::WriteGroup)
            || databaseDirInfo.permission(QFile::ReadUser  | QFile::WriteUser));
}

ExtendedStorage::Ptr ExtendedCalendar::defaultStorage(const ExtendedCalendar::Ptr &calendar)
{
    // Use a central storage location by default
    QString privilegedDataDir = QString("%1/.local/share/system/privileged/").arg(QDir::homePath());
    QString unprivilegedDataDir = QString("%1/.local/share/system/").arg(QDir::homePath());

    // Allow override
    QString dbFile = QLatin1String(qgetenv("SQLITESTORAGEDB"));
    if (dbFile.isEmpty()) {
        QDir databaseDir(privilegedDataDir);
        if (databaseDir.exists() && directoryIsRW(privilegedDataDir)) {
            databaseDir = privilegedDataDir + QLatin1String("Calendar/mkcal/");
        } else {
            databaseDir = unprivilegedDataDir + QLatin1String("Calendar/mkcal/");
        }

        if (!databaseDir.exists() && !databaseDir.mkpath(QString::fromLatin1("."))) {
            qCWarning(lcMkcal) << "Unable to create calendar database directory:" << databaseDir.path();
        }

        dbFile = databaseDir.absoluteFilePath(QLatin1String("db"));
    }

    SqliteStorage::Ptr ss = SqliteStorage::Ptr(new SqliteStorage(calendar, dbFile));

    return ss.staticCast<ExtendedStorage>();
}

Todo::List ExtendedCalendar::uncompletedTodos(bool hasDate, int hasGeo)
{
    Todo::List list;

    const Todo::List todos(rawTodos());
    for (const Todo::Ptr &todo: todos) {
        if (!todo->isCompleted()) {
            if ((hasDate && todo->hasDueDate()) || (!hasDate && !todo->hasDueDate())) {
                if (hasGeo < 0 || (hasGeo && todo->hasGeo()) || (!hasGeo && !todo->hasGeo())) {
                    list.append(todo);
                }
            }
        }
    }
    return list;
}

static bool isDateInRange(const KDateTime &dt,
                          const KDateTime &start, const KDateTime &end)
{
    return ((!start.isValid() || start <= dt) &&
            (!end.isValid() || end >= dt));
}

static bool isDateSpanInRange(const KDateTime &dtStart, const KDateTime &dtEnd,
                              const KDateTime &start, const KDateTime &end)
{
    return ((!start.isValid() || start <= dtEnd) &&
            (!end.isValid() || end >= dtStart));
}

Todo::List ExtendedCalendar::completedTodos(bool hasDate, int hasGeo,
                                            const KDateTime &start, const KDateTime &end)
{
    Todo::List list;

    const Todo::List todos(rawTodos());
    for (const Todo::Ptr &todo: todos) {
        if (todo->isCompleted()) {
            if (hasDate && todo->hasDueDate()) {
                if (hasGeo < 0 || (hasGeo && todo->hasGeo()) || (!hasGeo && !todo->hasGeo())) {
                    if ((!todo->recurs() && isDateInRange(todo->dtDue(), start, end))
                        || (todo->recurs() && (todo->recurrence()->duration() == -1
                                               || isDateInRange(todo->recurrence()->endDateTime(), start, end)))) {
                        list.append(todo);
                    }
                }
            } else if (!hasDate && !todo->hasDueDate()) {   // todos without due date
                if (hasGeo < 0 || (hasGeo && todo->hasGeo()) || (!hasGeo && !todo->hasGeo())) {
                    if (isDateInRange(todo->created(), start, end)) {
                        list.append(todo);
                    }
                }
            }
        }
    }
    return list;
}

static bool isEventInRange(const Event::Ptr &event, int hasDate,
                           const KDateTime &start, const KDateTime &end)
{
    if ((hasDate < 0 || hasDate) &&
        event->dtStart().isValid() && event->dtEnd().isValid()) {
        if ((!event->recurs() && isDateSpanInRange(event->dtStart(), event->dtEnd(), start, end))
            || (event->recurs() && (event->recurrence()->duration() == -1
                                    || isDateInRange(event->recurrence()->endDateTime(), start, end)))) {
            return true;
        }
    } else if ((hasDate < 0 || !hasDate) &&  // events without valid dates
               (!event->dtStart().isValid() || !event->dtEnd().isValid())) {
        if (isDateInRange(event->created(), start, end)) {
            return true;
        }
    }
    return false;
}

static bool isTodoInRange(const Todo::Ptr &todo, int hasDate,
                          const KDateTime &start, const KDateTime &end)
{
    if ((hasDate < 0 || hasDate) && todo->hasDueDate()) {    // todos with due date
        if ((!todo->recurs() && isDateInRange(todo->dtDue(), start, end))
            || (todo->recurs() && (todo->recurrence()->duration() == -1
                                   || isDateInRange(todo->recurrence()->endDateTime(), start, end)))) {
            return true;
        }
    } else if ((hasDate < 0 || !hasDate) && !todo->hasDueDate()) {   // todos without due date
        if (isDateInRange(todo->created(), start, end)) {
            return true;
        }
    }
    return false;
}

static bool isJournalInRange(const Journal::Ptr &journal, int hasDate,
                             const KDateTime &start, const KDateTime &end)
{
    if ((hasDate < 0 || hasDate) && journal->dtStart().isValid()) {
        if ((!journal->recurs() && isDateInRange(journal->dtStart(), start, end))
            || (journal->recurs() && (journal->recurrence()->duration() == -1
                                      || isDateInRange(journal->recurrence()->endDateTime(), start, end)))) {
            return true;
        }
    } else if ((hasDate < 0 || !hasDate) && !journal->dtStart().isValid()) { // journals without valid dates
        if (isDateInRange(journal->created(), start, end)) {
            return true;
        }
    }
    return false;
}

Incidence::List ExtendedCalendar::incidences(bool hasDate,
                                             const KDateTime &start, const KDateTime &end)
{
    Incidence::List list;

    Todo::List todos = rawTodos();
    std::copy_if(todos.constBegin(), todos.constEnd(),
                 std::back_inserter(list),
                 [hasDate, start, end] (const Todo::Ptr &todo) {
                     return isTodoInRange(todo, hasDate ? 1 : 0, start, end);});

    Event::List events = rawEvents();
    std::copy_if(events.constBegin(), events.constEnd(),
                 std::back_inserter(list),
                 [hasDate, start, end] (const Event::Ptr &event) {
                     return isEventInRange(event, hasDate ? 1 : 0, start, end);});

    Journal::List journals = rawJournals();
    std::copy_if(journals.constBegin(), journals.constEnd(),
                 std::back_inserter(list),
                 [hasDate, start, end] (const Journal::Ptr &journal) {
                     return isJournalInRange(journal, hasDate ? 1 : 0, start, end);});

    return list;
}

Journal::List ExtendedCalendar::journals(const QDate &start, const QDate &end)
{
    Journal::List journalList;
    KDateTime startK(start);
    KDateTime endK(end);

    const Journal::List journals(rawJournals());
    for (const Journal::Ptr &journal: journals) {
        KDateTime st = journal->dtStart();
        // If start time is not valid, try to use the creation time.
        if (!st.isValid())
            st = journal->created();
        if (!st.isValid())
            continue;
        if (startK.isValid() && st < startK)
            continue;
        if (endK.isValid() && st > endK)
            continue;
        journalList << journal;
    }
    return journalList;
}

Incidence::List ExtendedCalendar::geoIncidences(bool hasDate,
                                                const KDateTime &start, const KDateTime &end)
{
    Incidence::List list;

    for (Incidence::Ptr incidence: const_cast<const Incidence::List&>(d->mGeoIncidences)) {
        if (incidence->type() == Incidence::TypeTodo) {
            if (isTodoInRange(incidence.staticCast<Todo>(), hasDate ? 1 : 0, start, end)) {
                list.append(incidence);
            }
        } else if (incidence->type() == Incidence::TypeEvent) {
            if (isEventInRange(incidence.staticCast<Event>(), hasDate ? 1 : 0, start, end)) {
                list.append(incidence);
            }
        }
    }
    return list;
}

#if 0
Incidence::List ExtendedCalendar::unreadInvitationIncidences(const KCalCore::Person::Ptr &person)
{
    Incidence::List list;

    const Todo::List todos(rawTodos());
    for (const Todo::Ptr &todo: todos) {
        if (todo->invitationStatus() == IncidenceBase::StatusUnread) {
            if (!person || person->email() == todo->organizer()->email() ||
                    todo->attendeeByMail(person->email())) {
                list.append(todo);
            }
        }
    }
    const Event::List events(rawEvents());
    for (const Event::Ptr &event: events) {
        if (event->invitationStatus() == IncidenceBase::StatusUnread) {
            if (!person || person->email() == event->organizer()->email() ||
                    event->attendeeByMail(person->email())) {
                list.append(event);
            }
        }
    }
    const Journal::List journals(rawJournals());
    for (const Journal::Ptr &journal: journals) {
        if (journal->invitationStatus() == IncidenceBase::StatusUnread) {
            if (person || person->email() == journal->organizer()->email() ||
                    journal->attendeeByMail(person->email())) {
                list.append(journal);
            }
        }
    }
    return list;
}

Incidence::List ExtendedCalendar::oldInvitationIncidences(const KDateTime &start,
                                                          const KDateTime &end)
{
    Incidence::List list;

    const Todo::List todos(rawTodos());
    for (const Todo::Ptr &todo: todos) {
        if (todo->invitationStatus() > IncidenceBase::StatusUnread
            && isDateInRange(todo->created(), strat, end)) {
            list.append(todo);
        }
    }
    const Event::List events(rawEvents());
    for (const Event::Ptr &event: events) {
        if (event->invitationStatus() > IncidenceBase::StatusUnread
            && isDateInRange(event->created(), start, end)) {
            list.append(event);
        }
    }
    const Journal::List journals(rawJournals());
    for (const Journal::Ptr &journal: journals) {
        if (journal->invitationStatus() > IncidenceBase::StatusUnread
            && isDateInRange(journal->created(), start, end)) {
            list.append(journal);
        }
    }
    return list;
}
#endif

Incidence::List ExtendedCalendar::contactIncidences(const Person::Ptr &person,
                                                    const KDateTime &start, const KDateTime &end)
{
    Incidence::List list;
    const QList<Incidence::Ptr> incidences(d->mAttendeeIncidences.values(person->email()));
    for (const Incidence::Ptr &incidence: incidences) {
        if (incidence->type() == Incidence::TypeEvent) {
            if (isEventInRange(incidence.staticCast<Event>(), -1, start, end)) {
                list.append(incidence);
            }
        } else if (incidence->type() == Incidence::TypeTodo) {
            if (isTodoInRange(incidence.staticCast<Todo>(), -1, start, end)) {
                list.append(incidence);
            }
        } else if (incidence->type() == Incidence::TypeJournal) {
            if (isJournalInRange(incidence.staticCast<Journal>(), -1, start, end)) {
                list.append(incidence);
            }
        }
    }
    return list;
}

Incidence::List ExtendedCalendar::addIncidences(Incidence::List *incidenceList,
                                                const QString &notebookUid,
                                                bool duplicateRemovalEnabled)
{
    Incidence::List returnList;
    Incidence::List duplicatesList;
    Incidence::List::Iterator iit;
    Incidence::List::Iterator dit;

    for (iit = incidenceList->begin(); iit != incidenceList->end(); ++iit) {
        duplicatesList = duplicates(*iit);
        if (!duplicatesList.isEmpty()) {
            if (duplicateRemovalEnabled) {
                for (dit = duplicatesList.begin(); dit != duplicatesList.end(); ++dit) {
                    deleteIncidence(*dit);
                }
            } else {
                continue;
            }
        }

        addIncidence(*iit);
        setNotebook(*iit, notebookUid);
        returnList.append(*iit);
    }

    return returnList;
}

void ExtendedCalendar::storageModified(ExtendedStorage *storage, const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);

    clearNotebookAssociations();

    // Despite the strange name, close() method does exactly what we
    // want - clears the in-memory contents of the calendar.
    close();
}

void ExtendedCalendar::storageProgress(ExtendedStorage *storage, const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);
}

void ExtendedCalendar::storageFinished(ExtendedStorage *storage, bool error, const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(error);
    Q_UNUSED(info);
}

int ExtendedCalendar::eventCount(const QString &notebookUid)
{
    Event::List events = rawEvents();

    if (notebookUid.isEmpty())
        return events.size();

    return std::count_if(events.constBegin(), events.constEnd(),
                         [this, notebookUid] (const Event::Ptr &event)
                         {return notebook(event) == notebookUid;});
}

int ExtendedCalendar::todoCount(const QString &notebookUid)
{
    Todo::List todos = rawTodos();

    if (notebookUid.isEmpty())
        return todos.size();

    return std::count_if(todos.constBegin(), todos.constEnd(),
                         [this, notebookUid] (const Todo::Ptr &todo)
                         {return notebook(todo) == notebookUid;});
}

int ExtendedCalendar::journalCount(const QString &notebookUid)
{
    Journal::List journals = rawJournals();

    if (notebookUid.isEmpty())
        return journals.size();

    return std::count_if(journals.constBegin(), journals.constEnd(),
                         [this, notebookUid] (const Journal::Ptr &journal)
                         {return notebook(journal) == notebookUid;});
}

