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
#include "extendedstorage.h"
#include "extendedstorageobserver.h"
#include "logging_p.h"

#include <KCalendarCore/Exceptions>
#include <KCalendarCore/Calendar>
using namespace KCalendarCore;

#ifdef TIMED_SUPPORT
# include <timed-qt5/interface.h>
# include <timed-qt5/event-declarations.h>
# include <timed-qt5/exception.h>
# include <QtCore/QMap>
# include <QtDBus/QDBusReply>
using namespace Maemo;
static const QLatin1String RESET_ALARMS_CMD("invoker --type=generic -n /usr/bin/mkcaltool --reset-alarms");
#endif

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
bool operator<(const Range &a, const Range &b)
{
    return a.mEnd.isValid() && b.mStart.isValid() && a.mEnd < b.mStart;
}
// Date a is strictly before range b.
bool operator<(const QDate &at, const Range &range)
{
    return at.isNull() || (range.mStart.isValid() && at < range.mStart);
}

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::ExtendedStorage::Private
{
public:
    Private(bool validateNotebooks)
        : mValidateNotebooks(validateNotebooks),
          mIsRecurrenceLoaded(false),
          mDefaultNotebook(0)
    {}

    bool mValidateNotebooks;
    QList<Range> mRanges;
    bool mIsRecurrenceLoaded;
    QList<ExtendedStorageObserver *> mObservers;
    QHash<QString, Notebook::Ptr> mNotebooks; // uid to notebook
    Notebook::Ptr mDefaultNotebook;

    bool clear();

#if defined(TIMED_SUPPORT)
    // These alarm methods are used to communicate with an external
    // daemon, like timed, to bind Incidence::Alarm with the system notification.
    void clearAlarms(const Incidence::Ptr &incidence);
    void clearAlarmsByNotebook(const QString &notebookUid);
    QDateTime getNextOccurrence(const Incidence::Ptr &incidence,
                                const QDateTime &start,
                                const Incidence::List &exceptions);
    void setAlarms(const Incidence::List &incidences, const Calendar::Ptr &calendar);

    void addAlarms(const Incidence::Ptr &incidence, const QString &nbuid, Timed::Event::List *events, const QDateTime &now);
    void commitEvents(Timed::Event::List &events);
#endif
};

bool ExtendedStorage::Private::clear()
{
    mRanges.clear();
    mIsRecurrenceLoaded = false;
    mNotebooks.clear();
    mDefaultNotebook = Notebook::Ptr();

    return true;
}
//@endcond

ExtendedStorage::ExtendedStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks)
    : CalStorage(cal),
      d(new ExtendedStorage::Private(validateNotebooks))
{
    cal->registerObserver(this);
}

ExtendedStorage::~ExtendedStorage()
{
    calendar()->unregisterObserver(this);
    delete d;
}

bool ExtendedStorage::close()
{
    return d->clear();
}

bool ExtendedStorage::getLoadDates(const QDate &start, const QDate &end,
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
        loadStart->setTimeZone(calendar()->timeZone());
    }
    if (loadEnd->isValid()) {
        loadEnd->setTimeZone(calendar()->timeZone());
    }

    qCDebug(lcMkcal) << "get load dates" << start << end << *loadStart << *loadEnd;

    return true;
}

void ExtendedStorage::addLoadedRange(const QDate &start, const QDate &end) const
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

bool ExtendedStorage::isRecurrenceLoaded() const
{
    return d->mIsRecurrenceLoaded;
}

void ExtendedStorage::setIsRecurrenceLoaded(bool loaded)
{
    d->mIsRecurrenceLoaded = loaded;
}

bool ExtendedStorage::loadSeries(const QString &uid)
{
    qCWarning(lcMkcal) << "deprecated call to loadSeries(), use load() instead.";
    return load(uid);
}

bool ExtendedStorage::load(const QString &uid, const QDateTime &recurrenceId)
{
    Q_UNUSED(recurrenceId);

    qCWarning(lcMkcal) << "deprecated call to load(uid, recid), use load(uid) instead.";
    return load(uid);
}

bool ExtendedStorage::loadIncidenceInstance(const QString &instanceIdentifier)
{
    QString uid;
    // At the moment, from KCalendarCore, if the instance is an exception,
    // the instanceIdentifier will ends with yyyy-MM-ddTHH:mm:ss[Z|[+|-]HH:mm]
    // This is tested in tst_loadIncidenceInstance() to ensure that any
    // future breakage would be properly detected.
    if (instanceIdentifier.endsWith('Z')) {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 20);
    } else if (instanceIdentifier.length() > 19
               && instanceIdentifier[instanceIdentifier.length() - 9] == 'T') {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 19);
    } else if (instanceIdentifier.length() > 25
               && instanceIdentifier[instanceIdentifier.length() - 3] == ':') {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 25);
    } else {
        uid = instanceIdentifier;
    }

    // Even if we're looking for a specific incidence instance, we load all
    // the series for recurring event, to avoid orphaned exceptions in the
    // calendar or recurring events without their exceptions.
    return load(uid);
}

bool ExtendedStorage::load(const QDate &date)
{
    return date.isValid() && load(date, date.addDays(1));
}

void ExtendedStorageObserver::storageModified(ExtendedStorage *storage,
                                              const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);
}

void ExtendedStorageObserver::storageFinished(ExtendedStorage *storage,
                                              bool error, const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(error);
    Q_UNUSED(info);
}

void ExtendedStorageObserver::storageUpdated(ExtendedStorage *storage,
                                             const KCalendarCore::Incidence::List &added,
                                             const KCalendarCore::Incidence::List &modified,
                                             const KCalendarCore::Incidence::List &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(added);
    Q_UNUSED(modified);
    Q_UNUSED(deleted);
}

void ExtendedStorage::registerObserver(ExtendedStorageObserver *observer)
{
    if (!d->mObservers.contains(observer)) {
        d->mObservers.append(observer);
    }
}

void ExtendedStorage::unregisterObserver(ExtendedStorageObserver *observer)
{
    d->mObservers.removeAll(observer);
}

void ExtendedStorage::setModified(const QString &info)
{
    const QStringList list = d->mNotebooks.keys();
    for (const QString &uid : list) {
        if (!calendar()->deleteNotebook(uid)) {
            qCDebug(lcMkcal) << "notebook" << uid << "already removed from calendar";
        }
    }
    calendar()->close();
    d->clear();
    if (!loadNotebooks()) {
        qCWarning(lcMkcal) << "loading notebooks failed";
    }

    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageModified(this, info);
    }
}

void ExtendedStorage::setFinished(bool error, const QString &info)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageFinished(this, error, info);
    }
}

void ExtendedStorage::setUpdated(const KCalendarCore::Incidence::List &added,
                                 const KCalendarCore::Incidence::List &modified,
                                 const KCalendarCore::Incidence::List &deleted)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageUpdated(this, added, modified, deleted);
    }
#if defined(TIMED_SUPPORT)
    if (!added.isEmpty())
        d->setAlarms(added, calendar());
    // Store the recurring event uids, to avoid treating them twice in modified
    // and deleted cases.
    QSet<QString> recurringUids;
    for (const Incidence::Ptr incidence : modified + deleted) {
        d->clearAlarms(incidence);
        if (incidence->recurs()) {
            recurringUids.insert(incidence->uid());
        }
    }
    if (!modified.isEmpty()) {
        d->setAlarms(modified, calendar());
    }
    if (!deleted.isEmpty()) {
        Incidence::List recurring; // List of recurring events to be updated because of exception deletion.
        for (const Incidence::Ptr incidence : deleted) {
            // If deleting an exception but not the recurring parent,
            // the alarm of the parent need to be recomputed.
            if (incidence->hasRecurrenceId() && !recurringUids.contains(incidence->uid())) {
                Incidence::Ptr parent = calendar()->incidence(incidence->uid());
                if (parent) {
                    d->clearAlarms(parent);
                    recurring << parent;
                    recurringUids.insert(parent->uid());
                }
            }
        }
        d->setAlarms(recurring, calendar());
    }
#endif
}

bool ExtendedStorage::addNotebook(const Notebook::Ptr &nb)
{
    if (!nb || d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!nb->isRunTimeOnly() && !modifyNotebook(nb, DBInsert)) {
        return false;
    }

    d->mNotebooks.insert(nb->uid(), nb);
    if (!calendar()->addNotebook(nb->uid(), nb->isVisible())
        && !calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "notebook" << nb->uid() << "already in calendar";
    }

    return true;
}

bool ExtendedStorage::updateNotebook(const Notebook::Ptr &nb)
{
    if (!nb || !d->mNotebooks.contains(nb->uid()) ||
            d->mNotebooks.value(nb->uid()) != nb) {
        return false;
    }

    if (!nb->isRunTimeOnly() && !modifyNotebook(nb, DBUpdate)) {
        return false;
    }

    bool wasVisible = calendar()->isVisible(nb->uid());
    if (!calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "cannot update notebook" << nb->uid() << "in calendar";
        return false;
    }

#if defined(TIMED_SUPPORT)
    if (!nb->isRunTimeOnly()) {
        if (wasVisible && !nb->isVisible()) {
            d->clearAlarmsByNotebook(nb->uid());
        } else if (!wasVisible && nb->isVisible()) {
            Incidence::List list;
            if (allIncidences(&list, nb->uid())) {
                MemoryCalendar::Ptr calendar(new MemoryCalendar(QTimeZone::utc()));
                if (calendar->addNotebook(nb->uid(), true)) {
                    for (const Incidence::Ptr &incidence : const_cast<const Incidence::List&>(list)) {
                        calendar->addIncidence(incidence);
                        calendar->setNotebook(incidence, nb->uid());
                    }
                }
                d->setAlarms(calendar->incidences(), calendar);
            }
        }
    }
#endif

    return true;
}

bool ExtendedStorage::deleteNotebook(const Notebook::Ptr &nb)
{
    if (!nb || !d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!nb->isRunTimeOnly() && !modifyNotebook(nb, DBDelete)) {
        return false;
    }

    // purge all notebook incidences from storage
    if (!nb->isRunTimeOnly()) {
        Incidence::List deleted;
        deletedIncidences(&deleted, QDateTime(), nb->uid());
        qCDebug(lcMkcal) << "purging" << deleted.count() << "incidences of notebook" << nb->name();
        if (!deleted.isEmpty() && !purgeDeletedIncidences(deleted)) {
            qCWarning(lcMkcal) << "error when purging deleted incidences from notebook" << nb->uid();
        }
        if (loadNotebookIncidences(nb->uid())) {
            const Incidence::List list = calendar()->incidences(nb->uid());
            qCDebug(lcMkcal) << "deleting" << list.size() << "incidences of notebook" << nb->name();
            for (const Incidence::Ptr &toDelete : list) {
                // Need to test the existence of toDelete inside the calendar here,
                // because KCalendarCore::Calendar::incidences(nbuid) is returning
                // all incidences associated to nbuid, even those that have been
                // deleted already.
                // In addition, Calendar::deleteIncidence() is also deleting all exceptions
                // of a recurring event, so exceptions may have been already removed and
                // their existence should be checked to avoid warnings.
                if (calendar()->incidence(toDelete->uid(), toDelete->recurrenceId()))
                    calendar()->deleteIncidence(toDelete);
            }
            if (!list.isEmpty()) {
                save(ExtendedStorage::PurgeDeleted);
            }
        } else {
            qCWarning(lcMkcal) << "error when loading incidences for notebook" << nb->uid();
            return false;
        }
    }

    if (!calendar()->deleteNotebook(nb->uid())) {
        qCWarning(lcMkcal) << "notebook" << nb->uid() << "already deleted from calendar";
    }

    d->mNotebooks.remove(nb->uid());

    if (d->mDefaultNotebook == nb) {
        d->mDefaultNotebook = Notebook::Ptr();
    }

    return true;
}

bool ExtendedStorage::setDefaultNotebook(const Notebook::Ptr &nb)
{
    d->mDefaultNotebook = nb;

    if (!nb
        || (d->mNotebooks.contains(nb->uid()) && !updateNotebook(nb))
        || (!d->mNotebooks.contains(nb->uid()) && !addNotebook(nb))) {
        return false;
    }

    if (!calendar()->setDefaultNotebook(nb->uid())) {
        qCWarning(lcMkcal) << "cannot set notebook" << nb->uid() << "as default in calendar";
    }

    return true;
}

Notebook::Ptr ExtendedStorage::defaultNotebook()
{
    return d->mDefaultNotebook;
}

Notebook::List ExtendedStorage::notebooks()
{
    return d->mNotebooks.values();
}

Notebook::Ptr ExtendedStorage::notebook(const QString &uid) const
{
    return d->mNotebooks.value(uid);
}

void ExtendedStorage::setValidateNotebooks(bool validateNotebooks)
{
    d->mValidateNotebooks = validateNotebooks;
}

bool ExtendedStorage::validateNotebooks() const
{
    return d->mValidateNotebooks;
}

bool ExtendedStorage::isValidNotebook(const QString &notebookUid) const
{
    const Notebook::Ptr nb = notebook(notebookUid);
    if (!nb.isNull()) {
        if (nb->isRunTimeOnly() || nb->isReadOnly()) {
            qCDebug(lcMkcal) << "notebook" << notebookUid << "isRunTimeOnly or isReadOnly";
            return false;
        }
    } else if (validateNotebooks()) {
        qCDebug(lcMkcal) << "notebook" << notebookUid << "is not valid for this storage";
        return false;
    } else if (calendar()->hasValidNotebook(notebookUid)) {
        qCDebug(lcMkcal) << "notebook" << notebookUid << "is saved by another storage";
        return false;
    }
    return true;
}

Notebook::Ptr ExtendedStorage::createDefaultNotebook(QString name, QString color)
{
    qCWarning(lcMkcal) << "Deprecated call to createDefaultNotebook(),"
                       << "create a notebook and make it default with setDefaultNotebook() instead";
    if (name.isEmpty())
        name = "Default";
    if (color.isEmpty())
        color = "#0000FF";
    Notebook::Ptr nbDefault(new Notebook(name, QString(), color));
    return setDefaultNotebook(nbDefault) ? nbDefault : Notebook::Ptr();
}

#if defined(TIMED_SUPPORT)
// Todo: move this into a service plugin that is a ExtendedStorageObserver.
QDateTime ExtendedStorage::Private::getNextOccurrence(const Incidence::Ptr &incidence,
                                                      const QDateTime &start,
                                                      const Incidence::List &exceptions)
{
    if (!start.isNull() && incidence->recurs()) {
        Recurrence *recurrence = incidence->recurrence();
        QSet<QDateTime> recurrenceIds;
        for (const Incidence::Ptr &exception : exceptions)
            recurrenceIds.insert(exception->recurrenceId());

        QDateTime match = start;
        if (!recurrence->recursAt(start) || recurrenceIds.contains(start)) {
            do {
                match = recurrence->getNextDateTime(match);
            } while (match.isValid() && recurrenceIds.contains(match));
        }
        return match;
    } else {
        return incidence->dtStart();
    }
}

void ExtendedStorage::Private::setAlarms(const Incidence::List &incidences,
                                         const Calendar::Ptr &calendar)
{
    QSet<QString> recurringUids;
    for (const Incidence::Ptr &incidence : incidences) {
        if (incidence->recurs()) {
            recurringUids.insert(incidence->uid());
        }
    }
    const QDateTime now = QDateTime::currentDateTime();
    Timed::Event::List events;
    for (const Incidence::Ptr &incidence : incidences) {
        // The incidence from the list must be in the calendar and in a notebook.
        const QString &nbuid = calendar->notebook(incidence->uid());
        if (!calendar->isVisible(incidence) || nbuid.isEmpty()) {
            continue;
        }
        if (incidence->recurs()) {
            const QDateTime next = getNextOccurrence(incidence, now, calendar->instances(incidence));
            addAlarms(incidence, nbuid, &events, next);
        } else if (incidence->hasRecurrenceId()) {
            const Incidence::Ptr parent = calendar->incidence(incidence->uid());
            if (parent && !recurringUids.contains(parent->uid())) {
                clearAlarms(parent);
                const QDateTime next = getNextOccurrence(parent, now, calendar->instances(parent));
                addAlarms(parent, nbuid, &events, next);
                recurringUids.insert(parent->uid());
            }
            addAlarms(incidence, nbuid, &events, now);
        } else {
            addAlarms(incidence, nbuid, &events, now);
        }
    }
    commitEvents(events);
}

void ExtendedStorage::Private::clearAlarms(const Incidence::Ptr &incidence)
{
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["uid"] = incidence->uid();

    Timed::Interface timed;
    if (!timed.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << incidence->uid()
                           << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-")
                           << "alarm interface is not valid" << timed.lastError();
        return;
    }
    QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    if (!reply.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << incidence->uid()
                           << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-")
                           << timed.lastError();
        return;
    }

    const QList<QVariant> &result = reply.value();
    for (int i = 0; i < result.size(); i++) {
        uint32_t cookie = result[i].toUInt();
        // We got a list of all alarm matching UID of this incidence
        // - single event -> delete the alarm
        // - recurring parent event -> the recurs() case, delete if
        //   recurrenceId attribute is empty (thus invalid QDateTime)
        // - recurring exception event -> the hasRecurrenceId() case,
        //   delete if the recurrenceId attribute is matching in terms of QDateTime.
        if (incidence->recurs() || incidence->hasRecurrenceId()) {
            QDBusReply<QMap<QString, QVariant> > attributesReply = timed.query_attributes_sync(cookie);
            const QMap<QString, QVariant> attributeMap = attributesReply.value();
            const QVariant recurrenceId = attributeMap.value("recurrenceId", QVariant(QString()));
            QDateTime recid = QDateTime::fromString(recurrenceId.toString(), Qt::ISODate);
            if (incidence->recurrenceId() != recid) {
                continue;
            }
        }
        qCDebug(lcMkcal) << "removing alarm" << cookie << incidence->uid()
                         << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-");
        QDBusReply<bool> reply = timed.cancel_sync(cookie);
        if (!reply.isValid() || !reply.value()) {
            qCWarning(lcMkcal) << "cannot remove alarm" << cookie << incidence->uid()
                               << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-")
                               << reply.value() << timed.lastError();
        }
    }
}

void ExtendedStorage::Private::clearAlarmsByNotebook(const QString &notebookUid)
{
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["notebook"] = notebookUid;

    Timed::Interface timed;
    if (!timed.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << notebookUid
                 << "alarm interface is not valid" << timed.lastError();
        return;
    }
    QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    if (!reply.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << notebookUid << timed.lastError();
        return;
    }
    const QList<QVariant> &result = reply.value();
    for (int i = 0; i < result.size(); i++) {
        uint32_t cookie = result[i].toUInt();
        qCDebug(lcMkcal) << "removing alarm" << cookie << notebookUid;
        QDBusReply<bool> reply = timed.cancel_sync(cookie);
        if (!reply.isValid() || !reply.value()) {
            qCWarning(lcMkcal) << "cannot remove alarm" << cookie << notebookUid;
        }
    }
}

void ExtendedStorage::Private::addAlarms(const Incidence::Ptr &incidence,
                                         const QString &nbuid,
                                         Timed::Event::List *events,
                                         const QDateTime &laterThan)
{
    if (incidence->status() == Incidence::StatusCanceled || laterThan.isNull()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const Alarm::List alarms = incidence->alarms();
    foreach (const Alarm::Ptr alarm, alarms) {
        if (!alarm->enabled()) {
            continue;
        }

        QDateTime preTime = laterThan;
        if (incidence->recurs()) {
            QDateTime nextRecurrence = incidence->recurrence()->getNextDateTime(laterThan);
            if (nextRecurrence.isValid() && alarm->startOffset().asSeconds() < 0) {
                if (laterThan.addSecs(::abs(alarm->startOffset().asSeconds())) >= nextRecurrence) {
                    preTime = nextRecurrence;
                }
            }
        }

        // nextTime() is returning time strictly later than its argument.
        QDateTime alarmTime = alarm->nextTime(preTime.addSecs(-1), true);
        if (!alarmTime.isValid()) {
            continue;
        }

        if (now.addSecs(60) > alarmTime) {
            // don't allow alarms within the current minute -> take next alarm if so
            alarmTime = alarm->nextTime(preTime.addSecs(60), true);
            if (!alarmTime.isValid()) {
                continue;
            }
        }
        Timed::Event &e = events->append();
        e.setUserModeFlag();
        e.setMaximalTimeoutSnoozeCounter(2);
        e.setTicker(alarmTime.toUTC().toTime_t());
        // The code'll crash (=exception) iff the content is empty. So
        // we have to check here.
        QString s;

        s = incidence->summary();
        // Timed braindeath: Required field, BUT if empty, it asserts
        if (s.isEmpty()) {
            s = ' ';
        }
        e.setAttribute("TITLE", s);
        e.setAttribute("PLUGIN", "libCalendarReminder");
        e.setAttribute("APPLICATION", "libextendedkcal");
        //e.setAttribute( "translation", "organiser" );
        // This really has to exist or code is badly broken
        Q_ASSERT(!incidence->uid().isEmpty());
        e.setAttribute("uid", incidence->uid());
#ifndef QT_NO_DEBUG_OUTPUT //Helps debuggin
        e.setAttribute("alarmtime", alarmTime.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
#endif
        if (!incidence->location().isEmpty()) {
            e.setAttribute("location", incidence->location());
        }
        if (incidence->recurs()) {
            e.setAttribute("recurs", "true");
            Timed::Event::Action &a = e.addAction();
            a.runCommand(QString("%1 %2 %3")
                         .arg(RESET_ALARMS_CMD)
                         .arg(nbuid)
                         .arg(incidence->uid()));
            a.whenServed();
        }

        // TODO - consider this how it should behave for recurrence
        if ((incidence->type() == Incidence::TypeTodo)) {
            Todo::Ptr todo = incidence.staticCast<Todo>();

            if (todo->hasDueDate()) {
                e.setAttribute("time", todo->dtDue(true).toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            }
            e.setAttribute("type", "todo");
        } else if (incidence->dtStart().isValid()) {
            QDateTime eventStart;

            if (incidence->recurs()) {
                // assuming alarms not later than event start
                eventStart = incidence->recurrence()->getNextDateTime(alarmTime.addSecs(-60));
            } else {
                eventStart = incidence->dtStart();
            }
            e.setAttribute("time", eventStart.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            e.setAttribute("startDate", eventStart.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            if (incidence->endDateForStart(eventStart).isValid()) {
                e.setAttribute("endDate", incidence->endDateForStart(eventStart).toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            }
            e.setAttribute("type", "event");
        }

        if (incidence->hasRecurrenceId()) {
            e.setAttribute("recurrenceId", incidence->recurrenceId().toString(Qt::ISODate));
        }
        e.setAttribute("notebook", nbuid);

        if (alarm->type() == Alarm::Procedure) {
            QString prog = alarm->programFile();
            if (!prog.isEmpty()) {
                Timed::Event::Action &a = e.addAction();
                a.runCommand(prog + " " + alarm->programArguments());
                a.whenFinalized();
            }
        } else {
            e.setReminderFlag();
            e.setAlignedSnoozeFlag();
        }
    }
}

void ExtendedStorage::Private::commitEvents(Timed::Event::List &events)
{
    if (events.count() > 0) {
        Timed::Interface timed;
        if (!timed.isValid()) {
            qCWarning(lcMkcal) << "cannot set alarm for incidence: "
                               << "alarm interface is not valid" << timed.lastError();
            return;
        }
        QDBusReply < QList<QVariant> > reply = timed.add_events_sync(events);
        if (reply.isValid()) {
            foreach (QVariant v, reply.value()) {
                bool ok = true;
                uint cookie = v.toUInt(&ok);
                if (ok && cookie) {
                    qCDebug(lcMkcal) << "added alarm: " << cookie;
                } else {
                    qCWarning(lcMkcal) << "failed to add alarm";
                }
            }
        } else {
            qCWarning(lcMkcal) << "failed to add alarms: " << reply.error().message();
        }
    } else {
        qCDebug(lcMkcal) << "No alarms to send";
    }
}
#endif
