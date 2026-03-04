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

#include <KCalendarCore/CalFormat>
using namespace KCalendarCore;

using namespace mKCal;

class mKCal::ExtendedCalendar::Private
{
public:
    Private()
    {
    }
    ~Private()
    {
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Deprecated notebook API.
    QMultiHash<QString, Incidence::Ptr> mNotebookIncidences;
    QHash<QString, QString> mUidToNotebook;
    QHash<QString, bool> mNotebooks; // name to visibility
    QHash<Incidence::Ptr, bool> mIncidenceVisibility; // incidence -> visibility
    QString mDefaultNotebook; // uid of default notebook
#endif
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void ExtendedCalendar::close()
{
    d->mNotebookIncidences.clear();
    d->mUidToNotebook.clear();
    d->mIncidenceVisibility.clear();

    MemoryCalendar::close();
}
#endif

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

    if (aEvent->uid().length() < 7) {
        qCWarning(lcMkcal) << "adding an event without uid, creating one.";
        aEvent->setUid(CalFormat::createUniqueId());
    } else if (MemoryCalendar::event(aEvent->uid(), aEvent->recurrenceId())) {
        qCDebug(lcMkcal) << "Duplicate found, event was not added";
        return false;
    }

    if (MemoryCalendar::addIncidence(aEvent)) {
        return setNotebook(aEvent, notebookUid);
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
        return setNotebook(aTodo, notebookUid);
    } else {
        return false;
    }
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
        return setNotebook(aJournal, notebookUid);
    } else {
        return false;
    }
}

Incidence::List ExtendedCalendar::incidences(const QDate &start, const QDate &end)
{
    return mergeIncidenceList(events(start, end), todos(start, end), journals(start, end));
}

ExtendedStorage::Ptr ExtendedCalendar::defaultStorage(const ExtendedCalendar::Ptr &calendar)
{
    SqliteStorage::Ptr ss = SqliteStorage::Ptr(new SqliteStorage(calendar));

    return ss.staticCast<ExtendedStorage>();
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// Deprecated notebook API.
Incidence::List ExtendedCalendar::duplicates(const Incidence::Ptr &incidence)
{
    if (!incidence) {
        return {};
    }

    Incidence::List list;
    const Incidence::List vals = rawIncidences();
    std::copy_if(vals.cbegin(), vals.cend(), std::back_inserter(list), [&](const Incidence::Ptr &in) {
        return (incidence->dtStart() == in->dtStart() || (!incidence->dtStart().isValid() && !in->dtStart().isValid()))
            && incidence->summary() == in->summary();
    });
    return list;
}

bool ExtendedCalendar::addNotebook(const QString &notebook, bool isVisible)
{
    if (d->mNotebooks.contains(notebook)) {
        return false;
    } else {
        d->mNotebooks.insert(notebook, isVisible);
        return true;
    }
}

bool ExtendedCalendar::updateNotebook(const QString &notebook, bool isVisible)
{
    if (!d->mNotebooks.contains(notebook)) {
        return false;
    } else {
        d->mNotebooks.insert(notebook, isVisible);

        for (auto  noteIt = d->mNotebookIncidences.find(notebook);
             noteIt != d->mNotebookIncidences.end() && noteIt.key() == notebook;
             ++noteIt) {
            const Incidence::Ptr &incidence = noteIt.value();
            auto visibleIt = d->mIncidenceVisibility.find(incidence);
            if (visibleIt != d->mIncidenceVisibility.end()) {
                *visibleIt = isVisible;
            }
        }
        return true;
    }
}

bool ExtendedCalendar::deleteNotebook(const QString &notebook)
{
    if (!d->mNotebooks.contains(notebook)) {
        return false;
    } else {
        return d->mNotebooks.remove(notebook);
    }
}

bool ExtendedCalendar::setDefaultNotebook(const QString &notebook)
{
    if (!d->mNotebooks.contains(notebook)) {
        return false;
    } else {
        d->mDefaultNotebook = notebook;
        return true;
    }
}

QString ExtendedCalendar::defaultNotebook() const
{
    return d->mDefaultNotebook;
}

bool ExtendedCalendar::hasValidNotebook(const QString &notebook) const
{
    return d->mNotebooks.contains(notebook);
}

bool ExtendedCalendar::isVisible(const Incidence::Ptr &incidence) const
{
    if (d->mIncidenceVisibility.contains(incidence)) {
        return d->mIncidenceVisibility[incidence];
    }
    const QString nuid = notebook(incidence);
    bool rv;
    if (d->mNotebooks.contains(nuid)) {
        rv = d->mNotebooks.value(nuid);
    } else {
        // NOTE returns true also for nonexisting notebooks for compatibility
        rv = true;
    }
    d->mIncidenceVisibility[incidence] = rv;
    return rv;
}

bool ExtendedCalendar::isVisible(const QString &notebook) const
{
    QHash<QString, bool>::ConstIterator it = d->mNotebooks.constFind(notebook);
    return (it != d->mNotebooks.constEnd()) ? *it : true;
}

void ExtendedCalendar::clearNotebookAssociations()
{
}

bool ExtendedCalendar::setNotebook(const Incidence::Ptr &inc, const QString &notebook)
{
    if (!inc) {
        return false;
    }

    if (!notebook.isEmpty() && !incidence(inc->uid(), inc->recurrenceId())) {
        qCWarning(lcMkcal) << "cannot set notebook until incidence has been added";
        return false;
    }

    if (d->mUidToNotebook.contains(inc->uid())) {
        QString old = d->mUidToNotebook.value(inc->uid());
        if (!old.isEmpty() && notebook != old) {
            if (inc->hasRecurrenceId()) {
                qCWarning(lcMkcal) << "cannot set notebook for child incidences";
                return false;
            }
            // Move all possible children also.
            const Incidence::List list = instances(inc);
            for (const auto &incidence : list) {
                d->mNotebookIncidences.remove(old, incidence);
                d->mNotebookIncidences.insert(notebook, incidence);
            }
            notifyIncidenceChanged(inc); // for removing from old notebook
            // do not remove from mUidToNotebook to keep deleted incidences
            d->mNotebookIncidences.remove(old, inc);
        }
    }
    if (!notebook.isEmpty()) {
        d->mUidToNotebook.insert(inc->uid(), notebook);
        d->mNotebookIncidences.insert(notebook, inc);
        qCDebug(lcMkcal) << "setting notebook" << notebook << "for" << inc->uid();
        notifyIncidenceChanged(inc); // for inserting into new notebook
        const Incidence::List list = instances(inc);
        for (const auto &incidence : list) {
            notifyIncidenceChanged(incidence);
        }
    }

    return true;
}

QString ExtendedCalendar::notebook(const Incidence::Ptr &incidence) const
{
    if (incidence) {
        return d->mUidToNotebook.value(incidence->uid());
    } else {
        return QString();
    }
}

QString ExtendedCalendar::notebook(const QString &uid) const
{
    return d->mUidToNotebook.value(uid);
}

QStringList ExtendedCalendar::notebooks() const
{
    return d->mNotebookIncidences.uniqueKeys();
}

Incidence::List ExtendedCalendar::incidences(const QString &notebook) const
{
    if (notebook.isEmpty()) {
        return rawIncidences();
    } else {
        const QList<Incidence::Ptr> incs = d->mNotebookIncidences.values(notebook);
        Incidence::List v;
        v.reserve(incs.size());
        for (QList<Incidence::Ptr>::ConstIterator it = incs.constBegin(), end = incs.constEnd();
             it != end; ++it) {
            v.push_back(*it);
        }
        return v;
    }
}
#endif
