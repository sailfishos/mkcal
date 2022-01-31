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
#include "logging_p.h"

#include <KCalendarCore/Exceptions>
#include <KCalendarCore/Calendar>
using namespace KCalendarCore;

#include <QtCore/QUuid>

#if defined(MKCAL_FOR_MEEGO)
# include <mlocale.h>
#endif

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
    QDate mStart;
    QDate mEnd;
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

    void setAlarmsForNotebook(const KCalendarCore::Incidence::List &incidences, const QString &nbuid);
#if defined(TIMED_SUPPORT)
    void setAlarms(const Incidence::Ptr &incidence, const QString &nbuid, Timed::Event::List &events, const QDateTime &now);
    void commitEvents(Timed::Event::List &events);
#endif
};
//@endcond

ExtendedStorage::ExtendedStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks)
    : CalStorage(cal),
      d(new ExtendedStorage::Private(validateNotebooks))
{
    // Add the calendar as observer
    registerObserver(cal.data());
}

ExtendedStorage::~ExtendedStorage()
{
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
    d->mStart = QDate();
    d->mEnd = QDate();
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
                                   QDateTime &loadStart, QDateTime &loadEnd)
{
    // Check the need to load from db.
    if (start.isValid() && d->mStart.isValid() && start >= d->mStart &&
            end.isValid() && d->mEnd.isValid() && end <= d->mEnd) {
        return false;
    }

    // Set load dates to load only what's necessary.
    if (start.isValid() && d->mStart.isValid() && start >= d->mStart) {
        loadStart.setDate(d->mEnd);
    } else {
        loadStart.setDate(start);   // may be null if start is not valid
    }

    if (end.isValid() && d->mEnd.isValid() && end <= d->mEnd) {
        loadEnd.setDate(d->mStart);
    } else {
        loadEnd.setDate(end);   // may be null if end is not valid
    }

    loadStart.setTimeZone(calendar()->timeZone());
    loadEnd.setTimeZone(calendar()->timeZone());

    qCDebug(lcMkcal) << "get load dates" << start << end << loadStart << loadEnd;

    return true;
}

void ExtendedStorage::setLoadDates(const QDate &start, const QDate &end)
{
    // Set dates.
    if (start.isValid() && (!d->mStart.isValid() || start < d->mStart)) {
        d->mStart = start;
    }
    if (end.isValid() && (!d->mEnd.isValid() || end > d->mEnd)) {
        d->mEnd = end;
    }

    qCDebug(lcMkcal) << "set load dates" << d->mStart << d->mEnd;
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

#if 0
void ExtendedStorage::ExtendedStorageObserver::storageModified(ExtendedStorage *storage,
                                                               const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);
}

void ExtendedStorage::ExtendedStorageObserver::storageProgress(ExtendedStorage *storage,
                                                               const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);
}

void ExtendedStorage::ExtendedStorageObserver::storageFinished(ExtendedStorage *storage,
                                                               bool error, const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(error);
    Q_UNUSED(info);
}
#endif

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
    // Clear all smart loading variables
    d->mStart = QDate();
    d->mEnd = QDate();
    d->mIsUncompletedTodosLoaded = false;
    d->mIsCompletedTodosDateLoaded = false;
    d->mIsCompletedTodosCreatedLoaded = false;
    d->mIsGeoDateLoaded = false;
    d->mIsGeoCreatedLoaded = false;
    d->mIsUnreadIncidencesLoaded = false;
    d->mIsInvitationIncidencesLoaded = false;

    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageModified(this, info);
    }
}

void ExtendedStorage::setProgress(const QString &info)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageProgress(this, info);
    }
}

void ExtendedStorage::setFinished(bool error, const QString &info)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageFinished(this, error, info);
    }
}

bool ExtendedStorage::addNotebook(const Notebook::Ptr &nb, bool signal)
{
    if (nb->uid().length() < 7) {
        // Cannot accept this id, create better one.
        QByteArray uid(QUuid::createUuid().toByteArray());
        nb->setUid(uid.mid(1, uid.length() - 2));
    }

    if (!nb || d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!calendar()->hasValidNotebook(nb->uid())
        && !calendar()->addNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "cannot add notebook" << nb->uid() << "to calendar";
        return false;
    }

    if (!modifyNotebook(nb, DBInsert, signal)) {
        if (!calendar()->deleteNotebook(nb->uid())) {
            qCWarning(lcMkcal) << "cannot delete notebook" << nb->uid() << "from calendar";
        }
        return false;
    }
    d->mNotebooks.insert(nb->uid(), nb);

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
    if (wasVisible && !nb->isVisible()) {
        clearAlarms(nb->uid());
    } else if (!wasVisible && nb->isVisible()) {
        Incidence::List list;
        if (allIncidences(&list, nb->uid())) {
            d->setAlarmsForNotebook(list, nb->uid());
        }
    }

    return true;
}

bool ExtendedStorage::deleteNotebook(const Notebook::Ptr &nb, bool onlyMemory)
{
    if (!nb || !d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!modifyNotebook(nb, DBDelete)) {
        return false;
    }

    // delete all notebook incidences from calendar
    if (!onlyMemory) {
        Incidence::List list;
        Incidence::List::Iterator it;
        if (allIncidences(&list, nb->uid())) {
            qCDebug(lcMkcal) << "deleting" << list.size() << "notes of notebook" << nb->name();
            for (it = list.begin(); it != list.end(); ++it) {
                load((*it)->uid(), (*it)->recurrenceId());
            }
            for (it = list.begin(); it != list.end(); ++it) {
                Incidence::Ptr toDelete = calendar()->incidence((*it)->uid(), (*it)->recurrenceId());
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
        qCWarning(lcMkcal) << "cannot delete notebook" << nb->uid() << "from calendar";
        return false;
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
    if (d->mDefaultNotebook) {
        return d->mDefaultNotebook;
    } else {
        return Notebook::Ptr();
    }
}

Notebook::List ExtendedStorage::notebooks()
{
    return d->mNotebooks.values();
}

Notebook::Ptr ExtendedStorage::notebook(const QString &uid)
{
    if (d->mNotebooks.contains(uid)) {
        return d->mNotebooks.value(uid);
    } else {
        return Notebook::Ptr();
    }
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

void ExtendedStorage::resetAlarms(const Incidence::Ptr &incidence)
{
    resetAlarms(Incidence::List(1, incidence));
}

void ExtendedStorage::resetAlarms(const Incidence::List &incidences)
{
    clearAlarms(incidences);
    setAlarms(incidences);
}

void ExtendedStorage::setAlarms(const Incidence::Ptr &incidence)
{
    setAlarms(Incidence::List(1, incidence));
}

void ExtendedStorage::setAlarms(const Incidence::List &incidences)
{
#if defined(TIMED_SUPPORT)
    const QDateTime now = QDateTime::currentDateTime();
    Timed::Event::List events;
    foreach (const Incidence::Ptr incidence, incidences) {
        // The incidence from the list must be in the calendar and in a notebook.
        const QString &nbuid = calendar()->notebook(incidence->uid());
        if (!calendar()->isVisible(incidence) || nbuid.isEmpty()) {
            continue;
        }
        d->setAlarms(incidence, nbuid, events, now);
    }
    d->commitEvents(events);
#else
    Q_UNUSED(incidences);
#endif
}

void ExtendedStorage::clearAlarms(const Incidence::Ptr &incidence)
{
#if defined(TIMED_SUPPORT)
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
#else
    Q_UNUSED(incidence);
#endif
}

void ExtendedStorage::clearAlarms(const KCalendarCore::Incidence::List &incidences)
{
    foreach (const Incidence::Ptr incidence, incidences) {
        clearAlarms(incidence);
    }
}

void ExtendedStorage::clearAlarms(const QString &nb)
{
#if defined(TIMED_SUPPORT)
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["notebook"] = nb;

    Timed::Interface timed;
    if (!timed.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << nb
                 << "alarm interface is not valid" << timed.lastError();
        return;
    }
    QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    if (!reply.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << nb << timed.lastError();
        return;
    }
    const QList<QVariant> &result = reply.value();
    for (int i = 0; i < result.size(); i++) {
        uint32_t cookie = result[i].toUInt();
        qCDebug(lcMkcal) << "removing alarm" << cookie << nb;
        QDBusReply<bool> reply = timed.cancel_sync(cookie);
        if (!reply.isValid() || !reply.value()) {
            qCWarning(lcMkcal) << "cannot remove alarm" << cookie << nb;
        }
    }
#else
    Q_UNUSED(nb);
#endif
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


Notebook::Ptr ExtendedStorage::createDefaultNotebook(QString name, QString color)
{
    QString uid;
#ifdef MKCAL_FOR_MEEGO
    if (name.isEmpty()) {
        MLocale locale;
        locale.installTrCatalog("calendar");
        MLocale::setDefault(locale);
        name = qtTrId("qtn_caln_personal_caln");
    }
    if (color.isEmpty())
        color = "#63B33B";
    uid = "11111111-2222-3333-4444-555555555555";
#else
    if (name.isEmpty())
        name = "Default";
    if (color.isEmpty())
        color = "#0000FF";
#endif
    Notebook::Ptr nbDefault = Notebook::Ptr(new Notebook(uid, name, QString(), color, false, true, false, false, true));
    addNotebook(nbDefault, false);
    setDefaultNotebook(nbDefault);
    return nbDefault;
}

void ExtendedStorage::Private::setAlarmsForNotebook(const KCalendarCore::Incidence::List &incidences, const QString &nbuid)
{
#if defined(TIMED_SUPPORT)
    const QDateTime now = QDateTime::currentDateTime();
    // list of all timed events
    Timed::Event::List events;
    foreach (const Incidence::Ptr incidence, incidences) {
        setAlarms(incidence, nbuid, events, now);
    }
    commitEvents(events);
#else
    Q_UNUSED(incidences);
    Q_UNUSED(nbuid);
#endif
}

#if defined(TIMED_SUPPORT)
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
