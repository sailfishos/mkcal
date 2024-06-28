/*
  This file is part of the mkcal library.

  Copyright (c) 2023 Damien Caliste <dcaliste@free.fr>

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
  defines the MultiCalendarStorage abstract base class.

  @brief
  An abstract base class that provides a calendar storage interface.

  @author Cornelius Schumacher \<schumacher@kde.org\>
*/
#include "multicalendarstorage.h"
#include "sqlitemulticalendarstorage.h"
#include "alarmhandler_p.h"
#include "calendarhandler_p.h"
#include "logging_p.h"

using namespace KCalendarCore;

using namespace mKCal;

struct Range
{
    Range(const QDate &start, const QDate &end)
        : mStart(start), mEnd(end) { }

    bool contains(const QDate &at) const
    {
        return at.isValid()
            && (mStart.isNull() || at >= mStart)
            && (mEnd.isNull() || at <= mEnd);
    }

    QDate mStart, mEnd;
};

// Range a is strictly before range b.
static bool operator<(const Range &a, const Range &b)
{
    return a.mEnd.isValid() && b.mStart.isValid() && a.mEnd < b.mStart;
}
// Date a is strictly before range b.
static bool operator<(const QDate &at, const Range &range)
{
    return at.isNull() || (range.mStart.isValid() && at < range.mStart);
}

//@cond PRIVATE
class MultiCalendarStorage::Private: public AlarmHandler
{
public:
    Private(MultiCalendarStorage *storage, const QTimeZone &timezone)
        : mStorage(storage)
        , mTimezone(timezone)
    {}

    MultiCalendarStorage *mStorage;
    QList<MultiCalendarStorage::Observer *> mObservers;
    QHash<QString, CalendarHandler> mCalendars;
    QString mDefaultNotebookUid;
    QTimeZone mTimezone;
    QList<Range> mRanges;
    bool mIsRecurrenceLoaded = false;

    Incidence::List incidencesWithAlarms(const QString &notebookUid,
                                         const QString &uid) override;
};
//@endcond

MultiCalendarStorage::MultiCalendarStorage(const QTimeZone &timezone)
    : d(new Private(this, timezone))
{
}

MultiCalendarStorage::~MultiCalendarStorage()
{
    delete d;
}

bool MultiCalendarStorage::setTimeZone(const QTimeZone &timezone)
{
    bool changed = false;
    d->mTimezone = timezone;
    for (QHash<QString, CalendarHandler>::Iterator it = d->mCalendars.begin();
         it != d->mCalendars.end(); it++) {
        changed = changed || (it->calendar()->timeZone() != timezone);
        it->calendar()->setTimeZone(timezone);
    }
    return changed;
}

bool MultiCalendarStorage::open()
{
    for (const Notebook::Ptr &notebook : loadedNotebooks(&d->mDefaultNotebookUid)) {
        QHash<QString, CalendarHandler>::Iterator it = d->mCalendars.find(notebook->uid());
        if (it == d->mCalendars.end()) {
            it = d->mCalendars.insert(notebook->uid(), CalendarHandler(d->mTimezone));
        }
        it->setNotebook(notebook);
    }
    return true;
}

bool MultiCalendarStorage::close()
{
    d->mCalendars.clear();
    return true;
}

QString MultiCalendarStorage::multiCalendarIdentifier(const QString &notebookUid,
                                                      const Incidence &incidence)
{
    return multiCalendarIdentifier(notebookUid, incidence.instanceIdentifier());
}

QString MultiCalendarStorage::multiCalendarIdentifier(const QString &notebookUid,
                                                      const QString &identifier)
{
    return QString::fromLatin1("%1::NBUID::%2").arg(notebookUid).arg(identifier);
}

static QPair<QString, QString> deserialiseIdentifier(const QString &multiCalendarIdentifier)
{
    int at = multiCalendarIdentifier.indexOf(QString::fromLatin1("::NBUID::"));
    return at < 0 ? QPair<QString, QString>()
                  : QPair<QString, QString>(multiCalendarIdentifier.left(at), multiCalendarIdentifier.right(at + 9));
}

bool MultiCalendarStorage::loadIncidenceInstance(const QString &multiCalendarIdentifier)
{
    QPair<QString, QString> ids = deserialiseIdentifier(multiCalendarIdentifier);
    const QString notebookUid = ids.first;
    QString uid = ids.second;
    if (notebookUid.isEmpty() || uid.isEmpty()) {
        qCWarning(lcMkcal) << "invalid instance identifier" << multiCalendarIdentifier;
        return false;
    }

    // At the moment, from KCalendarCore, if the instance is an exception,
    // the instanceIdentifier will ends with yyyy-MM-ddTHH:mm:ss[Z|[+|-]HH:mm]
    // This is tested in tst_loadIncidenceInstance() to ensure that any
    // future breakage would be properly detected.
    if (uid.endsWith('Z')) {
        uid = uid.left(uid.length() - 20);
    } else if (uid.length() > 19 && uid[uid.length() - 9] == 'T') {
        uid = uid.left(uid.length() - 19);
    } else if (uid.length() > 25 && uid[uid.length() - 3] == ':') {
        uid = uid.left(uid.length() - 25);
    }

    // Even if we're looking for a specific incidence instance, we load all
    // the series for recurring event, to avoid orphaned exceptions in the
    // calendar or recurring events without their exceptions.
    const Incidence::List incs = incidences(notebookUid, uid);
    if (incs.isEmpty()) {
        return true;
    } else {
        QHash<QString, Incidence::List> hash;
        hash.insert(notebookUid, incs);
        return addIncidences(hash);
    }
}

void MultiCalendarStorage::Observer::storageModified(MultiCalendarStorage *storage)
{
    Q_UNUSED(storage);
}

void MultiCalendarStorage::Observer::storageUpdated(MultiCalendarStorage *storage,
                                                    const QString &notebookUid,
                                                    const Incidence::List &added,
                                                    const Incidence::List &modified,
                                                    const Incidence::List &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(notebookUid);
    Q_UNUSED(added);
    Q_UNUSED(modified);
    Q_UNUSED(deleted);
}

void MultiCalendarStorage::registerObserver(MultiCalendarStorage::Observer *observer)
{
    if (!d->mObservers.contains(observer)) {
        d->mObservers.append(observer);
    }
}

void MultiCalendarStorage::unregisterObserver(MultiCalendarStorage::Observer *observer)
{
    d->mObservers.removeAll(observer);
}

void MultiCalendarStorage::emitStorageModified()
{
    QStringList notebookUids;
    for (const Notebook::Ptr &notebook : loadedNotebooks()) {
        notebookUids.append(notebook->uid());
    }
    QHash<QString, CalendarHandler>::Iterator it = d->mCalendars.begin();
    while (it != d->mCalendars.end()) {
        if (!notebookUids.contains(it.key())) {
            it = d->mCalendars.erase(it);
        } else {
            it.value().calendar()->close();
        }
    }
    MultiCalendarStorage::open();

    foreach (Observer *observer, d->mObservers) {
        observer->storageModified(this);
    }
}

void MultiCalendarStorage::emitStorageUpdated(const QHash<QString, QStringList> &added,
                                              const QHash<QString, QStringList> &modified,
                                              const QHash<QString, QStringList> &deleted)
{
    QSet<QPair<QString, QString>> uids;

    for (const CalendarHandler &oneCalendar : d->mCalendars) {
        const QString notebookUid = oneCalendar.calendar()->id();
        const Incidence::List additions
            = oneCalendar.insertedIncidences(added.value(notebookUid));
        const Incidence::List modifications
            = oneCalendar.updatedIncidences(modified.value(notebookUid));
        const Incidence::List deletions
            = oneCalendar.deletedIncidences(deleted.value(notebookUid));
        if (!additions.isEmpty() || !modifications.isEmpty() || !deletions.isEmpty()) {
            foreach (Observer *observer, d->mObservers) {
                observer->storageUpdated(this, notebookUid,
                                         additions, modifications, deletions);
            }
        }

        for (const Incidence::Ptr &incidence : additions + modifications + deletions) {
            uids.insert(QPair<QString, QString>(notebookUid, incidence->uid()));
        }
    }

    d->setupAlarms(uids);
}

MemoryCalendar::Ptr MultiCalendarStorage::calendar(const QString &notebookUid) const
{
    QHash<QString, CalendarHandler>::ConstIterator it = d->mCalendars.find(notebookUid);
    return it != d->mCalendars.constEnd() ? it->calendar() : MemoryCalendar::Ptr();
}

Incidence::Ptr MultiCalendarStorage::instance(const QString &multiCalendarIdentifier) const
{
    QPair<QString, QString> ids = deserialiseIdentifier(multiCalendarIdentifier);
    const QString notebookUid = ids.first;
    const QString instanceId = ids.second;
    if (notebookUid.isEmpty() || instanceId.isEmpty()) {
        qCWarning(lcMkcal) << "invalid instance identifier" << multiCalendarIdentifier;
        return Incidence::Ptr();
    }
    QHash<QString, CalendarHandler>::ConstIterator it = d->mCalendars.find(notebookUid);
    return it != d->mCalendars.constEnd() ? it->calendar()->instance(instanceId) : Incidence::Ptr();
}

MemoryCalendar::Ptr MultiCalendarStorage::calendarOfInstance(const QString &multiCalendarIdentifier) const
{
    QPair<QString, QString> ids = deserialiseIdentifier(multiCalendarIdentifier);
    const QString notebookUid = ids.first;
    if (notebookUid.isEmpty()) {
        qCWarning(lcMkcal) << "invalid instance identifier" << multiCalendarIdentifier;
        return MemoryCalendar::Ptr();
    }
    return calendar(notebookUid);
}

Notebook::Ptr MultiCalendarStorage::notebookOfInstance(const QString &multiCalendarIdentifier) const
{
    QPair<QString, QString> ids = deserialiseIdentifier(multiCalendarIdentifier);
    const QString notebookUid = ids.first;
    if (notebookUid.isEmpty()) {
        qCWarning(lcMkcal) << "invalid instance identifier" << multiCalendarIdentifier;
        return Notebook::Ptr();
    }
    return notebook(notebookUid);
}

Notebook::Ptr MultiCalendarStorage::notebook(const QString &notebookUid) const
{
    QHash<QString, CalendarHandler>::ConstIterator it = d->mCalendars.find(notebookUid);
    return it != d->mCalendars.constEnd() ? it->notebook() : Notebook::Ptr();
}

Notebook::List MultiCalendarStorage::notebooks() const
{
    Notebook::List list;
    for (const CalendarHandler &oneCalendar : d->mCalendars) {
        list.append(oneCalendar.notebook());
    }
    return list;
}

Notebook::Ptr MultiCalendarStorage::addNotebook()
{
    Notebook::Ptr notebook(new Notebook);
    if (d->mCalendars.contains(notebook->uid())) {
        qCWarning(lcMkcal) << "cannot add notebook, uid already exists";
        return Notebook::Ptr();
    }

    QHash<QString, CalendarHandler>::Iterator it = 
        d->mCalendars.insert(notebook->uid(), CalendarHandler(d->mTimezone));
    it->setNotebook(notebook);

    return notebook;
}

void MultiCalendarStorage::emitNotebookUpdated(const Notebook &old)
{
    QHash<QString, CalendarHandler>::Iterator it = 
        d->mCalendars.find(old.uid());
    it->setNotebook(it->notebook());

    if (old.isVisible() && !it->notebook()->isVisible()) {
        d->clearAlarms(it->calendar()->id());
    } else if (!old.isVisible() && it->notebook()->isVisible()) {
        d->setupAlarms(it->calendar()->id());
    }
}

bool MultiCalendarStorage::save(const QString &notebookUid, DeleteAction deleteAction)
{
    QHash<QString, Incidence::List> toAdd, toUpdate, toDelete;

    QHash<QString, CalendarHandler>::Iterator it = d->mCalendars.end();
    if (notebookUid.isEmpty()) {
        for (const CalendarHandler &oneCalendar : d->mCalendars) {
            Incidence::List additions, modifications, deletions;
            oneCalendar.observedIncidences(&additions, &modifications, &deletions);
            toAdd.insert(oneCalendar.calendar()->id(), additions);
            toUpdate.insert(oneCalendar.calendar()->id(), modifications);
            toDelete.insert(oneCalendar.calendar()->id(), deletions);
        }
    } else {
        it = d->mCalendars.find(notebookUid);
        if (it == d->mCalendars.end()) {
            qCWarning(lcMkcal) << "not a known notebook" << notebookUid;
            return false;
        }
        Incidence::List additions, modifications, deletions;
        it->observedIncidences(&additions, &modifications, &deletions);
        toAdd.insert(it->calendar()->id(), additions);
        toUpdate.insert(it->calendar()->id(), modifications);
        toDelete.insert(it->calendar()->id(), deletions);
    }

    bool success = save(notebookUid, toAdd, toUpdate, toDelete, deleteAction);

    if (it == d->mCalendars.end()) {
        for (CalendarHandler &oneCalendar : d->mCalendars) {
            oneCalendar.clearObservedIncidences();
        }
    } else {
        it->clearObservedIncidences();
    }

    return success;
}

bool MultiCalendarStorage::deleteNotebook(const QString &notebookUid)
{
    QHash<QString, CalendarHandler>::Iterator it = d->mCalendars.find(notebookUid);
    if (it == d->mCalendars.end()) {
        qCWarning(lcMkcal) << "not a known notebook" << notebookUid;
        return false;
    }

    if (!it->notebook()->isRunTimeOnly()) {
        d->clearAlarms(it->calendar()->id());
    }

    d->mCalendars.erase(it);

    return true;
}

bool MultiCalendarStorage::setDefaultNotebook(const QString &notebookUid)
{
    if (notebookUid == d->mDefaultNotebookUid) {
        return true;
    }
    if (!d->mCalendars.contains(notebookUid)) {
        qCWarning(lcMkcal) << "cannot set default notebook" << notebookUid;
        return false;
    }
    d->mDefaultNotebookUid = notebookUid;

    return true;
}

Notebook::Ptr MultiCalendarStorage::defaultNotebook()
{
    QHash<QString, CalendarHandler>::ConstIterator it = d->mCalendars.find(d->mDefaultNotebookUid);
    return it == d->mCalendars.constEnd() ? Notebook::Ptr() : it->notebook();
}

Incidence::List MultiCalendarStorage::Private::incidencesWithAlarms(const QString &notebookUid, const QString &uid)
{
    Incidence::List list;

    QHash<QString, CalendarHandler>::ConstIterator it = mCalendars.find(notebookUid);
    if (it == mCalendars.constEnd() || !it->notebook()->isVisible()) {
        return list;
    }

    // Recurring incidences may not have alarms but their exception may.
    for (const Incidence::Ptr &incidence : mStorage->incidences(notebookUid, uid)) {
        if (incidence->hasEnabledAlarms() || incidence->recurs()) {
            list.append(incidence);
        }
    }
    return list;
}

bool MultiCalendarStorage::addIncidences(const QHash<QString, Incidence::List> &list)
{
    bool success = true;

    for (QHash<QString, Incidence::List>::ConstIterator it = list.constBegin();
         it != list.constEnd(); it++) {
        QHash<QString, CalendarHandler>::Iterator oneCalendar = d->mCalendars.find(it.key());
        if (oneCalendar != d->mCalendars.end()) {
            success = oneCalendar->addIncidences(it.value()) && success;
        }
    }

    return success;
}

bool MultiCalendarStorage::getLoadDates(const QDate &start, const QDate &end,
                                        QDateTime *loadStart, QDateTime *loadEnd) const
{
    loadStart->setDate(start);   // may be null if start is not valid
    loadEnd->setDate(end);   // may be null if end is not valid

    // Check the need to load from db.
    for (const Range &loadedRange : d->mRanges) {
        bool startIsIn = loadedRange.contains(loadStart->date())
            || (loadedRange.mStart.isNull() && loadStart->date().isNull());
        bool endIsIn = loadedRange.contains(loadEnd->date().addDays(-1))
            || (loadedRange.mEnd.isNull() && loadEnd->date().isNull());
        if (startIsIn && endIsIn) {
            return false;
        } else if (startIsIn) {
            loadStart->setDate(loadedRange.mEnd.addDays(1));
        } else if (endIsIn) {
            loadEnd->setDate(loadedRange.mStart);
        }
    }
    if (loadStart->isValid() && loadEnd->isValid() && *loadStart >= *loadEnd) {
        return false;
    }

    if (loadStart->isValid()) {
        loadStart->setTimeZone(d->mTimezone);
    }
    if (loadEnd->isValid()) {
        loadEnd->setTimeZone(d->mTimezone);
    }

    qCDebug(lcMkcal) << "get load dates" << start << end << *loadStart << *loadEnd;

    return true;
}

void MultiCalendarStorage::addLoadedRange(const QDate &start, const QDate &end) const
{
    qCDebug(lcMkcal) << "set load dates" << start << end;

    Range range(start, end.addDays(-1));
    QList<Range>::Iterator it = d->mRanges.begin();
    while (it != d->mRanges.end()) {
        if (range < *it) {
            d->mRanges.insert(it, range);
            return;
        } else if (it->contains(end)) {
            if (start < *it) {
                it->mStart = start;
            }
            return;
        } else if (start < *it) {
            it = d->mRanges.erase(it);
        } else if (it->contains(start)) {
            range.mStart = it->mStart;
            it = d->mRanges.erase(it);
        } else {
            it++;
        }
    }
    d->mRanges.append(range);
}

bool MultiCalendarStorage::isRecurrenceLoaded() const
{
    return d->mIsRecurrenceLoaded;
}

void MultiCalendarStorage::setIsRecurrenceLoaded(bool loaded)
{
    d->mIsRecurrenceLoaded = loaded;
}

MultiCalendarStorage::Ptr MultiCalendarStorage::systemStorage(const QTimeZone &timezone)
{
    return SqliteMultiCalendarStorage::Ptr(new SqliteMultiCalendarStorage(timezone)).staticCast<MultiCalendarStorage>();
}
