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
#include "extendedstorage.h"
#include "logging_p.h"

#include <KCalendarCore/CalFilter>
#include <KCalendarCore/CalFormat>
#include <KCalendarCore/Sorting>
using namespace KCalendarCore;

#include <cmath>

// #ifdef to control expensive/spammy debug stmts
#undef DEBUG_EXPANSION

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
    static QDateTime incidenceRecurrenceStart(const KCalendarCore::Incidence::Ptr &incidence, const QDateTime start);

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
    static QDateTime incidenceEndTime(const KCalendarCore::Incidence::Ptr &incidence,
                                      const QDateTime start,
                                      bool endWithinDay);
};

ExtendedCalendar::ExtendedCalendar(const QTimeZone &timeZone)
    : MemoryCalendar(timeZone), d(new mKCal::ExtendedCalendar::Private)
{
}

ExtendedCalendar::ExtendedCalendar(const QByteArray &timeZoneId)
    : MemoryCalendar(timeZoneId), d(new mKCal::ExtendedCalendar::Private)
{
}

ExtendedCalendar::~ExtendedCalendar()
{
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
    d->mGeoIncidences.clear();
    d->mAttendeeIncidences.clear();
    MemoryCalendar::close();
}

// ICalTimeZone ExtendedCalendar::parseZone(MSTimeZone *tz)
// {
//     ICalTimeZone zone;

//     ICalTimeZones *icalZones = timeZones();
//     if (icalZones) {
//         ICalTimeZoneSource src;
//         zone = src.parse(tz, *icalZones);
//     }
//     return zone;
// }

// Dissociate a single occurrence or all future occurrences from a recurring
// sequence. The new incidence is returned, but not automatically inserted
// into the calendar, which is left to the calling application.
Incidence::Ptr ExtendedCalendar::dissociateSingleOccurrence(const Incidence::Ptr &incidence,
                                                            const QDateTime &dateTime)
{
    if (!incidence) {
        return Incidence::Ptr();
    }

    // Don't save milliseconds
    QDateTime recId(dateTime);
    recId.setTime(QTime(recId.time().hour(),
                        recId.time().minute(),
                        recId.time().second()));

    if (!incidence->allDay()) {
        if (!incidence->recursAt(recId)) {
            return Incidence::Ptr();
        }
    } else {
        if (!incidence->recursOn(recId.date(), recId.timeZone())) {
            return Incidence::Ptr();
        }
    }
    const Incidence::List exceptions = instances(incidence);
    for (const Incidence::Ptr &exception : exceptions) {
        if (exception->recurrenceId() == dateTime) {
            qCWarning(lcMkcal) << "Exception already exists, cannot dissociate.";
            return Incidence::Ptr();
        }
    }

    Incidence::Ptr newInc = Calendar::createException(incidence, recId);
    if (newInc) {
        newInc->setSchedulingID(QString());
        incidence->setLastModified(newInc->created());
    }

    return newInc;
}

bool ExtendedCalendar::addIncidence(const Incidence::Ptr &incidence)
{
    // Need to by-pass the override done in MemoryCalendar to get back
    // the genericity of the call implemented in the Calendar class.
    return Calendar::addIncidence(incidence);
}

bool ExtendedCalendar::addIncidence(const Incidence::Ptr &incidence, const QString &notebookUid)
{
    if (!incidence) {
        return false;
    }

    switch (incidence->type()) {
    case IncidenceBase::TypeEvent:
        return addEvent(incidence.staticCast<Event>(), notebookUid);
    case IncidenceBase::TypeTodo:
        return addTodo(incidence.staticCast<Todo>(), notebookUid);
    case IncidenceBase::TypeJournal:
        return addJournal(incidence.staticCast<Journal>(), notebookUid);
    default:
        qCWarning(lcMkcal) << "Unsupported type in addIncidence().";
    }
    return false;
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

    if (aEvent->uid().isEmpty()) {
        qCWarning(lcMkcal) << "adding an event without uid, creating one.";
        aEvent->setUid(CalFormat::createUniqueId());
    } else if (MemoryCalendar::event(aEvent->uid(), aEvent->recurrenceId())) {
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

    if (aTodo->uid().isEmpty()) {
        qCWarning(lcMkcal) << "adding a todo without uid, creating one.";
        aTodo->setUid(CalFormat::createUniqueId());
    } else {
        Todo::Ptr old = MemoryCalendar::todo(aTodo->uid(), aTodo->recurrenceId());
        if (old) {
            if (aTodo->revision() > old->revision()) {
                deleteTodo(old);   // move old to deleted
            } else {
                qCDebug(lcMkcal) << "Duplicate found, todo was not added";
                return false;
            }
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

void ExtendedCalendar::incidenceUpdate(const QString &uid, const QDateTime &recurrenceId)
{
    // The static_cast is ok as the ExtendedCalendar only observes Incidence objects
    Incidence::Ptr incidence = this->incidence(uid, recurrenceId);

    if (!incidence) {
        return;
    }

    d->removeIncidenceFromLists(incidence);
    MemoryCalendar::incidenceUpdate(uid, recurrenceId);
}

void ExtendedCalendar::incidenceUpdated(const QString &uid, const QDateTime &recurrenceId)
{
    Incidence::Ptr incidence = this->incidence(uid, recurrenceId);

    if (!incidence) {
        return;
    }

    d->addIncidenceToLists(incidence);
    MemoryCalendar::incidenceUpdated(uid, recurrenceId);
}

QDate ExtendedCalendar::nextEventsDate(const QDate &date, const QTimeZone &timeZone)
{
    const QTimeZone &tz = timeZone.isValid() ? timeZone : this->timeZone();

    QDateTime kdt(date, QTime(0, 0, 0), tz);
    QDateTime tomorrow = kdt.addDays(1);
    QDateTime almostTomorrow = tomorrow.addSecs(-1);

    QDateTime rv;

    const Event::List &events(rawEvents());
    for (const Event::Ptr &ev: events) {
        if (!isVisible(ev)) {
            continue;
        }
        if (ev->recurs()) {
            if (ev->isMultiDay()) {
                int extraDays = ev->dtStart().date().daysTo(ev->dtEnd().date());
                for (int i = 0; i <= extraDays; ++i) {
                    if (ev->recursOn(date.addDays(1 - i), tz))
                        return tomorrow.toTimeZone(tz).date();
                }
            }

            QDateTime next = ev->recurrence()->getNextDateTime(almostTomorrow);
            next.setTime(QTime(0, 0, 0));

            if (!rv.isValid() || next < rv)
                rv = next;
        } else if (ev->isMultiDay()) {
            QDateTime edate = ev->dtStart();
            edate.setTime(QTime(0, 0, 0));
            if (edate > kdt) {
                if (!rv.isValid() || edate < rv)
                    rv = edate;
            } else {
                edate = ev->dtEnd();
                edate.setTime(QTime(0, 0, 0));
                if (edate > kdt)
                    rv = tomorrow;
            }
        } else {
            QDateTime edate = ev->dtStart();
            edate.setTime(QTime(0, 0, 0));
            if (edate > kdt && (!rv.isValid() || edate < rv))
                rv = edate;
        }

        if (rv == tomorrow)
            break; // Bail early - you can't beat tomorrow
    }

    if (!rv.isValid())
        return QDate();
    else
        return rv.toTimeZone(tz).date();
}

QDate ExtendedCalendar::previousEventsDate(const QDate &date, const QTimeZone &timeZone)
{
    const QTimeZone &tz = timeZone.isValid() ? timeZone : this->timeZone();

    QDateTime kdt(date, QTime(0, 0, 0), tz);
    QDateTime yesterday = kdt.addDays(-1);

    QDateTime rv;

    const Event::List events(rawEvents());
    for (const Event::Ptr &ev: events) {
        if (!isVisible(ev)) {
            continue;
        }
        if (ev->recurs()) {
            QDateTime prev = ev->recurrence()->getPreviousDateTime(kdt);
            prev.setTime(QTime(0, 0, 0));

            if (ev->isMultiDay()) {
                prev = prev.addDays(ev->dtStart().date().daysTo(ev->dtEnd().date()));
                if (prev >= kdt)
                    return yesterday.toTimeZone(tz).date();
            }

            if (!rv.isValid() || prev > rv)
                rv = prev;
        } else if (ev->isMultiDay()) {
            QDateTime edate = ev->dtEnd();
            edate.setTime(QTime(0, 0, 0));
            if (edate < kdt) {
                if (!rv.isValid() || edate > rv)
                    rv = edate;
            } else {
                edate = ev->dtStart();
                edate.setTime(QTime(0, 0, 0));
                if (edate < kdt)
                    rv = yesterday;
            }
        } else {
            QDateTime edate = ev->dtStart();
            edate.setTime(QTime(0, 0, 0));
            if (edate < kdt && (!rv.isValid() || edate > rv))
                rv = edate;
        }

        if (rv == yesterday)
            break; // Bail early - you can't beat tomorrow
    }

    if (!rv.isValid())
        return QDate();
    else
        return rv.toTimeZone(tz).date();
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

    if (aJournal->uid().isEmpty()) {
        qCWarning(lcMkcal) << "adding a journal without uid, creating one.";
        aJournal->setUid(CalFormat::createUniqueId());
    } else {
        Journal::Ptr old = journal(aJournal->uid(), aJournal->recurrenceId());
        if (old) {
            if (aJournal->revision() > old->revision()) {
                deleteJournal(old);   // move old to deleted
            } else {
                qCDebug(lcMkcal) << "Duplicate found, journal was not added";
                return false;
            }
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

Journal::List ExtendedCalendar::rawJournals(const QDate &start, const QDate &end,
                                            const QTimeZone &timeZone, bool inclusive) const
{
    Q_UNUSED(inclusive);
    Journal::List journalList;
    const QTimeZone &tz = timeZone.isValid() ? timeZone : this->timeZone();
    QDateTime st(start, QTime(0, 0, 0), tz);
    QDateTime nd(end, QTime(23, 59, 999), tz);

    // Get journals
    const Journal::List journals(rawJournals());
    for (const Journal::Ptr &journal: journals) {
        if (!isVisible(journal)) {
            continue;
        }
        QDateTime rStart = journal->dtStart();
        if (nd.isValid() && nd < rStart) {
            continue;
        }
        if (inclusive && st.isValid() && rStart < st) {
            continue;
        }

        if (!journal->recurs()) {   // non-recurring journals
            // TODO_ALVARO: journals don't have endDt, bug?
            QDateTime rEnd = journal->dateTime(Incidence::RoleEnd);
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
                QDateTime rEnd(journal->recurrence()->endDate(), QTime(23, 59, 999), tz);
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
                                             const QList<KCalendarCore::Incidence::IncidenceType> &types)
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
    const Event::List events = rawEvents();
    for (const Event::Ptr &ev : events) {
        notifyIncidenceDeleted(ev);
    }
    const Todo::List todos = rawTodos();
    for (const Todo::Ptr &todo : todos) {
        notifyIncidenceDeleted(todo);
    }
    const Journal::List journals = rawJournals();
    for (const Journal::Ptr &journal : journals) {
        notifyIncidenceDeleted(journal);
    }
    close();
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
    const Person &organizer = incidence->organizer();
    if (!organizer.isEmpty()) {
        mAttendeeIncidences.insert(organizer.email(), incidence);
    }
    const Attendee::List &list = incidence->attendees();
    Attendee::List::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        mAttendeeIncidences.insert(it->email(), incidence);
    }
    if (incidence->hasGeo()) {
        mGeoIncidences.append(incidence);
    }
}

void ExtendedCalendar::Private::removeIncidenceFromLists(const Incidence::Ptr &incidence)
{
    const Person &organizer = incidence->organizer();
    if (!organizer.isEmpty()) {
        mAttendeeIncidences.remove(organizer.email(), incidence);
    }
    const Attendee::List &list = incidence->attendees();
    Attendee::List::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        mAttendeeIncidences.remove(it->email(), incidence);
    }
    if (incidence->hasGeo()) {
        mGeoIncidences.removeAll(incidence);
    }
}

QDateTime ExtendedCalendar::Private::incidenceRecurrenceStart(const KCalendarCore::Incidence::Ptr &incidence,
                                                              const QDateTime ost)
{
    if (!incidence)
        return QDateTime();

    if (!incidence->recurs())
        return incidence->dtStart().toTimeSpec(Qt::LocalTime);

    // Then figure how far off from the start of this recurrence we are
    QDateTime dt(ost.addSecs(1));
    return incidence->recurrence()->getPreviousDateTime(dt).toTimeSpec(Qt::LocalTime);
}

QDateTime ExtendedCalendar::Private::incidenceEndTime(const KCalendarCore::Incidence::Ptr &incidence,
                                                      const QDateTime ost,
                                                      bool endWithinDay)
{
    if (!incidence)
        return QDateTime();

    // First off, figure how long the initial event is
    QDateTime dtS(incidence->dtStart());
    QDateTime dtE(incidence->dateTime(Incidence::RoleEnd));
    int duration = dtE.toTime_t() - dtS.toTime_t();

    QDateTime dt(ost);
    QDateTime start(incidenceRecurrenceStart(incidence, ost));

    int duration0 = dt.toTime_t() - start.toTime_t();

    int left = duration - duration0;

    QDateTime r = dt.addSecs(left);
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
    Incidence::List *incidenceList, const QDateTime &dtStart, const QDateTime &dtEnd, int maxExpand, bool *expandLimitHit)
{
    ExtendedCalendar::ExpandedIncidenceList returnList;
    Incidence::List::Iterator iit;
    QDateTime brokenDtStart = dtStart.addSecs(-1);
    const QTimeZone &tz = timeZone();
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
        QDateTime dt = (*iit)->dtStart().toTimeSpec(Qt::LocalTime);
        QDateTime dte = (*iit)->dateTime(IncidenceBase::RoleEndRecurrenceBase);
        int appended = 0;
        int skipped = 0;
        bool brokenEnd = false;

        if ((*iit)->type() == Incidence::TypeTodo) {
            Todo::Ptr todo = (*iit).staticCast<Todo>();
            if (todo->hasDueDate()) {
                dt = todo->dtDue().toTimeSpec(Qt::LocalTime);
            }
        }

        if (!dt.isValid()) {
            // Just leave the dateless incidences there (they will be
            // sorted out)
            validity.dtStart = dt;
            validity.dtEnd = d->incidenceEndTime(*iit, dt, true);
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
                        validity.dtStart = dt;
                        validity.dtEnd = d->incidenceEndTime(*iit, dt, true);
                        returnList.append(ExpandedIncidence(validity, *iit));
                        appended++;
                    }
                } else {
                    if (!(*iit)->recursOn((*iit)->dtStart().date(), tz)) {
#ifdef DEBUG_EXPANSION
                        qCDebug(lcMkcal) << "--not recurring on" << (*iit)->dtStart() << (*iit)->summary();
#endif /* DEBUG_EXPANSION */
                    } else {
                        validity.dtStart = dt;
                        validity.dtEnd = d->incidenceEndTime(*iit, dt, true);
                        returnList.append(ExpandedIncidence(validity, *iit));
                        appended++;
                    }
                }
            } else {
                validity.dtStart = dt;
                validity.dtEnd = d->incidenceEndTime(*iit, dt, true);
                returnList.append(ExpandedIncidence(validity, *iit));
                appended++;
            }
        } else {
#ifdef DEBUG_EXPANSION
            qCDebug(lcMkcal) << "-- no match" << dt.toString() << dte.toString() << dte.isValid() << brokenEnd;
#endif /* DEBUG_EXPANSION */
        }

        if ((*iit)->recurs()) {
            QDateTime dtr = dt;
            QDateTime dtro;

            // If the original entry wasn't part of the time window, try to
            // get more appropriate first item to add. Else, start the
            // next-iteration from the 'dt' (=current item).
            if (!appended) {
                dtr = (*iit)->recurrence()->getPreviousDateTime(dtStart);
                if (dtr.isValid()) {
                    QDateTime dtr2 = (*iit)->recurrence()->getPreviousDateTime(dtr);
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
            QDateTime dtStartMinusDuration = dtStart.addSecs(-duration);

            while (appended < maxExpand) {
                dtro = dtr;
                dtr = (*iit)->recurrence()->getNextDateTime(dtr).toTimeSpec(Qt::LocalTime);
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
                    validity.dtStart = dtr;
                    validity.dtEnd = d->incidenceEndTime(*iit, dtr, true);
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

        QDateTime dts = inc->dtStart().toTimeSpec(Qt::LocalTime);
        QDateTime dte = inc->dateTime(IncidenceBase::RoleEndRecurrenceBase).toTimeSpec(Qt::LocalTime).addSecs(
                            -1); // inclusive, all-day events end on first sec of next day

        int days = 1;
        while (dts.date() < dte.date()) {
            days++;
            dte = dte.addDays(-1);
        }

        // Initialize dts/dte to the current recurrence (if any)
        dts = QDateTime(ei.first.dtStart.date(), dts.time());
        dte = QDateTime(ei.first.dtStart.date().addDays(1), QTime(0, 0, 0));

        int added = 0;
        for (i = 0 ; i < days ; i++) {
            if (i || merge) {
                // Possibly add the currently iterated one.
                // Have to check it against time boundaries using the dts/dte, though
                if ((!startDate.isValid() || startDate < dte.date())
                        && (!endDate.isValid() || endDate >= dts.date())) {
                    validity.dtStart = dts;
                    validity.dtEnd = d->incidenceEndTime(inc, dts, true);
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

ExtendedStorage::Ptr ExtendedCalendar::defaultStorage(const ExtendedCalendar::Ptr &calendar)
{
    return ExtendedStorage::Ptr(new ExtendedStorage(calendar));
}

Todo::List ExtendedCalendar::uncompletedTodos(bool hasDate, int hasGeo)
{
    Todo::List list;

    const Todo::List todos(rawTodos());
    for (const Todo::Ptr &todo: todos) {
        if (isVisible(todo) && !todo->isCompleted()) {
            if ((hasDate && todo->hasDueDate()) || (!hasDate && !todo->hasDueDate())) {
                if (hasGeo < 0 || (hasGeo && todo->hasGeo()) || (!hasGeo && !todo->hasGeo())) {
                    list.append(todo);
                }
            }
        }
    }
    return list;
}

static bool isDateInRange(const QDateTime &dt,
                          const QDateTime &start, const QDateTime &end)
{
    return ((!start.isValid() || start <= dt) &&
            (!end.isValid() || end >= dt));
}

static bool isDateSpanInRange(const QDateTime &dtStart, const QDateTime &dtEnd,
                              const QDateTime &start, const QDateTime &end)
{
    return ((!start.isValid() || start <= dtEnd) &&
            (!end.isValid() || end >= dtStart));
}

Todo::List ExtendedCalendar::completedTodos(bool hasDate, int hasGeo,
                                            const QDateTime &start, const QDateTime &end)
{
    Todo::List list;

    const Todo::List todos(rawTodos());
    for (const Todo::Ptr &todo: todos) {
        if (isVisible(todo) && todo->isCompleted()) {
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
                           const QDateTime &start, const QDateTime &end)
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
                          const QDateTime &start, const QDateTime &end)
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
                             const QDateTime &start, const QDateTime &end)
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
                                             const QDateTime &start, const QDateTime &end)
{
    Incidence::List list;

    Todo::List todos = rawTodos();
    std::copy_if(todos.constBegin(), todos.constEnd(),
                 std::back_inserter(list),
                 [this, hasDate, start, end] (const Todo::Ptr &todo) {
                     return isVisible(todo) && isTodoInRange(todo, hasDate ? 1 : 0, start, end);});

    Event::List events = rawEvents();
    std::copy_if(events.constBegin(), events.constEnd(),
                 std::back_inserter(list),
                 [this, hasDate, start, end] (const Event::Ptr &event) {
                     return isVisible(event) && isEventInRange(event, hasDate ? 1 : 0, start, end);});

    Journal::List journals = rawJournals();
    std::copy_if(journals.constBegin(), journals.constEnd(),
                 std::back_inserter(list),
                 [this, hasDate, start, end] (const Journal::Ptr &journal) {
                     return isVisible(journal) && isJournalInRange(journal, hasDate ? 1 : 0, start, end);});

    return list;
}

Journal::List ExtendedCalendar::journals(const QDate &start, const QDate &end)
{
    Journal::List journalList;
    QDateTime startK(start);
    QDateTime endK(end);

    const Journal::List journals(rawJournals());
    for (const Journal::Ptr &journal: journals) {
        if (!isVisible(journal)) {
            continue;
        }
        QDateTime st = journal->dtStart();
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
                                                const QDateTime &start, const QDateTime &end)
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

Incidence::List ExtendedCalendar::oldInvitationIncidences(const QDateTime &start,
                                                          const QDateTime &end)
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

Incidence::List ExtendedCalendar::contactIncidences(const Person &person,
                                                    const QDateTime &start, const QDateTime &end)
{
    Incidence::List list;
    const QList<Incidence::Ptr> incidences(d->mAttendeeIncidences.values(person.email()));
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

