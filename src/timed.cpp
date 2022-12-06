/*
  This file is part of the mkcal library.

  Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>

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

#include "timed.h"
#include "extendedstorage.h"
#include "directstorageinterface.h"
#include "logging_p.h"

using namespace KCalendarCore;
using namespace mKCal;

#ifdef TIMED_SUPPORT
# include <timed-qt5/event-declarations.h>
# include <timed-qt5/exception.h>
# include <QtCore/QMap>
# include <QtDBus/QDBusReply>
using namespace Maemo;
static const QLatin1String RESET_ALARMS_CMD("invoker --type=generic -n /usr/bin/mkcaltool --reset-alarms");
#endif

void TimedPlugin::storageIncidenceAdded(DirectStorageInterface *storage,
                                        const Calendar *calendar,
                                        const Incidence::List &added)
{
    Q_UNUSED(storage);
#if defined(TIMED_SUPPORT)
    if (!added.isEmpty() && calendar) {
        setAlarms(*calendar, added);
    }
#endif    
}

void TimedPlugin::storageIncidenceModified(DirectStorageInterface *storage,
                                           const Calendar *calendar,
                                           const Incidence::List &modified)
{
    Q_UNUSED(storage);
#if defined(TIMED_SUPPORT)
    if (!modified.isEmpty() && calendar) {
        clearAlarms(modified);
        setAlarms(*calendar, modified);
    }
#endif    
}

void TimedPlugin::storageIncidenceDeleted(DirectStorageInterface *storage,
                                          const Calendar *calendar,
                                          const Incidence::List &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(calendar);
#if defined(TIMED_SUPPORT)
    if (!deleted.isEmpty()) {
        clearAlarms(deleted);
    }
#endif    
}

void TimedPlugin::storageNotebookModified(DirectStorageInterface *storage,
                                          const Notebook &nb, const Notebook &old)
{
#if defined(TIMED_SUPPORT)
    if (old.isVisible() && !nb.isVisible()) {
        clearAlarms(nb.uid());
    } else if (!old.isVisible() && nb.isVisible()) {
        Incidence::List list;
        if (storage->allIncidences(&list, nb.uid())) {
            MemoryCalendar::Ptr calendar(new MemoryCalendar(QTimeZone::utc()));
            if (calendar->addNotebook(nb.uid(), true)) {
                for (const Incidence::Ptr &incidence : const_cast<const Incidence::List&>(list)) {
                    calendar->addIncidence(incidence);
                    calendar->setNotebook(incidence, nb.uid());
                }
            }
            setAlarms(*calendar, calendar->incidences());
        }
    }
#endif
}

void TimedPlugin::storageNotebookDeleted(DirectStorageInterface *storage,
                                         const Notebook &nb)
{
#if defined(TIMED_SUPPORT)
    clearAlarms(nb.uid());
#endif
}

#if defined(TIMED_SUPPORT)
QDateTime TimedPlugin::getNextOccurrence(const Incidence::Ptr &incidence,
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

void TimedPlugin::setAlarms(const Calendar &calendar, const Incidence::List &incidences)
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
        const QString &nbuid = calendar.notebook(incidence->uid());
        if (!calendar.isVisible(incidence) || nbuid.isEmpty()) {
            continue;
        }
        if (incidence->recurs()) {
            const QDateTime next = getNextOccurrence(incidence, now, calendar.instances(incidence));
            addAlarms(incidence, nbuid, &events, next);
        } else if (incidence->hasRecurrenceId()) {
            const Incidence::Ptr parent = calendar.incidence(incidence->uid());
            if (parent && !recurringUids.contains(parent->uid())) {
                clearAlarms(parent);
                const QDateTime next = getNextOccurrence(parent, now, calendar.instances(parent));
                addAlarms(parent, nbuid, &events, next);
            }
            addAlarms(incidence, nbuid, &events, now);
        } else {
            addAlarms(incidence, nbuid, &events, now);
        }
    }
    commitEvents(events);
}

void TimedPlugin::clearAlarms(const Incidence::Ptr &incidence)
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

void TimedPlugin::clearAlarms(const Incidence::List &incidences)
{
    for (const Incidence::Ptr &incidence : incidences) {
        clearAlarms(incidence);
    }
}

void TimedPlugin::clearAlarms(const QString &notebookUid)
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

void TimedPlugin::addAlarms(const Incidence::Ptr &incidence,
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

void TimedPlugin::commitEvents(Timed::Event::List &events)
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
