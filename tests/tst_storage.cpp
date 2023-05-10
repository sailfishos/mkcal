/*
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

#include <QTest>
#include <QDebug>
#include <QTimeZone>
#include <QSignalSpy>

#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/OccurrenceIterator>

#include <sqlite3.h>

#include "dummystorage.h" // Not used, but tests API compilation

#include "tst_storage.h"
#include "sqlitestorage.h"
#ifdef TIMED_SUPPORT
#include <timed-qt5/interface.h>
#include <QtCore/QMap>
#include <QtDBus/QDBusReply>
using namespace Maemo;
#endif

// random
const char *const NotebookId("12345678-9876-1111-2222-222222222222");

tst_storage::tst_storage(QObject *parent)
    : QObject(parent)
{
}

void tst_storage::initTestCase()
{
    ExtendedCalendar::Ptr dummyCal(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    DummyStorage dummy(dummyCal);
    QVERIFY(dummy.open());

    openDb(true);
}

void tst_storage::cleanupTestCase()
{
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NotebookId);
    if (notebook)
        QVERIFY(m_storage->deleteNotebook(notebook));
}

void tst_storage::init()
{
    openDb(true);
}

void tst_storage::cleanup()
{
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NotebookId);
    if (notebook)
        QVERIFY(m_storage->deleteNotebook(notebook));
}

// Ensure that dissociateSingleOccurrence() for events
// given in various time zone or for all day events.
void tst_storage::tst_dissociateSingleOccurrence_data()
{
    QTest::addColumn<QDateTime>("dateTime");

    QTest::newRow("local time")
        << QDateTime(QDate(2019, 05, 21), QTime(12, 0), Qt::LocalTime);
    QTest::newRow("UTC time")
        << QDateTime(QDate(2019, 05, 21), QTime(12, 0), Qt::UTC);
    QTest::newRow("time zone")
        << QDateTime(QDate(2019, 05, 21), QTime(12, 0), QTimeZone("Europe/Helsinki"));

    QTest::newRow("all day, local time")
        << QDateTime(QDate(2019, 05, 21), QTime(), Qt::LocalTime);
    QTest::newRow("all day, UTC time")
        << QDateTime(QDate(2019, 05, 21), QTime(), Qt::UTC);
    QTest::newRow("all day, time zone")
        << QDateTime(QDate(2019, 05, 21), QTime(), QTimeZone("Europe/Helsinki"));
}

void tst_storage::tst_dissociateSingleOccurrence()
{
    QFETCH(QDateTime, dateTime);

    KCalendarCore::Event::Ptr event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(dateTime);
    if (dateTime.time().msecsSinceStartOfDay()) {
        event->setDtEnd(event->dtStart().addSecs(3600));
        event->setSummary("Reccurring event");
    } else {
        event->setAllDay(true);
        event->setSummary("Reccurring event all day");
    }
    event->setCreated(QDateTime::currentDateTimeUtc().addDays(-1));

    KCalendarCore::Recurrence *recurrence = event->recurrence();
    recurrence->setDaily(1);
    recurrence->setStartDateTime(event->dtStart(), false);
    QVERIFY(event->recurs());

    QDateTime createdDate = event->created();
    QDateTime recId = event->dtStart().addDays(1);
    KCalendarCore::Incidence::Ptr occurrence = m_calendar->dissociateSingleOccurrence(event, recId);
    QVERIFY(occurrence);
    QVERIFY(occurrence->hasRecurrenceId());
    QCOMPARE(occurrence->recurrenceId(), recId);
    QCOMPARE(event->created(), createdDate);
    QVERIFY(occurrence->created().secsTo(QDateTime::currentDateTimeUtc()) < 2);

    m_calendar->addEvent(event, NotebookId);
    m_calendar->addEvent(occurrence.staticCast<KCalendarCore::Event>(), NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb();

    KCalendarCore::Event::Ptr fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    QCOMPARE(fetchEvent->allDay(), event->allDay());
    QVERIFY(fetchEvent->recurs());

    KCalendarCore::Incidence::List occurences = m_calendar->instances(event);
    QCOMPARE(occurences.length(), 1);
    QCOMPARE(occurences[0]->recurrenceId(), recId);

    KCalendarCore::Event::Ptr fetchOccurrence = m_calendar->event(event->uid(), recId);
    QVERIFY(fetchOccurrence);
    QVERIFY(fetchOccurrence->hasRecurrenceId());
    QCOMPARE(fetchOccurrence->recurrenceId(), recId);
}

// Accessor check for the deleted incidences.
void tst_storage::tst_deleted()
{
    mKCal::Notebook::Ptr notebook =
        mKCal::Notebook::Ptr(new mKCal::Notebook("123456789-deletion",
                                                 "test notebook for deletion",
                                                 QLatin1String(""),
                                                 "#001122",
                                                 false, // Not shared.
                                                 true, // Is master.
                                                 false, // Not synced to Ovi.
                                                 false, // Writable.
                                                 true, // Visible.
                                                 QLatin1String(""),
                                                 QLatin1String(""),
                                                 0));
    m_storage->addNotebook(notebook);

    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime::currentDateTimeUtc());
    event->setSummary("Deleted event");
    event->setCreated(QDateTime::currentDateTimeUtc().addSecs(-3));
    const QString customValue = QLatin1String("A great value");
    event->setNonKDECustomProperty("X-TEST-PROPERTY", customValue);

    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setDtStart(QDateTime::currentDateTimeUtc());
    event2->setSummary("Purged event on save");
    event2->setCreated(QDateTime::currentDateTimeUtc().addSecs(-3));

    KCalendarCore::Event::Ptr event3(new KCalendarCore::Event);
    event3->setDtStart(QDateTime::currentDateTimeUtc());
    event3->setSummary("Re-created event");
    event3->setCreated(QDateTime::currentDateTimeUtc().addSecs(-3));

    KCalendarCore::Event::Ptr event4(new KCalendarCore::Event);
    event4->setDtStart(QDateTime(QDate(2022, 12, 6), QTime(13, 42)));
    event4->setSummary("Recurring event wth exceptions");
    event4->setCreated(QDateTime::currentDateTimeUtc().addSecs(-3));
    event4->recurrence()->setDaily(1);
    KCalendarCore::Event::Ptr event41(event4->clone());
    event41->clearRecurrence();
    event41->setRecurrenceId(event4->dtStart().addDays(2));
    event41->setSummary("Exception 1");
    KCalendarCore::Event::Ptr event42(event4->clone());
    event42->clearRecurrence();
    event42->setRecurrenceId(event4->dtStart().addDays(3));
    event42->setSummary("Exception 2");

    QVERIFY(m_calendar->addEvent(event, notebook->uid()));
    QVERIFY(m_calendar->addEvent(event2, notebook->uid()));
    QVERIFY(m_calendar->addEvent(event3, notebook->uid()));
    QVERIFY(m_storage->save());
    reloadDb();
    QVERIFY(m_storage->loadNotebookIncidences(notebook->uid()));

    KCalendarCore::Event::Ptr fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    QCOMPARE(fetchEvent->nonKDECustomProperty("X-TEST-PROPERTY"), customValue);

    QVERIFY(m_calendar->deleteIncidence(fetchEvent));
    QVERIFY(!m_calendar->event(fetchEvent->uid()));
    QVERIFY(m_calendar->deletedEvent(fetchEvent->uid()));

    // Deleted events are marked as deleted but remains in the DB
    QVERIFY(m_storage->save());

    // One can then add the same event in another notebook (here the default one).
    QVERIFY(m_calendar->addEvent(event));
    QVERIFY(m_storage->save());

    // And later on, also delete it.
    reloadDb();
    QVERIFY(m_storage->loadNotebookIncidences(m_calendar->defaultNotebook()));
    fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    QThread::sleep(1); // Need to sleep to avoid UNIQUE constrain violation
                       // by deleting the same event at the same time than
                       // on the other notebook.
    QVERIFY(m_calendar->deleteIncidence(fetchEvent));
    QVERIFY(m_storage->save());

    // Check that event is listed as deleted from notebook.
    KCalendarCore::Incidence::List deleted;
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(1), notebook->uid()));
    QVERIFY(deleted.isEmpty());
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), notebook->uid()));
    QCOMPARE(deleted.length(), 1);
    QCOMPARE(deleted[0]->uid(), event->uid());
    QCOMPARE(deleted[0]->nonKDECustomProperty("X-TEST-PROPERTY"), customValue);
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime(), "123456789-deletion"));
    QCOMPARE(deleted.length(), 1);

    // One can purge from DB previously deleted events from calendar
    QVERIFY(m_storage->purgeDeletedIncidences(deleted, notebook->uid()));
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), notebook->uid()));
    QCOMPARE(deleted.length(), 0);
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), m_calendar->defaultNotebook()));
    QCOMPARE(deleted.length(), 1);
    QVERIFY(m_storage->purgeDeletedIncidences(deleted, m_calendar->defaultNotebook()));
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), m_calendar->defaultNotebook()));
    QCOMPARE(deleted.length(), 0);

    // One can purge deleted events from DB directly when they are
    // removed from a calendar.
    QVERIFY(m_storage->loadNotebookIncidences(notebook->uid()));
    KCalendarCore::Event::Ptr fetchEvent2 = m_calendar->event(event2->uid());
    QVERIFY(fetchEvent2);
    QVERIFY(m_calendar->deleteIncidence(fetchEvent2));
    QVERIFY(m_storage->save(ExtendedStorage::PurgeDeleted));
    reloadDb();
    QVERIFY(m_storage->loadNotebookIncidences(notebook->uid()));
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), notebook->uid()));
    QCOMPARE(deleted.length(), 0);

    // One can re-create event after deletion using the same UID
    KCalendarCore::Event::Ptr fetchEvent3 = m_calendar->event(event3->uid());
    QVERIFY(fetchEvent3);
    QVERIFY(m_calendar->deleteIncidence(fetchEvent3));
    QVERIFY(m_storage->save());
    reloadDb();
    QVERIFY(m_storage->loadNotebookIncidences(notebook->uid()));
    // For instance a sync wants to re-create this because remote override local deletion
    QVERIFY(m_calendar->addEvent(event3, notebook->uid()));
    QVERIFY(m_storage->save());
    reloadDb();
    QVERIFY(m_storage->loadNotebookIncidences(notebook->uid()));
    fetchEvent3 = m_calendar->event(event3->uid());
    QVERIFY(fetchEvent3);
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), notebook->uid()));
    QCOMPARE(deleted.length(), 0);

    // Marking as deleted a recurring event marks the exceptions also,
    // even if they are not explicitely loaded
    QVERIFY(m_calendar->addEvent(event4, notebook->uid()));
    QVERIFY(m_calendar->addEvent(event41, notebook->uid()));
    QVERIFY(m_calendar->addEvent(event42, notebook->uid()));
    QVERIFY(m_storage->save());
    reloadDb();
    QVERIFY(m_storage->load(event4->uid()));
    KCalendarCore::Event::Ptr fetchEvent4 = m_calendar->event(event4->uid());
    QVERIFY(fetchEvent4);
    QVERIFY(m_calendar->deleteIncidence(fetchEvent4));
    QVERIFY(m_storage->save());
    reloadDb();
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), notebook->uid()));
    QCOMPARE(deleted.length(), 3);

    // Deleting a recurring event also wipes out the exceptions,
    // even if they are not explicitely loaded
    QVERIFY(m_calendar->addEvent(event4, notebook->uid()));
    QVERIFY(m_calendar->addEvent(event41, notebook->uid()));
    QVERIFY(m_calendar->addEvent(event42, notebook->uid()));
    QVERIFY(m_storage->save());
    reloadDb();
    QVERIFY(m_storage->load(event4->uid()));
    fetchEvent4 = m_calendar->event(event4->uid());
    QVERIFY(fetchEvent4);
    QVERIFY(m_calendar->deleteIncidence(fetchEvent4));
    QVERIFY(m_storage->save(ExtendedStorage::PurgeDeleted));
    reloadDb();
    QVERIFY(m_storage->loadNotebookIncidences(notebook->uid()));
    QVERIFY(!m_calendar->event(event4->uid()));
    QVERIFY(!m_calendar->event(event41->uid(), event41->recurrenceId()));
    QVERIFY(!m_calendar->event(event42->uid(), event42->recurrenceId()));
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), notebook->uid()));
    QCOMPARE(deleted.length(), 0);

    // Test notebook deletion (with non-purged deleted incidences)
    QVERIFY(m_calendar->deleteIncidence(fetchEvent3));
    QVERIFY(m_storage->save());
    reloadDb();
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime(), notebook->uid()));
    QCOMPARE(deleted.length(), 1);
    QVERIFY(m_storage->deleteNotebook(m_storage->notebook(notebook->uid())));
    reloadDb();
    QVERIFY(m_storage->notebook(notebook->uid()).isNull());
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime(), notebook->uid()));
    QVERIFY(deleted.isEmpty());
    KCalendarCore::Incidence::List incidences;
    QVERIFY(m_storage->allIncidences(&incidences, notebook->uid()));
    QVERIFY(incidences.isEmpty());
}

// Accessor check for modified incidences.
void tst_storage::tst_modified()
{
    mKCal::Notebook::Ptr notebook =
        mKCal::Notebook::Ptr(new mKCal::Notebook("123456789-modified",
                                                 "test notebook",
                                                 QLatin1String(""),
                                                 "#001122",
                                                 false, // Not shared.
                                                 true, // Is master.
                                                 false, // Not synced to Ovi.
                                                 false, // Writable.
                                                 true, // Visible.
                                                 QLatin1String(""),
                                                 QLatin1String(""),
                                                 0));
    m_storage->addNotebook(notebook);

    KCalendarCore::Event::Ptr event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(QDateTime::currentDateTimeUtc());
    event->setSummary("Base event");
    event->setCreated(QDateTime::currentDateTimeUtc().addSecs(-3));

    QVERIFY(m_calendar->addEvent(event, "123456789-modified"));
    m_storage->save();
    reloadDb();
    m_storage->loadNotebookIncidences("123456789-modified");

    KCalendarCore::Event::Ptr fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);

    fetchEvent->setSummary("Modified event");

    m_storage->save();
    reloadDb();
    m_storage->loadNotebookIncidences("123456789-modified");

    KCalendarCore::Incidence::List modified;
    QVERIFY(m_storage->modifiedIncidences(&modified, QDateTime::currentDateTimeUtc().addSecs(-2), "123456789-modified"));
    QCOMPARE(modified.length(), 1);
    QCOMPARE(modified[0]->uid(), event->uid());
}

// Accessor check for added incidences, including added incidence from
// dissociation of a recurring event.
void tst_storage::tst_inserted()
{
    mKCal::Notebook::Ptr notebook =
        mKCal::Notebook::Ptr(new mKCal::Notebook("123456789-inserted",
                                                 "test notebook",
                                                 QLatin1String(""),
                                                 "#001122",
                                                 false, // Not shared.
                                                 true, // Is master.
                                                 false, // Not synced to Ovi.
                                                 false, // Writable.
                                                 true, // Visible.
                                                 QLatin1String(""),
                                                 QLatin1String(""),
                                                 0));
    m_storage->addNotebook(notebook);

    KCalendarCore::Event::Ptr event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(QDateTime::currentDateTimeUtc());
    event->setSummary("Inserted event");
    event->setCreated(QDateTime::currentDateTimeUtc().addSecs(-10));

    KCalendarCore::Recurrence *recurrence = event->recurrence();
    recurrence->setDaily(1);
    recurrence->setStartDateTime(event->dtStart(), false);
    QVERIFY(event->recurs());

    QVERIFY(m_calendar->addEvent(event, "123456789-inserted"));
    m_storage->save();
    reloadDb();
    m_storage->loadNotebookIncidences("123456789-inserted");

    KCalendarCore::Incidence::List inserted;
    QVERIFY(m_storage->insertedIncidences(&inserted, QDateTime::currentDateTimeUtc().addSecs(-12), "123456789-inserted"));
    QCOMPARE(inserted.length(), 1);
    QCOMPARE(inserted[0]->uid(), event->uid());

    QDateTime recId = event->dtStart().addDays(1);
    KCalendarCore::Incidence::Ptr occurrence = m_calendar->dissociateSingleOccurrence(event, recId);
    QVERIFY(occurrence);
    recId = occurrence->recurrenceId();

    QVERIFY(m_calendar->addEvent(occurrence.staticCast<KCalendarCore::Event>(), "123456789-inserted"));
    m_storage->save();
    reloadDb();
    m_storage->loadNotebookIncidences("123456789-inserted");

    inserted.clear();
    QVERIFY(m_storage->insertedIncidences(&inserted, QDateTime::currentDateTimeUtc().addSecs(-5), "123456789-inserted"));
    QCOMPARE(inserted.length(), 1);
    QCOMPARE(inserted[0]->uid(), event->uid());
    QCOMPARE(inserted[0]->recurrenceId(), recId);

    KCalendarCore::Incidence::List modified;
    QVERIFY(m_storage->modifiedIncidences(&modified, QDateTime::currentDateTimeUtc().addSecs(-5), "123456789-inserted"));
    QCOMPARE(modified.length(), 1);
    QCOMPARE(modified[0]->uid(), event->uid());
    QCOMPARE(modified[0]->recurrenceId(), QDateTime());
}

void tst_storage::tst_deleteAllEvents()
{
    ExtendedCalendar::Ptr cal = ExtendedCalendar::Ptr(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    QVERIFY(cal->addNotebook(QStringLiteral("notebook"), true));
    QVERIFY(cal->setDefaultNotebook(QStringLiteral("notebook")));

    KCalendarCore::Event::Ptr ev = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    ev->setLastModified(QDateTime::currentDateTimeUtc().addSecs(-42));
    ev->setGeoLatitude(42.);
    ev->setGeoLongitude(42.);
    ev->setDtStart(QDateTime(QDate(2019, 10, 10)));
    KCalendarCore::Attendee bob(QStringLiteral("Bob"), QStringLiteral("bob@example.org"));
    ev->addAttendee(bob);

    QVERIFY(cal->addIncidence(ev));
    QCOMPARE(cal->incidences().count(), 1);
    QCOMPARE(cal->rawEventsForDate(ev->dtStart().date()).count(), 1);

    cal->deleteAllIncidences();
    QVERIFY(cal->incidences().isEmpty());
    QVERIFY(cal->rawEventsForDate(ev->dtStart().date()).isEmpty());
}

void tst_storage::tst_alarms()
{
    Notebook::Ptr notebook = Notebook::Ptr(new Notebook(QStringLiteral("Notebook for alarms"), QString()));
    QVERIFY(m_storage->addNotebook(notebook));
    const QString uid = notebook->uid();

    const QDateTime dt = QDateTime::currentDateTimeUtc().addSecs(300);
    KCalendarCore::Event::Ptr ev = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    ev->setDtStart(dt);
    KCalendarCore::Alarm::Ptr alarm = ev->newAlarm();
    alarm->setDisplayAlarm(QLatin1String("Testing alarm"));
    alarm->setStartOffset(KCalendarCore::Duration(0));
    alarm->setEnabled(true);
    QVERIFY(m_calendar->addEvent(ev, uid));
    QVERIFY(m_storage->save());

#if defined(TIMED_SUPPORT)
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["notebook"] = uid;

    Timed::Interface timed;
    QVERIFY(timed.isValid());
    QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 1);
#endif

    QVERIFY(m_calendar->deleteIncidence(ev));
    QVERIFY(m_storage->save());

#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 0);
#endif

    notebook = m_storage->notebook(uid);
    QVERIFY(notebook);
    notebook->setIsVisible(false);
    QVERIFY(m_storage->updateNotebook(notebook));

    // Adding an event in a non visible notebook should not add alarm.
    QVERIFY(m_calendar->addEvent(ev, uid));
    QVERIFY(m_storage->save());
#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 0);
#endif

    // Clearing calendar to be in a situation where the calendar
    // object has just been created.
    m_calendar->close();

    // Switching the notebook to visible should activate all alarms.
    notebook->setIsVisible(true);
    QVERIFY(m_storage->updateNotebook(notebook));
#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 1);
#endif

    // Switching the notebook to non visible should deactivate all alarms.
    notebook->setIsVisible(false);
    QVERIFY(m_storage->updateNotebook(notebook));
#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 0);
#endif
}

void tst_storage::checkAlarms(const QSet<QDateTime> &alarms, const QString &uid) const
{
#if defined(TIMED_SUPPORT)
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["notebook"] = uid;

    Timed::Interface timed;
    QVERIFY(timed.isValid());
    const QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), alarms.size());
    for (const QVariant &cookie : reply.value()) {
        QDBusReply<QMap<QString, QVariant> > attReply = timed.query_attributes_sync(cookie.toUInt());
        QVERIFY(attReply.isValid());
        QMap<QString, QVariant> attMap = attReply.value();
        QVERIFY(attMap.contains(QString::fromLatin1("time")));
        QVERIFY(alarms.contains(attMap.value(QString::fromLatin1("time")).toDateTime()));
    }
#endif
}

void tst_storage::tst_recurringAlarms()
{
    Notebook::Ptr notebook = Notebook::Ptr(new Notebook(QStringLiteral("Notebook for alarms"), QString()));
    QVERIFY(m_storage->addNotebook(notebook));
    const QString uid = notebook->uid();

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime dt = QDateTime(now.date().addDays(1), QTime(12, 00));
    KCalendarCore::Event::Ptr ev = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    ev->setDtStart(dt);
    ev->recurrence()->setDaily(1);
    KCalendarCore::Alarm::Ptr alarm = ev->newAlarm();
    alarm->setDisplayAlarm(QLatin1String("Testing alarm"));
    alarm->setStartOffset(KCalendarCore::Duration(-600));
    alarm->setEnabled(true);
    QVERIFY(m_calendar->addEvent(ev, uid));
    QVERIFY(m_storage->save());

    // Simple recurring event
    checkAlarms(QSet<QDateTime>() << ev->dtStart(), uid);

    KCalendarCore::Incidence::Ptr exception = KCalendarCore::Calendar::createException(ev, ev->dtStart());
    exception->setDtStart(dt.addSecs(300));
    QVERIFY(m_calendar->addEvent(exception.staticCast<KCalendarCore::Event>(), uid));
    KCalendarCore::Incidence::Ptr exception2 = KCalendarCore::Calendar::createException(ev, ev->dtStart().addDays(5));
    exception2->setDtStart(dt.addDays(5).addSecs(300));
    QVERIFY(m_calendar->addEvent(exception2.staticCast<KCalendarCore::Event>(), uid));
    QVERIFY(m_storage->save());

    // Exception on the next occurrence, and second exception on the 5th occurence
    checkAlarms(QSet<QDateTime>() << exception->dtStart()
                << ev->dtStart().addDays(1) << exception2->dtStart(), uid);

    QVERIFY(m_calendar->deleteIncidence(exception));
    QVERIFY(m_calendar->deleteIncidence(exception2));
    QVERIFY(m_storage->save());

    // Exception was deleted
    checkAlarms(QSet<QDateTime>() << ev->dtStart(), uid);

    ev->recurrence()->addExDateTime(ev->dtStart());
    QVERIFY(m_storage->save());

    // exdate added
    checkAlarms(QSet<QDateTime>() << ev->dtStart().addDays(1), uid);

    exception = KCalendarCore::Calendar::createException(ev, ev->dtStart().addDays(1));
    exception->setStatus(KCalendarCore::Incidence::StatusCanceled);
    QVERIFY(m_calendar->addEvent(exception.staticCast<KCalendarCore::Event>(), uid));
    QVERIFY(m_storage->save());

    // Cancelled next occurrence
    checkAlarms(QSet<QDateTime>() << ev->dtStart().addDays(2), uid);

    exception = KCalendarCore::Calendar::createException(ev, ev->dtStart().addDays(4));
    exception->setSummary(QString::fromLatin1("Exception in the future."));
    QVERIFY(m_calendar->addEvent(exception.staticCast<KCalendarCore::Event>(), uid));
    QVERIFY(m_storage->save());

    // Adding an exception later than the next occurrence
    checkAlarms(QSet<QDateTime>() << exception->dtStart() << ev->dtStart().addDays(2), uid);

    notebook = m_storage->notebook(uid);
    QVERIFY(notebook);
    notebook->setIsVisible(false);
    QVERIFY(m_storage->updateNotebook(notebook));

    // Alarms have been removed for non visible notebook.
    checkAlarms(QSet<QDateTime>(), uid);

    notebook = m_storage->notebook(uid);
    QVERIFY(notebook);
    notebook->setIsVisible(true);
    QVERIFY(m_storage->updateNotebook(notebook));

    // Alarms are reset when visible is turned on.
    checkAlarms(QSet<QDateTime>() << exception->dtStart() << ev->dtStart().addDays(2), uid);

    QVERIFY(m_storage->deleteNotebook(m_storage->notebook(uid)));
}

void tst_storage::tst_addIncidence()
{
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2021, 5, 4), QTime(14, 54),
                                Qt::LocalTime));
    event->setSummary("testing generic addIncidence for events.");
    QVERIFY(m_calendar->addIncidence(event, NotebookId));

    auto todo = KCalendarCore::Todo::Ptr(new KCalendarCore::Todo);
    todo->setDtStart(QDateTime(QDate(2021, 5, 4), QTime(14, 55),
                                Qt::LocalTime));
    todo->setSummary("testing generic addIncidence for todos.");
    QVERIFY(m_calendar->addIncidence(todo, NotebookId));

    auto journal = KCalendarCore::Journal::Ptr(new KCalendarCore::Journal);
    journal->setDtStart(QDateTime(QDate(2021, 5, 4), QTime(14, 56),
                                  Qt::LocalTime));
    journal->setSummary("testing generic addIncidence for journals.");
    QVERIFY(m_calendar->addIncidence(journal, NotebookId));

    m_storage->save();
    reloadDb();

    QVERIFY(m_calendar->event(event->uid()));
    QVERIFY(m_calendar->todo(todo->uid()));
    QVERIFY(m_calendar->journal(journal->uid()));
}

void tst_storage::tst_populateFromIcsData()
{
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setSummary("testing fromIcsData.");
    event->setDtStart(QDateTime(QDate(2021, 9, 23), QTime(15, 29)));

    KCalendarCore::ICalFormat icalFormat;
    QVERIFY(icalFormat.fromRawString(m_calendar, icalFormat.toICalString(event).toUtf8()));
    QVERIFY(m_calendar->incidence(event->uid()));
    QVERIFY(m_storage->save());

    reloadDb();
    QVERIFY(m_calendar->incidence(event->uid()));

    event->setRevision(event->revision() + 1);
    QVERIFY(icalFormat.fromRawString(m_calendar, icalFormat.toICalString(event).toUtf8()));
    QVERIFY(m_calendar->incidence(event->uid()));
    QCOMPARE(m_calendar->incidence(event->uid())->revision(), event->revision());
    m_storage->save();

    reloadDb();
    QVERIFY(m_calendar->incidence(event->uid()));
    QCOMPARE(m_calendar->incidence(event->uid())->revision(), event->revision());
}

void tst_storage::openDb(bool clear)
{
    m_calendar = ExtendedCalendar::Ptr(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = m_calendar->defaultStorage(m_calendar);
    m_storage->open();

    mKCal::Notebook::Ptr notebook = m_storage->notebook(NotebookId);

    if (notebook.data() && clear) {
        m_storage->deleteNotebook(notebook);
        notebook.clear();
    }

    if (notebook.isNull()) {
        notebook = mKCal::Notebook::Ptr(new mKCal::Notebook(NotebookId,
                                                            "test notebook",
                                                            QLatin1String(""),
                                                            "#001122",
                                                            false, // Not shared.
                                                            true, // Is master.
                                                            false, // Not synced to Ovi.
                                                            false, // Writable.
                                                            true, // Visible.
                                                            QLatin1String(""),
                                                            QLatin1String(""),
                                                            0));
        m_storage->addNotebook(notebook);
        m_storage->setDefaultNotebook(notebook);
    }

    m_storage->loadNotebookIncidences(NotebookId);
}

void tst_storage::reloadDb()
{
    m_storage.clear();
    m_calendar.clear();
    openDb();
}

void tst_storage::reloadDb(const QDate &from, const QDate &to)
{
    m_storage.clear();
    m_calendar.clear();

    m_calendar = ExtendedCalendar::Ptr(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = m_calendar->defaultStorage(m_calendar);
    m_storage->open();

    m_storage->load(from, to);
}

class TestStorageObserver: public QObject, public ExtendedStorageObserver
{
    Q_OBJECT
public:
    TestStorageObserver(ExtendedStorage::Ptr storage): mStorage(storage)
    {
        mStorage->registerObserver(this);
    }
    ~TestStorageObserver()
    {
        mStorage->unregisterObserver(this);
    }

    void storageModified(ExtendedStorage *storage, const QString &info)
    {
        emit modified();
    }

    void storageUpdated(ExtendedStorage *storage,
                        const KCalendarCore::Incidence::List &added,
                        const KCalendarCore::Incidence::List &modified,
                        const KCalendarCore::Incidence::List &deleted)
    {
        emit updated(added, modified, deleted);
    }

signals:
    void modified();
    void updated(const KCalendarCore::Incidence::List &added,
                 const KCalendarCore::Incidence::List &modified,
                 const KCalendarCore::Incidence::List &deleted);

private:
    ExtendedStorage::Ptr mStorage;
};

Q_DECLARE_METATYPE(KCalendarCore::Incidence::List);
void tst_storage::tst_storageObserver()
{
    TestStorageObserver observer(m_storage);
    QSignalSpy updated(&observer, &TestStorageObserver::updated);
    QSignalSpy modified(&observer, &TestStorageObserver::modified);

    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2023, 1, 13), QTime(16, 35)));
    event->recurrence()->setDaily(2);
    QVERIFY(m_calendar->addIncidence(event));
    KCalendarCore::Incidence::Ptr exception = m_calendar->createException(event, event->dtStart().addDays(4));
    QVERIFY(m_calendar->addIncidence(exception));
    QVERIFY(updated.isEmpty());
    m_storage->save();
    QCOMPARE(updated.count(), 1);
    QList<QVariant> args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    KCalendarCore::Incidence::List added = args[0].value<KCalendarCore::Incidence::List>();
    QCOMPARE(added.count(), 2);
    if (added[0]->recurs()) {
        QCOMPARE(added[0].staticCast<KCalendarCore::Event>(), event);
        QCOMPARE(added[1].staticCast<KCalendarCore::Event>(), exception.staticCast<KCalendarCore::Event>());
    } else {
        QCOMPARE(added[1].staticCast<KCalendarCore::Event>(), event);
        QCOMPARE(added[0].staticCast<KCalendarCore::Event>(), exception.staticCast<KCalendarCore::Event>());
    }
    QVERIFY(args[1].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[2].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200)); // Even after 200ms the modified signal is not emitted.

    event->setDtEnd(event->dtStart().addSecs(3600));
    QVERIFY(updated.isEmpty());
    m_storage->save();
    QCOMPARE(updated.count(), 1);
    args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QVERIFY(args[0].value<KCalendarCore::Incidence::List>().isEmpty());
    QCOMPARE(args[1].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[1].value<KCalendarCore::Incidence::List>()[0].staticCast<KCalendarCore::Event>(), event);
    QVERIFY(args[2].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200));

    QVERIFY(m_calendar->deleteIncidence(event));
    QVERIFY(updated.isEmpty());
    m_storage->save();
    QCOMPARE(updated.count(), 1);
    args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QVERIFY(args[0].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[1].value<KCalendarCore::Incidence::List>().isEmpty());
    KCalendarCore::Incidence::List deleted = args[2].value<KCalendarCore::Incidence::List>();
    QCOMPARE(deleted.count(), 2);
    if (deleted[0]->recurs()) {
        QCOMPARE(deleted[0].staticCast<KCalendarCore::Event>(), event);
        QCOMPARE(deleted[1].staticCast<KCalendarCore::Event>(), exception.staticCast<KCalendarCore::Event>());
    } else {
        QCOMPARE(deleted[1].staticCast<KCalendarCore::Event>(), event);
        QCOMPARE(deleted[0].staticCast<KCalendarCore::Event>(), exception.staticCast<KCalendarCore::Event>());
    }
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200));

    mKCal::Notebook::Ptr notebook(new mKCal::Notebook(QString::fromLatin1("signals"),
                                                      QString()));
    m_storage->addNotebook(notebook);
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200));

    notebook->setDescription(QString::fromLatin1("testing signals"));
    m_storage->updateNotebook(notebook);
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200));

    m_storage->deleteNotebook(notebook);
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200));

    mKCal::ExtendedCalendar::Ptr calendar(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = mKCal::ExtendedCalendar::defaultStorage(calendar);
    QVERIFY(storage->open());
    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setSummary(QString::fromLatin1("New event added externally"));
    event2->setDtStart(QDateTime(QDate(2022, 3, 9), QTime(11, 46)));
    QVERIFY(calendar->addEvent(event2));
    QVERIFY(storage->save());
    QVERIFY(modified.wait());
    QVERIFY(updated.isEmpty());
}

#include "tst_storage.moc"

QTEST_GUILESS_MAIN(tst_storage)
