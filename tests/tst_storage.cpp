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

#include <KCalendarCore/ICalFormat>

#include "tst_storage.h"
#include "dummystorage.h" // Not used, but tests API compilqtion
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
    m_storage->deleteNotebook(notebook);
}

void tst_storage::init()
{
    openDb(true);
}

void tst_storage::cleanup()
{
    mKCal::Notebook::Ptr notebook = m_storage->notebook(NotebookId);
    m_storage->deleteNotebook(notebook);
}

void tst_storage::tst_timezone()
{
    // for test sanity, verify kdatetime actually agrees timezone is for helsinki.
    QDateTime localTime(QDate(2014, 1, 1), QTime(), QTimeZone("Europe/Helsinki"));
    QCOMPARE(localTime.utcOffset(), 7200);
}

Q_DECLARE_METATYPE(QDateTime)
void tst_storage::tst_veventdtstart_data()
{
    QTest::addColumn<QDateTime>("startDateTime");

    QTest::newRow("clock time") << QDateTime(QDate(2020, 5, 29), QTime(10, 15), Qt::LocalTime);
    QTest::newRow("UTC") << QDateTime(QDate(2020, 5, 29), QTime(10, 15), Qt::UTC);
    QTest::newRow("time zone") << QDateTime(QDate(2020, 5, 29), QTime(10, 15), QTimeZone("Europe/Paris"));
    QTest::newRow("date only") << QDateTime(QDate(2020, 5, 29));
    QTest::newRow("origin date time") << m_storage.staticCast<SqliteStorage>()->fromOriginTime(0);
    // Not allowed by RFC, will be converted to origin of time after save.
    QTest::newRow("bogus QDateTime") << QDateTime();
}

void tst_storage::tst_veventdtstart()
{
    QFETCH(QDateTime, startDateTime);

    KCalendarCore::Event::Ptr event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(startDateTime);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb();

    KCalendarCore::Event::Ptr fetchedEvent = m_calendar->event(uid);
    QVERIFY(fetchedEvent.data());
    QVERIFY(fetchedEvent->dtStart().isValid());
    if (startDateTime.isValid()) {
        QCOMPARE(fetchedEvent->dtStart(), startDateTime);
    } else {
        // QDateTime is bogus, invalid date time == January 1st 1970.
        QCOMPARE(fetchedEvent->dtStart(), m_storage.staticCast<SqliteStorage>()->fromOriginTime(0));
    }
    QVERIFY(!fetchedEvent->hasEndDate());
}

void tst_storage::tst_allday_data()
{
    QTest::addColumn<QDate>("startDate");
    QTest::addColumn<int>("days");

    // DST changes according to finnish timezone
    // normal 1 day events
    QTest::newRow("normal") << QDate(2013, 10, 10) << 0;
    QTest::newRow("to non-DST") << QDate(2013, 10, 27) << 0;
    QTest::newRow("to DST") << QDate(2013, 3, 31) << 0;

    // 2 day events
    QTest::newRow("normal, 2d") << QDate(2013, 10, 10) << 1;
    QTest::newRow("to non-DST, 2d") << QDate(2013, 10, 27) << 1;
    QTest::newRow("to DST, 2d") << QDate(2013, 3, 31) << 1;
}

void tst_storage::tst_allday()
{
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);

    QFETCH(QDate, startDate);
    QFETCH(int, days);

    event->setDtStart(QDateTime(startDate, QTime(), Qt::LocalTime));
    event->setAllDay(true);
    if (days) {
        event->setDtEnd(QDateTime(startDate.addDays(days), QTime(), Qt::LocalTime));
    }
    event->setSummary("test event");

    QCOMPARE(event->allDay(), true);
    QCOMPARE(event->dtStart().date(), startDate);

    if (days) {
        QCOMPARE(event->dateEnd(), startDate.addDays(days));
        QCOMPARE(event->hasEndDate(), true);
        QVERIFY(event->dateEnd() > event->dtStart().date());
    } else {
        QCOMPARE(event->dateEnd(), startDate);
        QCOMPARE(event->hasEndDate(), false);
    }

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb(QDate(2012, 1, 1), QDate(2014, 1, 1));

    auto fetchedEvent = m_calendar->event(uid);
    QVERIFY(fetchedEvent.data());
    QCOMPARE(fetchedEvent->allDay(), true);
    QCOMPARE(fetchedEvent->dtStart().date(), startDate);
    QTime time = fetchedEvent->dtStart().time();
    QVERIFY(time == QTime() || time == QTime(0, 0));

    QTime localTime = fetchedEvent->dtStart().toTimeSpec(Qt::LocalTime).time();
    QVERIFY(localTime == QTime() || localTime == QTime(0, 0));

    if (days) {
        QCOMPARE(fetchedEvent->dateEnd(), startDate.addDays(days));
        QCOMPARE(fetchedEvent->hasEndDate(), true);
        QVERIFY(event->dateEnd() > event->dtStart().date());
    } else {
        QCOMPARE(fetchedEvent->dateEnd(), startDate);
        QCOMPARE(fetchedEvent->hasEndDate(), false);
    }
}

void tst_storage::tst_alldayUtc()
{
    // test event saved with UTC time
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    QDate startDate(2013, 12, 1);
    event->setDtStart(QDateTime(startDate, QTime(), Qt::UTC));
    event->setAllDay(true);
    event->setSummary("test event utc");

    QCOMPARE(event->allDay(), true);
    QCOMPARE(event->dtStart().timeSpec(), Qt::UTC);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb();

    auto fetchedEvent = m_calendar->event(uid);
    QVERIFY(fetchedEvent.data());
    QVERIFY(fetchedEvent->allDay());
    QVERIFY(!fetchedEvent->hasEndDate());
    QCOMPARE(fetchedEvent->dtStart().time(), QTime(0, 0, 0));
    QCOMPARE(fetchedEvent->dtStart().date(), startDate);
    QCOMPARE(fetchedEvent->dtEnd().time(), QTime(0, 0, 0));
    QCOMPARE(fetchedEvent->dtEnd().date(), startDate);

    QCOMPARE(fetchedEvent->dtStart().timeSpec(), Qt::LocalTime);
    QCOMPARE(fetchedEvent->dtEnd().timeSpec(), Qt::LocalTime);
}

// Verify that a recurring all day event is kept by storage
void tst_storage::tst_alldayRecurrence()
{
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);

    QDate startDate(2013, 12, 1);
    event->setDtStart(QDateTime(startDate));
    event->setAllDay(true);

    KCalendarCore::Recurrence *recurrence = event->recurrence();
    recurrence->setWeekly(1);
    recurrence->setStartDateTime(event->dtStart(), true);
    recurrence->setAllDay(true);
    recurrence->addRDate(startDate.addDays(2));

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb();

    auto fetchEvent = m_calendar->event(uid);
    QVERIFY(fetchEvent);
    KCalendarCore::Recurrence *fetchRecurrence = fetchEvent->recurrence();
    QVERIFY(fetchRecurrence);
    QCOMPARE(*recurrence, *fetchRecurrence);
    QDateTime match = fetchRecurrence->getNextDateTime(QDateTime(startDate));
    QCOMPARE(match, QDateTime(startDate.addDays(2)));
    match = fetchRecurrence->getNextDateTime(QDateTime(startDate.addDays(3)));
    QCOMPARE(match, QDateTime(startDate.addDays(7), QTime(), Qt::LocalTime));
}

// Verify that a recurrence with an exception rule is properly saved
void tst_storage::tst_recurrence()
{
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);

    QDate startDate(2013, 12, 1);
    QTime startTime(12, 34, 56);
    event->setDtStart(QDateTime(startDate, startTime, Qt::LocalTime));

    KCalendarCore::Recurrence *recurrence = event->recurrence();
    recurrence->setDaily(1);
    KCalendarCore::RecurrenceRule *rrule = new KCalendarCore::RecurrenceRule;
    rrule->setRecurrenceType(KCalendarCore::RecurrenceRule::rWeekly);
    rrule->setDuration(5);
    recurrence->addExRule(rrule);
    recurrence->setStartDateTime(event->dtStart(), false);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb();

    auto fetchEvent = m_calendar->event(uid);
    QVERIFY(fetchEvent);
    KCalendarCore::Recurrence *fetchRecurrence = fetchEvent->recurrence();
    QVERIFY(fetchRecurrence);
    QCOMPARE(*recurrence, *fetchRecurrence);
    QDateTime match = recurrence->getNextDateTime(event->dtStart());
    QCOMPARE(match, event->dtStart().addDays(1));
}

void tst_storage::tst_recurrenceExpansion_data()
{
    QTest::addColumn<QByteArray>("eventTimeZone");
    QTest::addColumn<QByteArray>("expansionTimeZone");
    QTest::addColumn<QString>("intervalEnd");
    QTest::addColumn<QStringList>("expectedEvents");

    // QTest::newRow("created in Brisbane, expanded in ClockTime")
    //     << QByteArray("Australia/Brisbane")
    //     << QByteArray() // ClockTime
    //     << QString("2019-11-18T00:00:00Z")
    //     << (QStringList() << QStringLiteral("2019-11-08T02:00:00+10:00")
    //                       << QStringLiteral("2019-11-11T02:00:00+10:00")
    //                       << QStringLiteral("2019-11-12T02:00:00+10:00")
    //                       << QStringLiteral("2019-11-13T02:00:00+10:00")
    //                       << QStringLiteral("2019-11-14T02:00:00+10:00")
    //                       << QStringLiteral("2019-11-15T02:00:00+10:00")
    //                       << QStringLiteral("2019-11-18T02:00:00+10:00"));

    QTest::newRow("created in ClockTime, expanded in Brisbane")
        << QByteArray() // ClockTime
        << QByteArray("Australia/Brisbane")
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00")
                          << QStringLiteral("2019-11-11T02:00:00")
                          << QStringLiteral("2019-11-12T02:00:00")
                          << QStringLiteral("2019-11-13T02:00:00")
                          << QStringLiteral("2019-11-14T02:00:00")
                          << QStringLiteral("2019-11-15T02:00:00")
                          << QStringLiteral("2019-11-18T02:00:00"));

    QTest::newRow("created in Brisbane, expanded in Brisbane")
        << QByteArray("Australia/Brisbane")
        << QByteArray("Australia/Brisbane")
        << QString("2019-11-18T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+10:00")
                          << QStringLiteral("2019-11-11T02:00:00+10:00")
                          << QStringLiteral("2019-11-12T02:00:00+10:00")
                          << QStringLiteral("2019-11-13T02:00:00+10:00")
                          << QStringLiteral("2019-11-14T02:00:00+10:00")
                          << QStringLiteral("2019-11-15T02:00:00+10:00")
                          << QStringLiteral("2019-11-18T02:00:00+10:00"));

    QTest::newRow("created in Brisbane, expanded in Paris")
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Paris") // up to the end of the 18th in Paris time includes the morning of the 19th in Brisbane time
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+10:00")
                          << QStringLiteral("2019-11-11T02:00:00+10:00")
                          << QStringLiteral("2019-11-12T02:00:00+10:00")
                          << QStringLiteral("2019-11-13T02:00:00+10:00")
                          << QStringLiteral("2019-11-14T02:00:00+10:00")
                          << QStringLiteral("2019-11-15T02:00:00+10:00")
                          << QStringLiteral("2019-11-18T02:00:00+10:00")
                          << QStringLiteral("2019-11-19T02:00:00+10:00"));

    QTest::newRow("created in Paris, expanded in Paris")
        << QByteArray("Europe/Paris")
        << QByteArray("Europe/Paris")
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+01:00")
                          << QStringLiteral("2019-11-11T02:00:00+01:00")
                          << QStringLiteral("2019-11-12T02:00:00+01:00")
                          << QStringLiteral("2019-11-13T02:00:00+01:00")
                          << QStringLiteral("2019-11-14T02:00:00+01:00")
                          << QStringLiteral("2019-11-15T02:00:00+01:00")
                          << QStringLiteral("2019-11-18T02:00:00+01:00"));

    QTest::newRow("created in Paris, expanded in Brisbane")
        << QByteArray("Europe/Paris")
        << QByteArray("Australia/Brisbane")
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+01:00")
                          << QStringLiteral("2019-11-11T02:00:00+01:00")
                          << QStringLiteral("2019-11-12T02:00:00+01:00")
                          << QStringLiteral("2019-11-13T02:00:00+01:00")
                          << QStringLiteral("2019-11-14T02:00:00+01:00")
                          << QStringLiteral("2019-11-15T02:00:00+01:00")
                          << QStringLiteral("2019-11-18T02:00:00+01:00"));

}

// Verify that expansion of recurring event takes timezone into account
void tst_storage::tst_recurrenceExpansion()
{
    QFETCH(QByteArray, eventTimeZone);
    QFETCH(QByteArray, expansionTimeZone);
    QFETCH(QString, intervalEnd);
    QFETCH(QStringList, expectedEvents);

    const QTimeZone eventTZ(eventTimeZone);
    const QTimeZone expTZ(expansionTimeZone);

    // Create an event which occurs every weekday of every week,
    // starting from Friday the 8th of November, from 2 am until 3 am.
    KCalendarCore::Event::Ptr event = KCalendarCore::Event::Ptr(new KCalendarCore::Event());
    event->setUid(QStringLiteral("tst_recurrenceExpansion:%1:%2:%3").arg(eventTimeZone, expansionTimeZone, intervalEnd));
    event->setLocation(QStringLiteral("Test location"));
    event->setAllDay(false);
    event->setDescription(QStringLiteral("Test description"));
    if (eventTZ.isValid()) {
        event->setDtStart(QDateTime(QDate(2019,11,8),
                                    QTime(02,00,00),
                                    eventTZ));
        event->setDtEnd(QDateTime(QDate(2019,11,8),
                                  QTime(03,00,00),
                                  eventTZ));
    } else {
        event->setDtStart(QDateTime(QDate(2019,11,8),
                                    QTime(02,00,00),
                                    Qt::LocalTime));
        event->setDtEnd(QDateTime(QDate(2019,11,8),
                                  QTime(03,00,00),
                                  Qt::LocalTime));
    }
    event->setSummary(QStringLiteral("Test event summary"));
    event->setCategories(QStringList() << QStringLiteral("Category One"));

    KCalendarCore::RecurrenceRule * const rule = new KCalendarCore::RecurrenceRule();
    rule->setRecurrenceType(KCalendarCore::RecurrenceRule::rWeekly);
    rule->setStartDt(event->dtStart());
    rule->setFrequency(1);
    rule->setByDays(QList<KCalendarCore::RecurrenceRule::WDayPos>()
            << KCalendarCore::RecurrenceRule::WDayPos(0, 1)   // monday
            << KCalendarCore::RecurrenceRule::WDayPos(0, 2)   // tuesday
            << KCalendarCore::RecurrenceRule::WDayPos(0, 3)   // wednesday
            << KCalendarCore::RecurrenceRule::WDayPos(0, 4)   // thursday
            << KCalendarCore::RecurrenceRule::WDayPos(0, 5)); // friday

    event->recurrence()->addRRule(rule);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    const QString uid = event->uid();
    reloadDb();

    auto fetchEvent = m_calendar->event(uid);
    QVERIFY(fetchEvent);
    QDateTime match = fetchEvent->recurrence()->getNextDateTime(event->dtStart());
    QCOMPARE(match, event->dtStart().addDays(3)); // skip the weekend

    mKCal::ExtendedCalendar::ExpandedIncidenceList expandedEvents
        = m_calendar->rawExpandedEvents(
            QDate(2019, 11, 05), QDate(2019, 11, 18), // i.e. until the end of the 18th
            false, false, expTZ);

    const KCalendarCore::DateTimeList timesInInterval = event->recurrence()->timesInInterval(
            QDateTime::fromString(QStringLiteral("2019-11-05T00:00:00Z"), Qt::ISODate),
            QDateTime::fromString(intervalEnd, Qt::ISODate));

    QCOMPARE(expandedEvents.size(), expectedEvents.size());
    if (!eventTimeZone.isEmpty()) {
        // timesInInterval() doesn't expand the way we'd like it to,
        // if the event is specified in clock-time, as it performs
        // some conversion to local time via offset addition/subtraction
        // which can result in one extra result being returned.
        QCOMPARE(timesInInterval.size(), expectedEvents.size());
    }
    for (int i = 0; i < expectedEvents.size(); ++i) {
        // We define the expectedEvents in the event time spec,
        // to make it simpler to define the expected values.
        // Thus, we need to convert the actual values into
        // the event time spec prior to comparison.
        if (eventTZ.isValid()) {
            const QDateTime dt = QDateTime::fromString(expectedEvents.at(i), Qt::ISODate);
            QCOMPARE(expandedEvents.at(i).first.dtStart.toTimeZone(eventTZ), dt);
            QCOMPARE(timesInInterval.at(i).toTimeZone(eventTZ), dt);
        } else {
            QDateTime tsExpEvent = expandedEvents.at(i).first.dtStart;
            QDateTime tsTimeInInterval = timesInInterval.at(i);
            tsExpEvent.setTimeSpec(Qt::LocalTime);
            tsTimeInInterval.setTimeSpec(Qt::LocalTime);
            QCOMPARE(tsExpEvent.toString(), expectedEvents.at(i));
            QCOMPARE(tsTimeInInterval.toString(), expectedEvents.at(i));
        }
    }
}

void tst_storage::tst_origintimes()
{
    SqliteStorage *ss = dynamic_cast<SqliteStorage *>(m_storage.data());
    QVERIFY(ss);

    QDateTime utcTime(QDate(2014, 1, 15), QTime(), Qt::UTC);
    QDateTime localTime(QDate(2014, 1, 15), QTime(), Qt::LocalTime);

    // local origin time is the same as specific time set to utc
    // note: currently origin time of clock time is saved as time in current time zone.
    // that does not necessarily make sense, but better be careful when changing behavior there.
    QCOMPARE(ss->toOriginTime(utcTime), ss->toLocalOriginTime(utcTime));
    QCOMPARE(ss->toLocalOriginTime(localTime), ss->toLocalOriginTime(utcTime));
}

void tst_storage::tst_rawEvents_data()
{
    QTest::addColumn<QDate>("date");
    QTest::addColumn<QTime>("startTime");
    QTest::addColumn<QTime>("endTime");
    QTest::addColumn<QByteArray>("timeZone");
    QTest::addColumn<QByteArray>("exceptionTimeZone");
    QTest::addColumn<bool>("rangeCutsOffFirst");
    QTest::addColumn<bool>("secondExceptionApplies");
    QTest::addColumn<bool>("rangeCutsOffLast");

    QTest::newRow("non all day event in clock time with exception in Europe/Helsinki")
        << QDate(2010, 01, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray()
        << QByteArray("Europe/Helsinki")
        << false
        << (QDateTime(QDate(2019, 01, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 01, 03), QTime(00, 00), QTimeZone("Europe/Helsinki"))) == 0)
        << false;

    QTest::newRow("non all day event in clock time with exception in America/Toronto")
        << QDate(2010, 01, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray()
        << QByteArray("America/Toronto")
        << false
        << (QDateTime(QDate(2019, 01, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 01, 03), QTime(00, 00), QTimeZone("America/Toronto"))) == 0)
        << false;

    QTest::newRow("non all day event in Europe/Helsinki with exception in Europe/Helsinki")
        << QDate(2010, 02, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray("Europe/Helsinki")
        << QByteArray("Europe/Helsinki")
        << (QDateTime(QDate(2011, 02, 01), QTime(12, 0), QTimeZone("Europe/Helsinki")).secsTo(
            QDateTime(QDate(2011, 02, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << true // event tz and exception tz are equal
        << (QDateTime(QDate(2011, 02, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 02, 04), QTime(12, 00, 00), QTimeZone("Europe/Helsinki"))) > 0);

    QTest::newRow("non all day event in Europe/Helsinki with exception in America/Toronto")
        << QDate(2010, 02, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray("Europe/Helsinki")
        << QByteArray("America/Toronto")
        << (QDateTime(QDate(2011, 02, 01), QTime(12, 0), QTimeZone("Europe/Helsinki")).secsTo(
            QDateTime(QDate(2011, 02, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false // event tz and exception tz are unequal
        << (QDateTime(QDate(2011, 02, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 02, 04), QTime(12, 00, 00), QTimeZone("Europe/Helsinki"))) > 0);

    QTest::newRow("non all day event in Pacific/Midway with exception in Europe/Helsinki")
        << QDate(2010, 03, 01)
        << QTime(8, 0) << QTime(9, 0)
        << QByteArray("Pacific/Midway")
        << QByteArray("Europe/Helsinki")
        << (QDateTime(QDate(2011, 03, 01), QTime(12, 0), QTimeZone("Pacific/Midway")).secsTo(
            QDateTime(QDate(2011, 03, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 03, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 03, 04), QTime(8, 00, 00), QTimeZone("Pacific/Midway"))) > 0);

    QTest::newRow("non all day event in Pacific/Midway with exception in America/Toronto")
        << QDate(2010, 03, 01)
        << QTime(8, 0) << QTime(9, 0)
        << QByteArray("Pacific/Midway")
        << QByteArray("America/Toronto")
        << (QDateTime(QDate(2011, 03, 01), QTime(12, 0), QTimeZone("Pacific/Midway")).secsTo(
            QDateTime(QDate(2011, 03, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 03, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 03, 04), QTime(8, 00, 00), QTimeZone("Pacific/Midway"))) > 0);

    QTest::newRow("all day event stored as local clock with exception in Europe/Helsinki")
        << QDate(2010, 04, 01)
        << QTime(0, 0) << QTime()
        << QByteArray()
        << QByteArray("Europe/Helsinki")
        << false
        << (QDateTime(QDate(2019, 04, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 04, 03), QTime(00, 00), QTimeZone("Europe/Helsinki"))) == 0)
        << false;

    QTest::newRow("all day event stored as local clock with exception in America/Toronto")
        << QDate(2010, 04, 01)
        << QTime(0, 0) << QTime()
        << QByteArray()
        << QByteArray("America/Toronto")
        << false
        << (QDateTime(QDate(2019, 04, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 04, 03), QTime(00, 00), QTimeZone("America/Toronto"))) == 0)
        << false;

    QTest::newRow("all day event stored as date only with exception in Europe/Helsinki")
        << QDate(2010, 05, 01)
        << QTime() << QTime()
        << QByteArray()
        << QByteArray("Europe/Helsinki")
        << false
        << (QDateTime(QDate(2019, 05, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 05, 03), QTime(00, 00), QTimeZone("Europe/Helsinki"))) == 0)
        << false;

    QTest::newRow("all day event stored as date only with exception in America/Toronto")
        << QDate(2010, 05, 01)
        << QTime() << QTime()
        << QByteArray()
        << QByteArray("America/Toronto")
        << false
        << (QDateTime(QDate(2019, 05, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 05, 03), QTime(00, 00), QTimeZone("America/Toronto"))) == 0)
        << false;

    QTest::newRow("non all day event in clock time with exception in Australia/Brisbane")
        << QDate(2011, 06, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray()
        << QByteArray("Australia/Brisbane")
        << false
        << (QDateTime(QDate(2019, 06, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 06, 03), QTime(00, 00), QTimeZone("Australia/Brisbane"))) == 0)
        << false;

    QTest::newRow("non all day event in Europe/Helsinki with exception in Australia/Brisbane")
        << QDate(2011, 06, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray("Europe/Helsinki")
        << QByteArray("Australia/Brisbane")
        << (QDateTime(QDate(2011, 06, 01), QTime(12, 0), QTimeZone("Europe/Helsinki")).secsTo(
            QDateTime(QDate(2011, 06, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 06, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 06, 04), QTime(12, 00, 00), QTimeZone("Europe/Helsinki"))) > 0);

    QTest::newRow("non all day event in Pacific/Midway with exception in Australia/Brisbane")
        << QDate(2011, 06, 01)
        << QTime(8, 0) << QTime(9, 0)
        << QByteArray("Pacific/Midway")
        << QByteArray("Australia/Brisbane")
        << (QDateTime(QDate(2011, 06, 01), QTime(12, 0), QTimeZone("Pacific/Midway")).secsTo(
            QDateTime(QDate(2011, 06, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 06, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 06, 04), QTime(8, 00, 00), QTimeZone("Pacific/Midway"))) > 0);

    QTest::newRow("all day event stored as local clock with exception in Australia/Brisbane")
        << QDate(2011, 07, 01)
        << QTime(0, 0) << QTime()
        << QByteArray()
        << QByteArray("Australia/Brisbane")
        << false
        << (QDateTime(QDate(2019, 07, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 07, 03), QTime(00, 00), QTimeZone("Australia/Brisbane"))) == 0)
        << false;

    QTest::newRow("all day event stored as date only with exception in Australia/Brisbane")
        << QDate(2011, 07, 01)
        << QTime() << QTime()
        << QByteArray()
        << QByteArray("Australia/Brisbane")
        << false
        << (QDateTime(QDate(2019, 07, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 07, 03), QTime(00, 00), QTimeZone("Australia/Brisbane"))) == 0)
        << false;

    QTest::newRow("non all day event in America/Toronto with exception in Australia/Brisbane")
        << QDate(2011, 8, 1)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray("America/Toronto")
        << QByteArray("Australia/Brisbane")
        << (QDateTime(QDate(2011, 8, 1), QTime(12, 0), QTimeZone("America/Toronto")).secsTo(
            QDateTime(QDate(2011, 8, 1), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 8, 4), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 8, 4), QTime(12, 00, 00), QTimeZone("America/Toronto"))) > 0);

    QTest::newRow("non all day event in America/Toronto with exception in America/Toronto")
        << QDate(2011, 8, 1)
        << QTime(12, 0) << QTime(13, 0)
        << QByteArray("America/Toronto")
        << QByteArray("America/Toronto")
        << (QDateTime(QDate(2011, 8, 1), QTime(12, 0), QTimeZone("America/Toronto")).secsTo(
            QDateTime(QDate(2011, 8, 1), QTime(00, 0), Qt::LocalTime)) > 0)
        << true
        << (QDateTime(QDate(2011, 8, 4), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 8, 4), QTime(12, 00, 00), QTimeZone("America/Toronto"))) > 0);
}

// NOTE: to adequately test the functionality,
//       this test MUST be run in a variety of timezones,
//       e.g. Pacific/Midway, America/Toronto, France/Paris,
//            Europe/Helsinki, Australia/Brisbane, Australia/Sydney.
void tst_storage::tst_rawEvents()
{
    QFETCH(QDate, date);
    QFETCH(QTime, startTime);
    QFETCH(QTime, endTime);
    QFETCH(QByteArray, timeZone);
    QFETCH(QByteArray, exceptionTimeZone);
    QFETCH(bool, rangeCutsOffFirst);
    QFETCH(bool, secondExceptionApplies);
    QFETCH(bool, rangeCutsOffLast);

    const QTimeZone spec(timeZone);
    const QTimeZone exceptionSpec(exceptionTimeZone);
    const QTimeZone expansionSpec = QTimeZone::systemTimeZone();

    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    if (startTime.isValid()) {
        if (spec.isValid()) {
            event->setDtStart(QDateTime(date, startTime, spec));
        } else {
            event->setDtStart(QDateTime(date, startTime, Qt::LocalTime));
        }
        if (endTime.isValid() && spec.isValid()) {
            event->setDtEnd(QDateTime(date, endTime, spec));
        } else if (endTime.isValid()) {
            event->setDtEnd(QDateTime(date, endTime, Qt::LocalTime));
        } else if (startTime == QTime(0, 0)) {
            event->setAllDay(true);
        }
    } else {
        event->setDtStart(QDateTime(date));
        event->setAllDay(true);
    }
    event->setSummary(QStringLiteral("testing rawExpandedEvents()"));

    KCalendarCore::Recurrence *recurrence = event->recurrence();
    recurrence->setDaily(1);
    recurrence->setStartDateTime(event->dtStart(), event->allDay());
    recurrence->setDuration(5);
    recurrence->setAllDay(event->allDay());
    if (event->allDay()) {
        // Save exception as clock time
        recurrence->addExDateTime(QDateTime(event->dtStart().date().addDays(1), QTime(0,0), Qt::LocalTime));
        // Save exception in exception time zone
        recurrence->addExDateTime(QDateTime(event->dtStart().date().addDays(2), QTime(0,0), exceptionSpec));
    } else {
        // Register an exception in spec of the event
        recurrence->addExDateTime(event->dtStart().addDays(1));
        // Register an exception in exception time zone
        recurrence->addExDateTime(QDateTime(event->dtStart().date().addDays(2), event->dtStart().time(), exceptionSpec));
    }

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb();

    auto fetchEvent = m_calendar->event(uid);
    QVERIFY(fetchEvent);
    QCOMPARE(fetchEvent->allDay(), event->allDay());
    KCalendarCore::Recurrence *fetchRecurrence = fetchEvent->recurrence();
    QVERIFY(fetchRecurrence);
    QCOMPARE(fetchRecurrence->allDay(), recurrence->allDay());

    // should return occurrence for expected days and omit exceptions
    mKCal::ExtendedCalendar::ExpandedIncidenceList events
        = m_calendar->rawExpandedEvents(date, date.addDays(3), false, false, expansionSpec);

    // note that if the range cuts off the first event, we expect an "extra" recurrence at the end to make up for it.
    QCOMPARE(events.size(), secondExceptionApplies && rangeCutsOffLast ? 1 : ((secondExceptionApplies || rangeCutsOffLast) ? 2 : 3));

    if (!rangeCutsOffFirst) {
        int currIndex = 0;
        QCOMPARE(events[currIndex].first.dtStart, event->dtStart().toTimeZone(expansionSpec));
        QCOMPARE(events[currIndex].first.dtEnd, event->dtEnd().toTimeZone(expansionSpec));

        if (!secondExceptionApplies) {
            currIndex++;
            QCOMPARE(events[currIndex].first.dtStart, event->dtStart().addDays(2).toTimeZone(expansionSpec));
            QCOMPARE(events[currIndex].first.dtEnd, event->dtEnd().addDays(2).toTimeZone(expansionSpec));
        }

        if (!rangeCutsOffLast) {
            currIndex++;
            QCOMPARE(events[currIndex].first.dtStart, event->dtStart().addDays(3).toTimeZone(expansionSpec));
            QCOMPARE(events[currIndex].first.dtEnd, event->dtEnd().addDays(3).toTimeZone(expansionSpec));
        }
    } else {
        int currIndex = 0;
        if (!secondExceptionApplies) {
            QCOMPARE(events[currIndex].first.dtStart, event->dtStart().addDays(2).toTimeZone(expansionSpec));
            QCOMPARE(events[currIndex].first.dtEnd, event->dtEnd().addDays(2).toTimeZone(expansionSpec));
            currIndex++;
        }

        // if the range cuts off the first, it cannot cut off the last.
        // indeed, we should expect an EXTRA event, which squeezes into the range when converted to local time.
        QCOMPARE(rangeCutsOffLast, false);
        QCOMPARE(events[currIndex].first.dtStart, event->dtStart().addDays(3).toTimeZone(expansionSpec));
        QCOMPARE(events[currIndex].first.dtEnd, event->dtEnd().addDays(3).toTimeZone(expansionSpec));
        currIndex++;
        QCOMPARE(events[currIndex].first.dtStart, event->dtStart().addDays(4).toTimeZone(expansionSpec));
        QCOMPARE(events[currIndex].first.dtEnd, event->dtEnd().addDays(4).toTimeZone(expansionSpec));
    }
}

void tst_storage::tst_rawEvents_nonRecur_data()
{
    QTest::addColumn<QDate>("startDate");
    QTest::addColumn<QTime>("startTime");
    QTest::addColumn<QDate>("endDate");
    QTest::addColumn<QTime>("endTime");
    QTest::addColumn<QByteArray>("timeZone");
    QTest::addColumn<QByteArray>("expansionTimeZone");
    QTest::addColumn<QDate>("rangeStartDate");
    QTest::addColumn<QDate>("rangeEndDate");
    QTest::addColumn<bool>("expectFound");

    QTest::newRow("single day event in clock time expanded in Europe/Helsinki, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 01)
        << QTime(16, 30)
        << QByteArray()
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 02)
        << QDate(2019, 07, 03)
        << false;

    QTest::newRow("single day event in clock time expanded in Europe/Helsinki, found")
        << QDate(2019, 07, 01)
        << QTime(12, 0)
        << QDate(2019, 07, 01)
        << QTime(20, 0)
        << QByteArray()
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 01)
        << QDate(2019, 07, 03)
        << true;

    QTest::newRow("single day event in Europe/Helsinki expanded in clock time, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 01)
        << QTime(16, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray()
        << QDate(2019, 07, 02)
        << QDate(2019, 07, 03)
        << false;

    QTest::newRow("single day event in Europe/Helsinki expanded in clock time, found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 01)
        << QTime(16, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray()
        << QDate(2019, 07, 01)
        << QDate(2019, 07, 03)
        << true;

    QTest::newRow("single day event in Australia/Brisbane expanded in Europe/Helsinki, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 01)
        << QTime(16, 30)
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 02)
        << QDate(2019, 07, 03)
        << false;

    QTest::newRow("single day event in Australia/Brisbane expanded in Europe/Helsinki, not found 2")
        << QDate(2019, 07, 01)
        << QTime(5, 0)
        << QDate(2019, 07, 01)
        << QTime(6, 0)
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 01)
        << QDate(2019, 07, 03)
        << false; // (dtEnd 2019-07-01T06:00:00+10:00 == 1561924800) < (rangeStart 2019-07-01T00:00:00+02:00 == 1561932000)

    QTest::newRow("single day event in Australia/Brisbane expanded in Europe/Helsinki, found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 01)
        << QTime(16, 30)
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 01)
        << QDate(2019, 07, 03)
        << true;

    QTest::newRow("single day event in Europe/Helsinki expanded in Australia/Brisbane, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 01)
        << QTime(16, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray("Australia/Brisbane")
        << QDate(2019, 07, 02)
        << QDate(2019, 07, 03)
        << false;

    QTest::newRow("single day event in Europe/Helsinki expanded in Australia/Brisbane, found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 01)
        << QTime(20, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray("Australia/Brisbane")
        << QDate(2019, 07, 01)
        << QDate(2019, 07, 03)
        << true;

    QTest::newRow("multi day event in clock time expanded in Europe/Helsinki, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray()
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 04)
        << QDate(2019, 07, 05)
        << false;

    QTest::newRow("multi day event in clock time expanded in Europe/Helsinki, found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray()
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 03)
        << QDate(2019, 07, 05)
        << true;

    QTest::newRow("multi day event in Europe/Helsinki expanded in clock time, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray()
        << QDate(2019, 07, 04)
        << QDate(2019, 07, 05)
        << false;

    QTest::newRow("multi day event in Europe/Helsinki expanded in clock time, found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray()
        << QDate(2019, 07, 03)
        << QDate(2019, 07, 05)
        << true;

    QTest::newRow("multi day event in Australia/Brisbane expanded in Europe/Helsinki, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 04)
        << QDate(2019, 07, 05)
        << false;

    QTest::newRow("multi day event in Australia/Brisbane expanded in Europe/Helsinki, not found 2")
        << QDate(2019, 07, 03)
        << QTime(9, 0)
        << QDate(2019, 07, 05)
        << QTime(23, 0)
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 01)
        << QDate(2019, 07, 02)
        << false;

    QTest::newRow("multi day event in Australia/Brisbane expanded in Europe/Helsinki, found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 03)
        << QDate(2019, 07, 05)
        << true;

    QTest::newRow("multi day event in Australia/Brisbane expanded in Europe/Helsinki, found 2")
        << QDate(2019, 07, 03)
        << QTime(6, 0) // 2019-07-03T06:00:00+10:00 --> 2019-07-02T22:00:00+02:00, so in range (and 23:00 in DST)
        << QDate(2019, 07, 05)
        << QTime(23, 0)
        << QByteArray("Australia/Brisbane")
        << QByteArray("Europe/Helsinki")
        << QDate(2019, 07, 01)
        << QDate(2019, 07, 02)
        << true;

    QTest::newRow("multi day event in Europe/Helsinki expanded in Australia/Brisbane, not found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray("Australia/Brisbane")
        << QDate(2019, 07, 04)
        << QDate(2019, 07, 05)
        << false;

    QTest::newRow("multi day event in Europe/Helsinki expanded in Australia/Brisbane, found")
        << QDate(2019, 07, 01)
        << QTime(15, 0)
        << QDate(2019, 07, 03)
        << QTime(16, 30)
        << QByteArray("Europe/Helsinki")
        << QByteArray("Australia/Brisbane")
        << QDate(2019, 07, 03)
        << QDate(2019, 07, 04)
        << true;
}

void tst_storage::tst_rawEvents_nonRecur()
{
    QFETCH(QDate, startDate);
    QFETCH(QTime, startTime);
    QFETCH(QDate, endDate);
    QFETCH(QTime, endTime);
    QFETCH(QByteArray, timeZone);
    QFETCH(QByteArray, expansionTimeZone);
    QFETCH(QDate, rangeStartDate);
    QFETCH(QDate, rangeEndDate);
    QFETCH(bool, expectFound);

    static int count = 0;
    const QString eventUid = QStringLiteral("tst_rawEvents_nonRecur:%1in%2=%3-%5")
                                       .arg(timeZone.isEmpty() ? QStringLiteral("clocktime") : timeZone)
                                       .arg(expansionTimeZone.isEmpty() ? QStringLiteral("clocktime") : expansionTimeZone)
                                       .arg(expectFound)
                                       .arg(++count);

    const QTimeZone spec(timeZone);
    QTimeZone rangeSpec(expansionTimeZone);
    if (!rangeSpec.isValid()) {
        rangeSpec = QTimeZone::systemTimeZone();
    }

    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    if (spec.isValid()) {
        event->setDtStart(QDateTime(startDate, startTime, spec));
        event->setDtEnd(QDateTime(endDate, endTime, spec));
    } else {
        event->setDtStart(QDateTime(startDate, startTime, Qt::LocalTime));
        event->setDtEnd(QDateTime(endDate, endTime, Qt::LocalTime));
    }
    event->setSummary(QStringLiteral("testing rawExpandedEvents, non-recurring: %2").arg(eventUid));
    event->setUid(eventUid);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QString uid = event->uid();
    reloadDb();

    auto fetchEvent = m_calendar->event(uid);
    QVERIFY(fetchEvent);
    if (spec.isValid()) {
        QCOMPARE(fetchEvent->dtStart(), QDateTime(startDate, startTime, spec));
        QCOMPARE(fetchEvent->dtEnd(), QDateTime(endDate, endTime, spec));
    } else {
        QCOMPARE(fetchEvent->dtStart(), QDateTime(startDate, startTime, Qt::LocalTime));
        QCOMPARE(fetchEvent->dtEnd(), QDateTime(endDate, endTime, Qt::LocalTime));
    }

    mKCal::ExtendedCalendar::ExpandedIncidenceList events
        = m_calendar->rawExpandedEvents(rangeStartDate, rangeEndDate, false, false, rangeSpec);

    QCOMPARE(events.size(), expectFound ? 1 : 0);
    if (expectFound) {
        QCOMPARE(events[0].second->summary(), event->summary());
        QCOMPARE(events[0].first.dtStart, event->dtStart().toTimeZone(rangeSpec));
        QCOMPARE(events[0].first.dtEnd, event->dtEnd().toTimeZone(rangeSpec));
    }
}

// Check that the creation date can be tuned and restored properly.
void tst_storage::tst_dateCreated_data()
{
    QTest::addColumn<QDateTime>("dateCreated");
    QTest::addColumn<QDateTime>("dateCreated_update");

    QTest::newRow("insert new event without creation date")
        << QDateTime()
        << QDateTime();
    QTest::newRow("insert new event with creation date")
        << QDateTime::fromString("2019-04-01T10:21:15+02:00", Qt::ISODate)
        << QDateTime();
    QTest::newRow("update new event without creation date")
        << QDateTime()
        << QDateTime::fromString("2019-04-01T10:21:15+02:00", Qt::ISODate);
    QTest::newRow("update new event with creation date")
        << QDateTime::fromString("2019-04-01T10:21:15+02:00", Qt::ISODate)
        << QDateTime::fromString("2020-04-01T10:21:15+02:00", Qt::ISODate);
}

void tst_storage::tst_dateCreated()
{
    QFETCH(QDateTime, dateCreated);
    QFETCH(QDateTime, dateCreated_update);

    // Verify that date craetion date can be tuned on new insertion and on update.
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2019, 04, 01), QTime(10, 11),
                                Qt::LocalTime));
    event->setSummary("Creation date test event");
    event->setCreated(dateCreated.toUTC());

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    reloadDb();

    auto fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    if (dateCreated.isNull()) {
        QVERIFY(fetchEvent->created().secsTo(QDateTime::currentDateTimeUtc()) <= 1);
    } else {
        QCOMPARE(fetchEvent->created(), dateCreated);
    }

    if (!dateCreated_update.isNull()) {
        fetchEvent->setCreated(dateCreated_update.toUTC());
        fetchEvent->updated();
        m_storage->save();
        reloadDb();

        fetchEvent = m_calendar->event(event->uid());
        QVERIFY(fetchEvent);
        QCOMPARE(fetchEvent->created(), dateCreated_update);
    }
}

// Check that lastModified field is not modified by storage,
// but actually updated whenever a modification is done to a stored incidence.
void tst_storage::tst_lastModified()
{
    QDateTime dt(QDate(2019, 07, 26), QTime(11, 41), Qt::LocalTime);
    KCalendarCore::Event::Ptr event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(dt.addDays(1));
    event->setSummary("Modified date test event");
    event->setLastModified(dt);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    QCOMPARE(event->lastModified(), dt);

    reloadDb();
    auto fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    QCOMPARE(fetchEvent->lastModified(), dt);

    fetchEvent->setDtStart(dt.addDays(2));
    QVERIFY(fetchEvent->lastModified().secsTo(QDateTime::currentDateTimeUtc()) <= 1);
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
    if (event->allDay()) {
        QCOMPARE(recurrence->exDates().length(), 1);
        QCOMPARE(recurrence->exDates()[0], recId.date());
    } else {
        QCOMPARE(recurrence->exDateTimes().length(), 1);
        QCOMPARE(recurrence->exDateTimes()[0], recId);
    }
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
    KCalendarCore::Recurrence *fetchRecurrence = event->recurrence();
    if (event->allDay()) {
        QCOMPARE(fetchRecurrence->exDates().length(), 1);
        QCOMPARE(fetchRecurrence->exDates()[0], recId.date());
    } else {
        QCOMPARE(fetchRecurrence->exDateTimes().length(), 1);
        QCOMPARE(fetchRecurrence->exDateTimes()[0], recId);
    }

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
    reloadDb();
    QVERIFY(m_storage->loadNotebookIncidences(notebook->uid()));

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

    // One can purge previously deleted events from DB
    QVERIFY(m_storage->purgeDeletedIncidences(deleted));
    deleted.clear();
    QVERIFY(m_storage->deletedIncidences(&deleted, QDateTime::currentDateTimeUtc().addSecs(-2), notebook->uid()));
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

// Test various way of describing all day events in iCal format.
void tst_storage::tst_icalAllDay_data()
{
    QTest::addColumn<QString>("uid");
    QTest::addColumn<QString>("vEvent");
    QTest::addColumn<bool>("allDay");

    QTest::newRow("local time")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A70")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART:20190607T000000\n"
                          "DTEND:20190607T000000\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A70\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT")
        << true;
    QTest::newRow("UTC")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A71")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART:20190607T000000Z\n"
                          "DTEND:20190607T000000Z\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A71\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT")
        << false;
    QString zid = QString::fromLatin1(QTimeZone::systemTimeZoneId());
    QTest::newRow("system time zone")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A72")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART;TZID=%1:20190607T000000\n"
                          "DTEND;TZID=%1:20190607T000000\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A72\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT").arg(zid)
        << false; // TODO: FIXME: MR#17 addresses this issue.
    QTest::newRow("floating date")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A73")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART:20190607\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A73\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT")
        << true;
}

void tst_storage::tst_icalAllDay()
{
    QFETCH(QString, uid);
    QFETCH(QString, vEvent);
    QFETCH(bool, allDay);

    const QString icsData =
        QStringLiteral("BEGIN:VCALENDAR\n"
                       "PRODID:-//NemoMobile.org/Nemo//NONSGML v1.0//EN\n"
                       "VERSION:2.0\n") + vEvent +
        QStringLiteral("\nEND:VCALENDAR");
    KCalendarCore::ICalFormat format;
    QVERIFY(format.fromString(m_calendar, icsData));
    KCalendarCore::Event::Ptr event = m_calendar->event(uid);
    QVERIFY(event);

    m_storage->save();
    reloadDb();

    KCalendarCore::Event::Ptr fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    QCOMPARE(fetchEvent->allDay(), allDay);
    QCOMPARE(event->dtStart(), fetchEvent->dtStart());
    QCOMPARE(event->dtEnd(), fetchEvent->dtEnd());
}

void tst_storage::tst_deleteAllEvents()
{
    ExtendedCalendar::Ptr cal = ExtendedCalendar::Ptr(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    QVERIFY(cal->addNotebook(QStringLiteral("notebook"), true));
    QVERIFY(cal->setDefaultNotebook(QStringLiteral("notebook")));

    KCalendarCore::Event::Ptr ev = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    ev->setLastModified(QDateTime::currentDateTimeUtc().addSecs(-42));
    ev->setHasGeo(true);
    ev->setGeoLatitude(42.);
    ev->setGeoLongitude(42.);
    ev->setDtStart(QDateTime(QDate(2019, 10, 10)));
    KCalendarCore::Attendee bob(QStringLiteral("Bob"), QStringLiteral("bob@example.org"));
    ev->addAttendee(bob);

    QVERIFY(cal->addIncidence(ev));
    QCOMPARE(cal->incidences().count(), 1);
    QCOMPARE(cal->geoIncidences().count(), 1);
    QCOMPARE(cal->attendees().count(), 1);
    QCOMPARE(cal->rawEventsForDate(ev->dtStart().date()).count(), 1);

    cal->deleteAllIncidences();
    QVERIFY(cal->incidences().isEmpty());
    QVERIFY(cal->geoIncidences().isEmpty());
    QVERIFY(cal->attendees().isEmpty());
    QVERIFY(cal->rawEventsForDate(ev->dtStart().date()).isEmpty());
}

void tst_storage::tst_calendarProperties()
{
    Notebook::Ptr notebook = Notebook::Ptr(new Notebook(QStringLiteral("Notebook"), QString()));

    QCOMPARE(notebook->customPropertyKeys().count(), 0);
    const QByteArray propKey("a key");
    const QString propValue = QStringLiteral("a value");
    notebook->setCustomProperty(propKey, propValue);
    QCOMPARE(notebook->customPropertyKeys().count(), 1);
    QCOMPARE(notebook->customProperty(propKey), propValue);

    QVERIFY(m_storage->addNotebook(notebook));
    QString uid = notebook->uid();

    reloadDb();
    notebook = m_storage->notebook(uid);
    QVERIFY(notebook);
    QCOMPARE(notebook->customPropertyKeys().count(), 1);
    QCOMPARE(notebook->customProperty(propKey), propValue);

    const QByteArray propKey2("a second key");
    const QString propValue2 = QStringLiteral("another value");
    notebook->setCustomProperty(propKey2, propValue2);
    QCOMPARE(notebook->customPropertyKeys().count(), 2);
    QCOMPARE(notebook->customProperty(propKey2), propValue2);

    QVERIFY(m_storage->updateNotebook(notebook));

    reloadDb();
    notebook = m_storage->notebook(uid);
    QVERIFY(notebook);
    QCOMPARE(notebook->customPropertyKeys().count(), 2);
    QCOMPARE(notebook->customProperty(propKey), propValue);
    QCOMPARE(notebook->customProperty(propKey2), propValue2);

    notebook->setCustomProperty(propKey2, QString());
    QCOMPARE(notebook->customPropertyKeys().count(), 1);
    QCOMPARE(notebook->customProperty(propKey), propValue);
    QCOMPARE(notebook->customProperty(propKey2), QString());
    QString defaultValue = QStringLiteral("default value");
    QCOMPARE(notebook->customProperty(propKey2, defaultValue), defaultValue);

    QVERIFY(m_storage->updateNotebook(notebook));

    reloadDb();
    notebook = m_storage->notebook(uid);
    QVERIFY(notebook);
    QCOMPARE(notebook->customPropertyKeys().count(), 1);
    QCOMPARE(notebook->customProperty(propKey), propValue);
    QCOMPARE(notebook->customProperty(propKey2), QString());

    m_storage->deleteNotebook(notebook);

    // Need to check by hand that property entries have been deleted.
    int rv;
    sqlite3 *database;
    rv = sqlite3_open(m_storage.staticCast<SqliteStorage>()->databaseName().toUtf8(), &database);
    QCOMPARE(rv, 0);
    const char *query = SELECT_CALENDARPROPERTIES_BY_ID;
    int qsize = sizeof(SELECT_CALENDARPROPERTIES_BY_ID);
    sqlite3_stmt *stmt = NULL;
#undef sqlite3_prepare_v2
    rv = sqlite3_prepare_v2(database, query, qsize, &stmt, NULL);
    QCOMPARE(rv, 0);
    const QByteArray id(uid.toUtf8());
#undef sqlite3_bind_text
    rv = sqlite3_bind_text(stmt, 1, id.constData(), id.length(), SQLITE_STATIC);
    QCOMPARE(rv, 0);
#undef sqlite3_step
    rv = sqlite3_step(stmt);
    QCOMPARE(rv, SQLITE_DONE);
    sqlite3_close(database);
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

void tst_storage::tst_load()
{
    mKCal::Notebook::Ptr notebook =
        mKCal::Notebook::Ptr(new mKCal::Notebook("123456789-load",
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
    event->setSummary("Deleted event");
    event->setCreated(QDateTime::currentDateTimeUtc().addSecs(-3));
    event->recurrence()->setDaily(1);
    KCalendarCore::Event::Ptr occurrence(event->clone());
    occurrence->clearRecurrence();
    occurrence->setDtStart(event->dtStart().addDays(1));
    QDateTime recId = event->dtStart().addDays(1);
    recId.setTime(QTime(recId.time().hour(), recId.time().minute(), recId.time().second()));
    occurrence->setRecurrenceId(recId);
    occurrence->setSummary("Deleted occurrence");
    event->recurrence()->addExDateTime(occurrence->recurrenceId());

    QVERIFY(m_calendar->addEvent(event, notebook->uid()));
    QVERIFY(m_calendar->addEvent(occurrence, notebook->uid()));
    QVERIFY(m_storage->save());

    m_storage.clear();
    m_calendar.clear();
    m_calendar = ExtendedCalendar::Ptr(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = m_calendar->defaultStorage(m_calendar);
    m_storage->open();

    QVERIFY(m_calendar->events().isEmpty());

    QVERIFY(m_storage->load(occurrence->uid(), occurrence->recurrenceId()));
    QCOMPARE(m_calendar->events().length(), 1);
    QVERIFY(m_calendar->deleteIncidence(m_calendar->incidence(occurrence->uid(), occurrence->recurrenceId())));
    QVERIFY(m_calendar->events().isEmpty());
    QVERIFY(m_storage->load(occurrence->uid(), occurrence->recurrenceId()));
    QVERIFY(m_calendar->events().isEmpty());

    QVERIFY(m_storage->load(event->uid()));
    QCOMPARE(m_calendar->events().length(), 1);
    QVERIFY(m_calendar->deleteIncidence(m_calendar->incidence(event->uid())));
}

void tst_storage::tst_loadSeries()
{
    mKCal::Notebook::Ptr notebook =
        mKCal::Notebook::Ptr(new mKCal::Notebook("123456789-loadSeries",
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
    event->setSummary("Parent event");
    event->setCreated(QDateTime::currentDateTimeUtc().addSecs(-3));
    event->recurrence()->setDaily(1);
    KCalendarCore::Event::Ptr occurrence(event->clone());
    occurrence->clearRecurrence();
    occurrence->setDtStart(event->dtStart().addDays(1));
    QDateTime recId = event->dtStart().addDays(1);
    recId.setTime(QTime(recId.time().hour(), recId.time().minute(), recId.time().second()));
    occurrence->setRecurrenceId(recId);
    occurrence->setSummary("Exception occurrence");
    event->recurrence()->addExDateTime(occurrence->recurrenceId());
    KCalendarCore::Event::Ptr single = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    single->setDtStart(QDateTime::currentDateTimeUtc().addDays(2));
    single->setSummary("Single event");

    QVERIFY(m_calendar->addEvent(event, notebook->uid()));
    QVERIFY(m_calendar->addEvent(occurrence, notebook->uid()));
    QVERIFY(m_calendar->addEvent(single, notebook->uid()));
    QVERIFY(m_storage->save());

    m_storage.clear();
    m_calendar.clear();
    m_calendar = ExtendedCalendar::Ptr(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = m_calendar->defaultStorage(m_calendar);
    m_storage->open();

    QVERIFY(m_calendar->events().isEmpty());

    QVERIFY(m_storage->loadSeries(event->uid()));
    QCOMPARE(m_calendar->events().length(), 2);
    QVERIFY(m_calendar->incidence(event->uid()));
    QVERIFY(m_calendar->incidence(occurrence->uid(), occurrence->recurrenceId()));

    QVERIFY(m_storage->loadSeries(single->uid()));
    QCOMPARE(m_calendar->events().length(), 3);
    QVERIFY(m_calendar->incidence(single->uid()));
}

void tst_storage::tst_loadIncidenceInstance()
{
    mKCal::Notebook::Ptr notebook =
        mKCal::Notebook::Ptr(new mKCal::Notebook("123456789-loadIncidenceInstance",
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

    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2021, 4, 26), QTime(16, 49), Qt::UTC));
    event->setSummary("Parent event");
    event->setCreated(event->dtStart().addSecs(-3));
    event->recurrence()->setDaily(1);
    KCalendarCore::Event::Ptr occurrence(event->clone());
    occurrence->clearRecurrence();
    occurrence->setDtStart(event->dtStart().addDays(1).addSecs(3600));
    occurrence->setRecurrenceId(event->dtStart().addDays(1));
    occurrence->setSummary("Exception occurrence");
    event->recurrence()->addExDateTime(occurrence->recurrenceId());
    KCalendarCore::Event::Ptr single(new KCalendarCore::Event);
    single->setDtStart(QDateTime(QDate(2021, 4, 26), QTime(17, 26), QTimeZone("Europe/Paris")));
    single->setSummary("Single event");

    QVERIFY(m_calendar->addEvent(event, notebook->uid()));
    QVERIFY(m_calendar->addEvent(occurrence, notebook->uid()));
    QVERIFY(m_calendar->addEvent(single, notebook->uid()));
    QVERIFY(m_storage->save());

    m_storage.clear();
    m_calendar.clear();
    m_calendar = ExtendedCalendar::Ptr(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = m_calendar->defaultStorage(m_calendar);
    m_storage->open();

    QVERIFY(m_calendar->events().isEmpty());

    QVERIFY(m_storage->loadIncidenceInstance(occurrence->instanceIdentifier()));
    QVERIFY(m_calendar->instance(occurrence->instanceIdentifier()));

    QVERIFY(m_storage->loadIncidenceInstance(event->instanceIdentifier()));
    QVERIFY(m_calendar->instance(event->instanceIdentifier()));

    QVERIFY(m_storage->loadIncidenceInstance(single->instanceIdentifier()));
    QVERIFY(m_calendar->instance(single->instanceIdentifier()));
}

void tst_storage::tst_url_data()
{
    QTest::addColumn<QUrl>("url");

    QTest::newRow("no URL")
        << QUrl();
    QTest::newRow("simple URL")
        << QUrl("http://example.org/dav/123-456-789.ics");
    QTest::newRow("percent encoded URL")
        << QUrl("https://example.org/dav%20user/123-456-789.ics");
}

void tst_storage::tst_url()
{
    QFETCH(QUrl, url);

    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2021, 1, 4), QTime(15, 37),
                                Qt::LocalTime));
    event->setSummary("URL test event");
    event->setUrl(url);
    QCOMPARE(event->url(), url);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    reloadDb();

    auto fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    QCOMPARE(fetchEvent->url(), url);
}

void tst_storage::tst_color()
{
    const QString &red = QString::fromLatin1("red");
    auto event = KCalendarCore::Event::Ptr(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2021, 1, 4), QTime(15, 59),
                                Qt::LocalTime));
    event->setSummary("Color test event");
    event->setColor(red);
    QCOMPARE(event->color(), red);

    m_calendar->addEvent(event, NotebookId);
    m_storage->save();
    reloadDb();

    auto fetchEvent = m_calendar->event(event->uid());
    QVERIFY(fetchEvent);
    QCOMPARE(fetchEvent->color(), red);

    const QString &green = QString::fromLatin1("green");
    fetchEvent->setColor(green);
    QCOMPARE(fetchEvent->color(), green);

    m_storage->save();
    reloadDb();

    auto updatedEvent = m_calendar->event(event->uid());
    QVERIFY(updatedEvent);
    QCOMPARE(updatedEvent->color(), green);
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

QTEST_GUILESS_MAIN(tst_storage)
