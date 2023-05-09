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

#include "alarmhandler_p.h"
#include "logging_p.h"

using namespace mKCal;

#include <KCalendarCore/Todo>
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

bool AlarmHandler::clearAlarms(const QString &notebookUid, const QString &uid)
{
#if defined(TIMED_SUPPORT)
    Timed::Interface timed;
    if (!timed.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms,"
                 << "timed interface is not valid" << timed.lastError();
        return false;
    }

    QMap<QString, QVariant> query;
    query["APPLICATION"] = "libextendedkcal";
    query["notebook"] = notebookUid;
    if (!uid.isEmpty()) {
        query["uid"] = uid;
    }
    QDBusReply<QList<QVariant> > reply = timed.query_sync(query);
    if (!reply.isValid()) {
        qCWarning(lcMkcal) << "cannot get alarm cookies" << timed.lastError();
        return false;
    }
    QList<uint> cookies;
    for (const QVariant &variant : reply.value()) {
        cookies.append(variant.toUInt());
    }
    if (!cookies.isEmpty()) {
        QDBusReply<QList<uint>> reply = timed.cancel_events_sync(cookies);
        if (!reply.isValid() || !reply.value().isEmpty()) {
            qCWarning(lcMkcal) << "cannot remove alarms" << cookies;
        }
    }
#endif
    return true;
}

static bool cancelAlarms(const QSet<QPair<QString, QString>> &uids)
{
#if defined(TIMED_SUPPORT)
    if (uids.count() == 1) {
        QPair<QString, QString> id = uids.values()[0];
        return AlarmHandler::clearAlarms(id.first, id.second);
    }

    Timed::Interface timed;
    if (!timed.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms,"
                 << "timed interface is not valid" << timed.lastError();
        return false;
    }

    QMap<QString, QVariant> query;
    query["APPLICATION"] = "libextendedkcal";
    QDBusReply<QList<QVariant> > reply = timed.query_sync(query);
    if (!reply.isValid()) {
        qCWarning(lcMkcal) << "cannot get alarm cookies" << timed.lastError();
        return false;
    }
    QList<uint> cookiesAll;
    for (const QVariant &variant : reply.value()) {
        cookiesAll.append(variant.toUInt());
    }
    QDBusReply<QMap<uint, QMap<QString,QString> >> attributes = timed.get_attributes_by_cookies_sync(cookiesAll);
    if (!attributes.isValid()) {
        qCWarning(lcMkcal) << "cannot get alarm attributes" << timed.lastError();
        return false;
    }
    QList<uint> cookiesDoomed;
    const QMap<uint, QMap<QString,QString> > map = attributes.value();
    for (QMap<uint, QMap<QString,QString> >::ConstIterator it = map.constBegin();
         it != map.constEnd(); it++) {
        const QString notebook = it.value()["notebook"];
        const QString uid = it.value()["uid"];

        if (uids.contains(QPair<QString, QString>(notebook, uid))
            || uids.contains(QPair<QString, QString>(notebook, QString()))) {
            qCDebug(lcMkcal) << "removing alarm" << it.key() << notebook << uid;
            cookiesDoomed.append(it.key());
        }
    }
    if (!cookiesDoomed.isEmpty()) {
        QDBusReply<QList<uint>> reply = timed.cancel_events_sync(cookiesDoomed);
        if (!reply.isValid() || !reply.value().isEmpty()) {
            qCWarning(lcMkcal) << "cannot remove alarms" << cookiesDoomed;
        }
    }
#endif
    return true;
}

#if defined(TIMED_SUPPORT)
static QDateTime getNextOccurrence(const Recurrence *recurrence,
                                   const QDateTime &start,
                                   const QSet<QDateTime> &recurrenceIds)
{
    QDateTime match = start;
    if (!recurrence->recursAt(start) || recurrenceIds.contains(start)) {
        do {
            match = recurrence->getNextDateTime(match);
        } while (match.isValid() && recurrenceIds.contains(match));
    }
    return match;
}

static void addAlarms(Timed::Event::List *events,
                      const QString &notebookUid, const Incidence &incidence,
                      const QDateTime &laterThan)
{
    if (incidence.status() == Incidence::StatusCanceled || laterThan.isNull()) {
        return;
    }

    const QDateTime now = QDateTime::currentDateTime();
    const Alarm::List alarms = incidence.alarms();
    foreach (const Alarm::Ptr alarm, alarms) {
        if (!alarm->enabled()) {
            continue;
        }

        QDateTime preTime = laterThan;
        if (incidence.recurs() && alarm->startOffset().asSeconds() < 0) {
            // by construction for recurring events, laterThan is the time of the
            // actual next occurrence, so one need to remove the alarm offset.
            preTime = preTime.addSecs(alarm->startOffset().asSeconds());
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

        s = incidence.summary();
        // Timed braindeath: Required field, BUT if empty, it asserts
        if (s.isEmpty()) {
            s = ' ';
        }
        e.setAttribute("TITLE", s);
        e.setAttribute("PLUGIN", "libCalendarReminder");
        e.setAttribute("APPLICATION", "libextendedkcal");
        //e.setAttribute( "translation", "organiser" );
        // This really has to exist or code is badly broken
        Q_ASSERT(!incidence.uid().isEmpty());
        e.setAttribute("uid", incidence.uid());
#ifndef QT_NO_DEBUG_OUTPUT //Helps debuggin
        e.setAttribute("alarmtime", alarmTime.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
#endif
        if (!incidence.location().isEmpty()) {
            e.setAttribute("location", incidence.location());
        }
        if (incidence.recurs()) {
            e.setAttribute("recurs", "true");
            Timed::Event::Action &a = e.addAction();
            a.runCommand(QString("%1 %2 %3")
                         .arg(RESET_ALARMS_CMD)
                         .arg(notebookUid)
                         .arg(incidence.uid()));
            a.whenServed();
        }

        // TODO - consider this how it should behave for recurrence
        if ((incidence.type() == Incidence::TypeTodo)) {
            const Todo *todo = static_cast<const Todo*>(&incidence);

            if (todo->hasDueDate()) {
                e.setAttribute("time", todo->dtDue(true).toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            }
            e.setAttribute("type", "todo");
        } else if (incidence.dtStart().isValid()) {
            QDateTime eventStart;

            if (incidence.recurs()) {
                // assuming alarms not later than event start
                eventStart = incidence.recurrence()->getNextDateTime(alarmTime.addSecs(-60));
            } else {
                eventStart = incidence.dtStart();
            }
            e.setAttribute("time", eventStart.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            e.setAttribute("startDate", eventStart.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            if (incidence.endDateForStart(eventStart).isValid()) {
                e.setAttribute("endDate", incidence.endDateForStart(eventStart).toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            }
            e.setAttribute("type", "event");
        }

        if (incidence.hasRecurrenceId()) {
            e.setAttribute("recurrenceId", incidence.recurrenceId().toString(Qt::ISODate));
        }
        e.setAttribute("notebook", notebookUid);

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
#endif

bool AlarmHandler::setupAlarms(const QString &notebookUid, const QString &uid)
{
    QSet<QPair<QString, QString>> uids;
    uids.insert(QPair<QString, QString>(notebookUid, uid));
    return setupAlarms(uids);
}

bool AlarmHandler::setupAlarms(const QSet<QPair<QString, QString>> &uids)
{
#if defined(TIMED_SUPPORT)
    cancelAlarms(uids);

    const QDateTime now = QDateTime::currentDateTime();
    Timed::Event::List events;
    for (QSet<QPair<QString, QString>>::ConstIterator it = uids.constBegin();
         it != uids.constEnd(); it++) {
        Incidence::List list = incidencesWithAlarms(it->first, it->second);
        QSet<QDateTime> recurrenceIds;
        for (Incidence::List::ConstIterator inc = list.constBegin();
             inc != list.constEnd(); inc++) {
            if ((*inc)->hasRecurrenceId()) {
                recurrenceIds.insert((*inc)->recurrenceId());
            }
        }
        for (Incidence::List::ConstIterator inc = list.constBegin();
             inc != list.constEnd(); inc++) {
            if ((*inc)->recurs()) {
                addAlarms(&events, it->first, **inc,
                          getNextOccurrence((*inc)->recurrence(), now, recurrenceIds));
            } else {
                addAlarms(&events, it->first, **inc, now);
            }
        }
    }
    if (events.count() > 0) {
        Timed::Interface timed;
        if (!timed.isValid()) {
            qCWarning(lcMkcal) << "cannot set alarm for incidence: "
                               << "alarm interface is not valid" << timed.lastError();
            return false;
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
            return false;
        }
    } else {
        qCDebug(lcMkcal) << "No alarms to send";
    }
#endif
    return true;
}
