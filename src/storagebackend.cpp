/*
  Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>.

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

#include "storagebackend.h"
#include "logging_p.h"

#include <QtCore/QUuid>

using namespace KCalendarCore;

#ifdef TIMED_SUPPORT
# include <timed-qt5/interface.h>
# include <timed-qt5/event-declarations.h>
# include <timed-qt5/exception.h>
# include <QtCore/QMap>
# include <QtDBus/QDBusReply>
# include <KCalendarCore/Todo>
using namespace Maemo;
static const QLatin1String RESET_ALARMS_CMD("invoker --type=generic -n /usr/bin/mkcaltool --reset-alarms");
#endif

using namespace mKCal;

namespace {
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
}

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::StorageBackend::Private
{
public:
    Private(const QTimeZone &timeZone)
        : mTimeZone(timeZone)
    {}

    QTimeZone mTimeZone;
    QList<Range> mRanges;
    bool mIsRecurrenceLoaded = false;
    bool mIsUncompletedTodosLoaded = false;
    bool mIsCompletedTodosDateLoaded = false;
    bool mIsCompletedTodosCreatedLoaded = false;
    bool mIsDateLoaded = false;
    bool mIsCreatedLoaded = false;
    bool mIsFutureDateLoaded = false;
    bool mIsGeoDateLoaded = false;
    bool mIsGeoCreatedLoaded = false;
    bool mIsJournalsLoaded = false;
    QList<StorageBackend::Observer *> mObservers;

#if defined(TIMED_SUPPORT)
    QHash<QString, bool> mNotebookVisibility;
    // These alarm methods are used to communicate with an external
    // daemon, like timed, to bind Incidence::Alarm with the system notification.
    void clearAlarms(const Incidence::Ptr &incidence);
    void clearAlarms(const StorageBackend::Collection &incidences);
    void clearAlarms(const QString &notebookUid);
    void setAlarms(const StorageBackend::Collection &incidences);
    void resetAlarms(const StorageBackend::Collection &incidences);

    void setAlarmsForNotebook(const Incidence::List &incidences, const QString &notebookUid);
    void setAlarms(const Incidence::Ptr &incidence, const QString &nbuid, Timed::Event::List &events, const QDateTime &now);
    void commitEvents(Timed::Event::List &events);
#endif
};
//@endcond

StorageBackend::StorageBackend(const QTimeZone &timeZone)
    : QObject(), d(new StorageBackend::Private(timeZone))
{
}

StorageBackend::~StorageBackend()
{
    delete d;
}

QTimeZone StorageBackend::timeZone() const
{
    return d->mTimeZone;
}

void StorageBackend::setTimeZone(const QTimeZone &timeZone)
{
    d->mTimeZone = timeZone;
}

void StorageBackend::clearLoaded()
{
    d->mRanges.clear();
    d->mIsRecurrenceLoaded = false;
    d->mIsUncompletedTodosLoaded = false;
    d->mIsCompletedTodosDateLoaded = false;
    d->mIsCompletedTodosCreatedLoaded = false;
    d->mIsDateLoaded = false;
    d->mIsCreatedLoaded = false;
    d->mIsFutureDateLoaded = false;
    d->mIsGeoDateLoaded = false;
    d->mIsGeoCreatedLoaded = false;
    d->mIsJournalsLoaded = false;
}

bool StorageBackend::getLoadDates(const QDate &start, const QDate &end,
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
        loadStart->setTimeZone(d->mTimeZone);
    }
    if (loadEnd->isValid()) {
        loadEnd->setTimeZone(d->mTimeZone);
    }

    qCDebug(lcMkcal) << "get load dates" << start << end << *loadStart << *loadEnd;

    return true;
}

void StorageBackend::addLoadedRange(const QDate &start, const QDate &end) const
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

bool StorageBackend::isRecurrenceLoaded() const
{
    return d->mIsRecurrenceLoaded;
}

void StorageBackend::setIsRecurrenceLoaded(bool loaded)
{
    d->mIsRecurrenceLoaded = loaded;
}

bool StorageBackend::isUncompletedTodosLoaded()
{
    return d->mIsUncompletedTodosLoaded;
}

void StorageBackend::setIsUncompletedTodosLoaded(bool loaded)
{
    d->mIsUncompletedTodosLoaded = loaded;
}

bool StorageBackend::isCompletedTodosDateLoaded()
{
    return d->mIsCompletedTodosDateLoaded;
}

void StorageBackend::setIsCompletedTodosDateLoaded(bool loaded)
{
    d->mIsCompletedTodosDateLoaded = loaded;
}

bool StorageBackend::isCompletedTodosCreatedLoaded()
{
    return d->mIsCompletedTodosCreatedLoaded;
}

void StorageBackend::setIsCompletedTodosCreatedLoaded(bool loaded)
{
    d->mIsCompletedTodosCreatedLoaded = loaded;
}

bool StorageBackend::isDateLoaded()
{
    return d->mIsDateLoaded;
}

void StorageBackend::setIsDateLoaded(bool loaded)
{
    d->mIsDateLoaded = loaded;
}

bool StorageBackend::isFutureDateLoaded()
{
    return d->mIsFutureDateLoaded;
}

void StorageBackend::setIsFutureDateLoaded(bool loaded)
{
    d->mIsFutureDateLoaded = loaded;
}

bool StorageBackend::isJournalsLoaded()
{
    return d->mIsJournalsLoaded;
}

void StorageBackend::setIsJournalsLoaded(bool loaded)
{
    d->mIsJournalsLoaded = loaded;
}

bool StorageBackend::isCreatedLoaded()
{
    return d->mIsCreatedLoaded;
}

void StorageBackend::setIsCreatedLoaded(bool loaded)
{
    d->mIsCreatedLoaded = loaded;
}

bool StorageBackend::isGeoDateLoaded()
{
    return d->mIsGeoDateLoaded;
}

void StorageBackend::setIsGeoDateLoaded(bool loaded)
{
    d->mIsGeoDateLoaded = loaded;
}

bool StorageBackend::isGeoCreatedLoaded()
{
    return d->mIsGeoCreatedLoaded;
}

void StorageBackend::setIsGeoCreatedLoaded(bool loaded)
{
    d->mIsGeoCreatedLoaded = loaded;
}

StorageBackend::Observer::~Observer()
{
}

void StorageBackend::Observer::storageOpened(StorageBackend *storage,
                                             const Notebook::List &notebooks,
                                             const Notebook::Ptr &defaultNotebook)
{
    Q_UNUSED(storage);
    Q_UNUSED(notebooks);
    Q_UNUSED(defaultNotebook);
}

void StorageBackend::Observer::storageClosed(StorageBackend *storage)
{
    Q_UNUSED(storage);
}

void StorageBackend::Observer::storageModified(StorageBackend *storage,
                                               const Notebook::List &notebooks,
                                               const Notebook::Ptr &defaultNotebook)
{
    Q_UNUSED(storage);
    Q_UNUSED(notebooks);
    Q_UNUSED(defaultNotebook);
}

void StorageBackend::Observer::storageUpdated(StorageBackend *storage,
                                              const StorageBackend::Collection &added,
                                              const StorageBackend::Collection &modified,
                                              const StorageBackend::Collection &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(added);
    Q_UNUSED(modified);
    Q_UNUSED(deleted);
}

void StorageBackend::Observer::incidenceLoaded(StorageBackend *storage,
                                               const StorageBackend::Collection &incidences)
{
    Q_UNUSED(storage);
    Q_UNUSED(incidences);
}

void StorageBackend::registerObserver(StorageBackend::Observer *observer)
{
    if (!d->mObservers.contains(observer)) {
        d->mObservers.append(observer);
    }
}

void StorageBackend::unregisterObserver(StorageBackend::Observer *observer)
{
    d->mObservers.removeAll(observer);
}

void StorageBackend::storageOpened(const Notebook::List &notebooks, const Notebook::Ptr &defaultNotebook)
{
    foreach (StorageBackend::Observer *observer, d->mObservers) {
        observer->storageOpened(this, notebooks, defaultNotebook);
    }
}

void StorageBackend::storageClosed()
{
    clearLoaded();

    foreach (StorageBackend::Observer *observer, d->mObservers) {
        observer->storageClosed(this);
    }
}

void StorageBackend::storageModified(const Notebook::List &notebooks, const Notebook::Ptr &defaultNotebook)
{
    clearLoaded();

    foreach (StorageBackend::Observer *observer, d->mObservers) {
        observer->storageModified(this, notebooks, defaultNotebook);
    }
}

void StorageBackend::storageUpdated(const StorageBackend::Collection &added,
                                    const StorageBackend::Collection &modified,
                                    const StorageBackend::Collection &deleted)
{
#if defined(TIMED_SUPPORT)
    if (!added.isEmpty())
        d->setAlarms(added);
    if (!modified.isEmpty())
        d->resetAlarms(modified);
    if (!deleted.isEmpty())
        d->clearAlarms(deleted);
#endif

    foreach (StorageBackend::Observer *observer, d->mObservers) {
        observer->storageUpdated(this, added, modified, deleted);
    }
}

void StorageBackend::incidenceLoaded(const StorageBackend::Collection &incidences)
{
    foreach (StorageBackend::Observer *observer, d->mObservers) {
        observer->incidenceLoaded(this, incidences);
    }
}

bool StorageBackend::addNotebook(const Notebook::Ptr &nb, bool isDefault)
{
    if (!nb) {
        return false;
    }

    if (!modifyNotebook(nb, DBInsert, isDefault)) {
        return false;
    }

#if defined(TIMED_SUPPORT)
    d->mNotebookVisibility.insert(nb->uid(), nb->isVisible());
#endif

    return true;
}

bool StorageBackend::updateNotebook(const Notebook::Ptr &nb, bool isDefault)
{
    if (!nb) {
        return false;
    }

    if (!modifyNotebook(nb, DBUpdate, isDefault)) {
        return false;
    }

#if defined(TIMED_SUPPORT)
    bool wasVisible = d->mNotebookVisibility.value(nb->uid());
    if (wasVisible && !nb->isVisible()) {
        d->clearAlarms(nb->uid());
    } else if (!wasVisible && nb->isVisible()) {
        Incidence::List list;
        if (allIncidences(&list, nb->uid())) {
            d->setAlarmsForNotebook(list, nb->uid());
        }
    }
    d->mNotebookVisibility.insert(nb->uid(), nb->isVisible());
#endif

    return true;
}

bool StorageBackend::deleteNotebook(const Notebook::Ptr &nb)
{
    if (!nb) {
        return false;
    }

    if (!modifyNotebook(nb, DBDelete, false)) {
        return false;
    }

#if defined(TIMED_SUPPORT)
    d->clearAlarms(nb->uid());
#endif

    return true;
}

Notebook::Ptr StorageBackend::createDefaultNotebook(QString name, QString color)
{
    // Could use QUuid::WithoutBraces when moving to Qt5.11.
    const QString uid(QUuid::createUuid().toString());
    if (name.isEmpty())
        name = "Default";
    if (color.isEmpty())
        color = "#0000FF";
    Notebook::Ptr nbDefault(new Notebook(uid.mid(1, uid.length() - 2),
                                         name, QString(), color,
                                         false, true, false, false, true));
    return nbDefault;
}

#if defined(TIMED_SUPPORT)
// Todo: move this into a service plugin that is a StorageBackend::Observer.
void StorageBackend::Private::resetAlarms(const StorageBackend::Collection &incidences)
{
    clearAlarms(incidences);
    setAlarms(incidences);
}

void StorageBackend::Private::setAlarms(const StorageBackend::Collection &incidences)
{
    const QDateTime now = QDateTime::currentDateTime();
    Timed::Event::List events;
    for (StorageBackend::Collection::ConstIterator it = incidences.constBegin();
         it != incidences.constEnd(); it++) {
        if (it.key().isEmpty() || !mNotebookVisibility.contains(it.key())
            || !mNotebookVisibility.value(it.key())) {
            continue;
        }
        setAlarms(it.value(), it.key(), events, now);
    }
    commitEvents(events);
}

void StorageBackend::Private::clearAlarms(const Incidence::Ptr &incidence)
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

void StorageBackend::Private::clearAlarms(const StorageBackend::Collection &incidences)
{
    for (const Incidence::Ptr incidence : incidences) {
        clearAlarms(incidence);
    }
}

void StorageBackend::Private::clearAlarms(const QString &notebookUid)
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

void StorageBackend::Private::setAlarmsForNotebook(const Incidence::List &incidences, const QString &notebookUid)
{
    const QDateTime now = QDateTime::currentDateTime();
    // list of all timed events
    Timed::Event::List events;
    foreach (const Incidence::Ptr incidence, incidences) {
        setAlarms(incidence, notebookUid, events, now);
    }
    commitEvents(events);
}

void StorageBackend::Private::setAlarms(const Incidence::Ptr &incidence,
                                         const QString &nbuid,
                                         Timed::Event::List &events,
                                         const QDateTime &now)
{
    if (incidence->status() == Incidence::StatusCanceled) {
        return;
    }

    const Alarm::List alarms = incidence->alarms();
    foreach (const Alarm::Ptr alarm, alarms) {
        if (!alarm->enabled()) {
            continue;
        }

        QDateTime preTime = now;
        if (incidence->recurs()) {
            QDateTime nextRecurrence = incidence->recurrence()->getNextDateTime(now);
            if (nextRecurrence.isValid() && alarm->startOffset().asSeconds() < 0) {
                if (now.addSecs(::abs(alarm->startOffset().asSeconds())) >= nextRecurrence) {
                    preTime = nextRecurrence;
                }
            }
        }

        QDateTime alarmTime = alarm->nextTime(preTime, true);
        if (!alarmTime.isValid()) {
            continue;
        }

        if (now.addSecs(60) > alarmTime) {
            // don't allow alarms at the same minute -> take next alarm if so
            alarmTime = alarm->nextTime(preTime.addSecs(60), true);
            if (!alarmTime.isValid()) {
                continue;
            }
        }
        Timed::Event &e = events.append();
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

void StorageBackend::Private::commitEvents(Timed::Event::List &events)
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
