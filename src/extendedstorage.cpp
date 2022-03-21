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

#include <QtCore/QUuid>

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
          mIsUncompletedTodosLoaded(false),
          mIsCompletedTodosDateLoaded(false),
          mIsCompletedTodosCreatedLoaded(false),
          mIsDateLoaded(false),
          mIsCreatedLoaded(false),
          mIsFutureDateLoaded(false),
          mIsGeoDateLoaded(false),
          mIsGeoCreatedLoaded(false),
          mIsUnreadIncidencesLoaded(false),
          mIsInvitationIncidencesLoaded(false),
          mIsJournalsLoaded(false),
          mDefaultNotebook(0)
    {}

    bool mValidateNotebooks;
    QList<Range> mRanges;
    bool mIsRecurrenceLoaded;
    bool mIsUncompletedTodosLoaded;
    bool mIsCompletedTodosDateLoaded;
    bool mIsCompletedTodosCreatedLoaded;
    bool mIsDateLoaded;
    bool mIsCreatedLoaded;
    bool mIsFutureDateLoaded;
    bool mIsGeoDateLoaded;
    bool mIsGeoCreatedLoaded;
    bool mIsUnreadIncidencesLoaded;
    bool mIsInvitationIncidencesLoaded;
    bool mIsJournalsLoaded;
    QList<ExtendedStorageObserver *> mObservers;
    QHash<QString, Notebook::Ptr> mNotebooks; // uid to notebook
    Notebook::Ptr mDefaultNotebook;

#if defined(TIMED_SUPPORT)
    // These alarm methods are used to communicate with an external
    // daemon, like timed, to bind Incidence::Alarm with the system notification.
    void clearAlarms(const Incidence::Ptr &incidence);
    void clearAlarms(const Incidence::List &incidences);
    void clearAlarms(const QString &notebookUid);
    void setAlarms(const Incidence::List &incidences, const Calendar::Ptr &calendar);
    void resetAlarms(const Incidence::List &incidences, const Calendar::Ptr &calendar);

    void setAlarmsForNotebook(const Incidence::List &incidences, const QString &notebookUid);
    void setAlarms(const Incidence::Ptr &incidence, const QString &nbuid, Timed::Event::List &events, const QDateTime &now);
    void commitEvents(Timed::Event::List &events);
#endif
};
//@endcond

ExtendedStorage::ExtendedStorage(const Calendar::Ptr &cal, bool validateNotebooks)
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
    clearLoaded();

    d->mNotebooks.clear();
    d->mDefaultNotebook = Notebook::Ptr();

    return true;
}

void ExtendedStorage::clearLoaded()
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
    d->mIsUnreadIncidencesLoaded  = false;
    d->mIsInvitationIncidencesLoaded  = false;
    d->mIsJournalsLoaded = false;
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

    loadStart->setTimeZone(calendar()->timeZone());
    loadEnd->setTimeZone(calendar()->timeZone());

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

bool ExtendedStorage::isUncompletedTodosLoaded()
{
    return d->mIsUncompletedTodosLoaded;
}

void ExtendedStorage::setIsUncompletedTodosLoaded(bool loaded)
{
    d->mIsUncompletedTodosLoaded = loaded;
}

bool ExtendedStorage::isCompletedTodosDateLoaded()
{
    return d->mIsCompletedTodosDateLoaded;
}

void ExtendedStorage::setIsCompletedTodosDateLoaded(bool loaded)
{
    d->mIsCompletedTodosDateLoaded = loaded;
}

bool ExtendedStorage::isCompletedTodosCreatedLoaded()
{
    return d->mIsCompletedTodosCreatedLoaded;
}

void ExtendedStorage::setIsCompletedTodosCreatedLoaded(bool loaded)
{
    d->mIsCompletedTodosCreatedLoaded = loaded;
}

bool ExtendedStorage::isDateLoaded()
{
    return d->mIsDateLoaded;
}

void ExtendedStorage::setIsDateLoaded(bool loaded)
{
    d->mIsDateLoaded = loaded;
}

bool ExtendedStorage::isFutureDateLoaded()
{
    return d->mIsFutureDateLoaded;
}

void ExtendedStorage::setIsFutureDateLoaded(bool loaded)
{
    d->mIsFutureDateLoaded = loaded;
}

bool ExtendedStorage::isJournalsLoaded()
{
    return d->mIsJournalsLoaded;
}

void ExtendedStorage::setIsJournalsLoaded(bool loaded)
{
    d->mIsJournalsLoaded = loaded;
}

bool ExtendedStorage::isCreatedLoaded()
{
    return d->mIsCreatedLoaded;
}

void ExtendedStorage::setIsCreatedLoaded(bool loaded)
{
    d->mIsCreatedLoaded = loaded;
}

bool ExtendedStorage::isGeoDateLoaded()
{
    return d->mIsGeoDateLoaded;
}

void ExtendedStorage::setIsGeoDateLoaded(bool loaded)
{
    d->mIsGeoDateLoaded = loaded;
}

bool ExtendedStorage::isGeoCreatedLoaded()
{
    return d->mIsGeoCreatedLoaded;
}

void ExtendedStorage::setIsGeoCreatedLoaded(bool loaded)
{
    d->mIsGeoCreatedLoaded = loaded;
}

bool ExtendedStorage::isUnreadIncidencesLoaded()
{
    return d->mIsUnreadIncidencesLoaded;
}

void ExtendedStorage::setIsUnreadIncidencesLoaded(bool loaded)
{
    d->mIsUnreadIncidencesLoaded = loaded;
}

bool ExtendedStorage::isInvitationIncidencesLoaded()
{
    return d->mIsInvitationIncidencesLoaded;
}

void ExtendedStorage::setIsInvitationIncidencesLoaded(bool loaded)
{
    d->mIsInvitationIncidencesLoaded = loaded;
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
    clearLoaded();
    d->mNotebooks.clear();
    d->mDefaultNotebook.clear();
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
    if (!modified.isEmpty())
        d->resetAlarms(modified, calendar());
    if (!deleted.isEmpty())
        d->clearAlarms(deleted);
#endif
}

bool ExtendedStorage::addNotebook(const Notebook::Ptr &nb)
{
    if (nb->uid().length() < 7) {
        // Cannot accept this id, create better one.
        QString uid(QUuid::createUuid().toString());
        nb->setUid(uid.mid(1, uid.length() - 2));
    }

    if (!nb || d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!modifyNotebook(nb, DBInsert)) {
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

    bool wasVisible = calendar()->isVisible(nb->uid());
    if (!calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "cannot update notebook" << nb->uid() << "in calendar";
        return false;
    }
    if (!modifyNotebook(nb, DBUpdate)) {
        return false;
    }
#if defined(TIMED_SUPPORT)
    if (wasVisible && !nb->isVisible()) {
        d->clearAlarms(nb->uid());
    } else if (!wasVisible && nb->isVisible()) {
        Incidence::List list;
        if (allIncidences(&list, nb->uid())) {
            d->setAlarmsForNotebook(list, nb->uid());
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

    if (!modifyNotebook(nb, DBDelete)) {
        return false;
    }

    // purge all notebook incidences from storage
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
    if (!nb || !d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (d->mDefaultNotebook) {
        d->mDefaultNotebook->setIsDefault(false);
        if (!modifyNotebook(d->mDefaultNotebook, DBUpdate, false)) {
            return false;
        }
    }

    d->mDefaultNotebook = nb;
    d->mDefaultNotebook->setIsDefault(true);
    if (!modifyNotebook(d->mDefaultNotebook, DBUpdate)) {
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

Notebook::Ptr ExtendedStorage::notebook(const QString &uid)
{
    return d->mNotebooks.value(uid);
}

void ExtendedStorage::setValidateNotebooks(bool validateNotebooks)
{
    d->mValidateNotebooks = validateNotebooks;
}

bool ExtendedStorage::validateNotebooks()
{
    return d->mValidateNotebooks;
}

bool ExtendedStorage::isValidNotebook(const QString &notebookUid)
{
    Notebook::Ptr nb = notebook(notebookUid);
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
    // Could use QUuid::WithoutBraces when moving to Qt5.11.
    const QString uid(QUuid::createUuid().toString());
    if (name.isEmpty())
        name = "Default";
    if (color.isEmpty())
        color = "#0000FF";
    Notebook::Ptr nbDefault(new Notebook(uid.mid(1, uid.length() - 2), name, QString(), color,
                                         false, true, false, false, true));
    if (modifyNotebook(nbDefault, DBInsert, false)) {
        d->mNotebooks.insert(nbDefault->uid(), nbDefault);
        if (!calendar()->addNotebook(nbDefault->uid(), nbDefault->isVisible())
            && !calendar()->updateNotebook(nbDefault->uid(), nbDefault->isVisible())) {
            qCWarning(lcMkcal) << "notebook" << nbDefault->uid() << "already in calendar";
        }
    }
    setDefaultNotebook(nbDefault);
    return nbDefault;
}

Incidence::Ptr ExtendedStorage::checkAlarm(const QString &uid, const QString &recurrenceId,
                                           bool loadAlways)
{
    QDateTime rid;

    if (!recurrenceId.isEmpty()) {
        rid = QDateTime::fromString(recurrenceId, Qt::ISODate);
    }
    Incidence::Ptr incidence = calendar()->incidence(uid, rid);
    if (!incidence || loadAlways) {
        load(uid, rid);
        incidence = calendar()->incidence(uid, rid);
    }
    if (incidence && incidence->hasEnabledAlarms()) {
        // Return incidence if it exists and has active alarms.
        return incidence;
    }
    return Incidence::Ptr();
}

#if defined(TIMED_SUPPORT)
// Todo: move this into a service plugin that is a ExtendedStorageObserver.
void ExtendedStorage::Private::resetAlarms(const Incidence::List &incidences,
                                           const Calendar::Ptr &calendar)
{
    clearAlarms(incidences);
    setAlarms(incidences, calendar);
}

void ExtendedStorage::Private::setAlarms(const Incidence::List &incidences,
                                         const Calendar::Ptr &calendar)
{
    const QDateTime now = QDateTime::currentDateTime();
    Timed::Event::List events;
    foreach (const Incidence::Ptr incidence, incidences) {
        // The incidence from the list must be in the calendar and in a notebook.
        const QString &nbuid = calendar->notebook(incidence->uid());
        if (!calendar->isVisible(incidence) || nbuid.isEmpty()) {
            continue;
        }
        setAlarms(incidence, nbuid, events, now);
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

void ExtendedStorage::Private::clearAlarms(const KCalendarCore::Incidence::List &incidences)
{
    foreach (const Incidence::Ptr incidence, incidences) {
        clearAlarms(incidence);
    }
}

void ExtendedStorage::Private::clearAlarms(const QString &notebookUid)
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

void ExtendedStorage::Private::setAlarmsForNotebook(const Incidence::List &incidences, const QString &notebookUid)
{
    const QDateTime now = QDateTime::currentDateTime();
    // list of all timed events
    Timed::Event::List events;
    foreach (const Incidence::Ptr incidence, incidences) {
        setAlarms(incidence, notebookUid, events, now);
    }
    commitEvents(events);
}

void ExtendedStorage::Private::setAlarms(const Incidence::Ptr &incidence,
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
