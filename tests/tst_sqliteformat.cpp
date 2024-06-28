/*
  Copyright (c) 2014-2019 Jolla Ltd.
  Copyright (c) 2019 Open Mobile Platform LLC.
  Copyright (c) 2023 Damien Caliste <dcaliste@free.fr>.

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

#include <QObject>
#include <QTest>
#include <QDebug>

#include <KCalendarCore/Event>
#include <KCalendarCore/ICalFormat>
#include <KCalendarCore/MemoryCalendar>
#include <KCalendarCore/OccurrenceIterator>

#include <sqlite3.h>

#include "sqliteformat.h"

using namespace KCalendarCore;
using namespace mKCal;

class tst_sqliteformat: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testTimezone();
    void testOriginTimes();
    void testDtStart_data();
    void testDtStart();
    void testAllDay_data();
    void testAllDay();
    void testAllDayUtc();
    void testAllDayRecurrence();
    void testRecurrence();
    void testDateCreated_data();
    void testDateCreated();
    void testLastModified();
    void testUrl_data();
    void testUrl();
    void testThisAndFuture();
    void testColor();
    void testAttachments();
    void testAttendees();
    void testIcalAllDay_data();
    void testIcalAllDay();
    void testRecurrenceExpansion_data();
    void testRecurrenceExpansion();
    void testRawEvents_data();
    void testRawEvents();
    void testRawEvents_nonRecur_data();
    void testRawEvents_nonRecur();
    void testCalendarProperties();

private:
    void fetchEvent(Event::Ptr *fetchedEvent, const QByteArray &uid,
                    const QDateTime &recurrenceId = QDateTime());

    SqliteFormat *mFormat = nullptr;
    Notebook mNotebook;
};

void tst_sqliteformat::init()
{
    mFormat = new SqliteFormat(QString());
    QVERIFY(mFormat->database());

    sqlite3_stmt *stmt = nullptr;
    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), INSERT_CALENDARS,
                                sizeof(INSERT_CALENDARS), &stmt, nullptr));
    QVERIFY(mFormat->modifyCalendars(mNotebook, DBInsert, stmt, false));
    sqlite3_finalize(stmt);
}

void tst_sqliteformat::cleanup()
{
    QVERIFY(mFormat->database());

    sqlite3_stmt *stmt = nullptr;
    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), DELETE_CALENDARS,
                                sizeof(DELETE_CALENDARS), &stmt, nullptr));
    QVERIFY(mFormat->modifyCalendars(mNotebook, DBDelete, stmt, false));
    sqlite3_finalize(stmt);

    QVERIFY(mFormat->purgeAllComponents(mNotebook.uid()));

    delete mFormat;
    mFormat = nullptr;
}

void tst_sqliteformat::testTimezone()
{
    // for test sanity, verify kdatetime actually agrees timezone is for helsinki.
    QDateTime localTime(QDate(2014, 1, 1), QTime(), QTimeZone("Europe/Helsinki"));
    QCOMPARE(localTime.utcOffset(), 7200);
}

void tst_sqliteformat::testOriginTimes()
{
    QDateTime utcTime(QDate(2014, 1, 15), QTime(), Qt::UTC);
    QDateTime localTime(QDate(2014, 1, 15), QTime(), Qt::LocalTime);

    // local origin time is the same as specific time set to utc
    // note: currently origin time of clock time is saved as time in current time zone.
    // that does not necessarily make sense, but better be careful when changing behavior there.
    QCOMPARE(SqliteFormat::toOriginTime(utcTime), SqliteFormat::toLocalOriginTime(utcTime));
    QCOMPARE(SqliteFormat::toLocalOriginTime(localTime), SqliteFormat::toLocalOriginTime(utcTime));
}

void tst_sqliteformat::fetchEvent(Event::Ptr *fetchedEvent, const QByteArray &uid,
                                  const QDateTime &recurrenceId)
{
    sqlite3_stmt *stmt = nullptr;
    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), SELECT_COMPONENTS_BY_UID,
                                sizeof(SELECT_COMPONENTS_BY_UID), &stmt, nullptr));
    QVERIFY(!sqlite3_bind_text(stmt, 1, uid.constData(), uid.length(), SQLITE_STATIC));

    QString nb;
    do {
        *fetchedEvent = mFormat->selectComponents(stmt, nb).staticCast<Event>();
    } while (*fetchedEvent && (*fetchedEvent)->recurrenceId() != recurrenceId);
    sqlite3_finalize(stmt);
}

void tst_sqliteformat::testDtStart_data()
{
    QTest::addColumn<QDateTime>("startDateTime");

    QTest::newRow("clock time") << QDateTime(QDate(2020, 5, 29), QTime(10, 15), Qt::LocalTime);
    QTest::newRow("UTC") << QDateTime(QDate(2020, 5, 29), QTime(10, 15), Qt::UTC);
    QTest::newRow("time zone") << QDateTime(QDate(2020, 5, 29), QTime(10, 15), QTimeZone("Europe/Paris"));
    QTest::newRow("date only") << QDateTime(QDate(2020, 5, 29));
    QTest::newRow("origin date time") << SqliteFormat::fromOriginTime(0);
    // Not allowed by RFC, will be converted to origin of time after save.
    QTest::newRow("bogus QDateTime") << QDateTime();
}

void tst_sqliteformat::testDtStart()
{
    QFETCH(QDateTime, startDateTime);

    Event::Ptr event(new Event);
    event->setDtStart(startDateTime);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    QVERIFY(fetchedEvent->dtStart().isValid());
    if (startDateTime.isValid()) {
        QCOMPARE(fetchedEvent->dtStart(), startDateTime);
    } else {
        SqliteFormat format(nullptr);
        // QDateTime is bogus, invalid date time == January 1st 1970.
        QCOMPARE(fetchedEvent->dtStart(), format.fromOriginTime(0));
    }
    QVERIFY(!fetchedEvent->hasEndDate());
}

void tst_sqliteformat::testAllDay_data()
{
    QTest::addColumn<QDate>("startDate");
    QTest::addColumn<int>("days");

    // DST changes according to finnish timezone
    // normal 1 day events
    QTest::newRow("out of range") << QDate(2011, 10, 10) << 0;
    QTest::newRow("normal") << QDate(2013, 10, 10) << 0;
    QTest::newRow("to non-DST") << QDate(2013, 10, 27) << 0;
    QTest::newRow("to DST") << QDate(2013, 3, 31) << 0;

    // 2 day events
    QTest::newRow("normal, 2d") << QDate(2013, 10, 10) << 1;
    QTest::newRow("to non-DST, 2d") << QDate(2013, 10, 27) << 1;
    QTest::newRow("to DST, 2d") << QDate(2013, 3, 31) << 1;
}

void tst_sqliteformat::testAllDay()
{
    Event::Ptr event(new Event);

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

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
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

void tst_sqliteformat::testAllDayUtc()
{
    // test event saved with UTC time
    Event::Ptr event(new Event);
    QDate startDate(2013, 12, 1);
    event->setDtStart(QDateTime(startDate, QTime(), Qt::UTC));
    event->setAllDay(true);
    event->setSummary("test event utc");

    QCOMPARE(event->allDay(), true);
    QCOMPARE(event->dtStart().timeSpec(), Qt::UTC);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
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
void tst_sqliteformat::testAllDayRecurrence()
{
    Event::Ptr event(new Event);

    QDate startDate(2013, 12, 1);
    event->setDtStart(QDateTime(startDate));
    event->setAllDay(true);

    Recurrence *recurrence = event->recurrence();
    recurrence->setWeekly(1);
    recurrence->setStartDateTime(event->dtStart(), true);
    recurrence->setAllDay(true);
    recurrence->addRDate(startDate.addDays(2));

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    Recurrence *fetchRecurrence = fetchedEvent->recurrence();
    QVERIFY(fetchRecurrence);
    QCOMPARE(*recurrence, *fetchRecurrence);
    QDateTime match = fetchRecurrence->getNextDateTime(QDateTime(startDate));
    QCOMPARE(match, QDateTime(startDate.addDays(2)));
    match = fetchRecurrence->getNextDateTime(QDateTime(startDate.addDays(3)));
    QCOMPARE(match, QDateTime(startDate.addDays(7), QTime(), Qt::LocalTime));
}

// Verify that a recurrence with an exception rule is properly saved
void tst_sqliteformat::testRecurrence()
{
    Event::Ptr event(new Event);

    QDate startDate(2013, 12, 1);
    QTime startTime(12, 34, 56);
    event->setDtStart(QDateTime(startDate, startTime, Qt::LocalTime));

    Recurrence *recurrence = event->recurrence();
    recurrence->setDaily(1);
    RecurrenceRule *rrule = new RecurrenceRule;
    rrule->setRecurrenceType(RecurrenceRule::rWeekly);
    rrule->setDuration(5);
    recurrence->addExRule(rrule);
    recurrence->setStartDateTime(event->dtStart(), false);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    Recurrence *fetchRecurrence = fetchedEvent->recurrence();
    QVERIFY(fetchRecurrence);
    QCOMPARE(*recurrence, *fetchRecurrence);
    QDateTime match = recurrence->getNextDateTime(event->dtStart());
    QCOMPARE(match, event->dtStart().addDays(1));
}

// Check that the creation date can be tuned and restored properly.
void tst_sqliteformat::testDateCreated_data()
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

void tst_sqliteformat::testDateCreated()
{
    QFETCH(QDateTime, dateCreated);
    QFETCH(QDateTime, dateCreated_update);

    // Verify that date craetion date can be tuned on new insertion and on update.
    Event::Ptr event(new Event);
    event->setDtStart(QDateTime(QDate(2019, 04, 01), QTime(10, 11),
                                Qt::LocalTime));
    event->setSummary("Creation date test event");
    event->setCreated(dateCreated.toUTC());

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    if (dateCreated.isNull()) {
        QVERIFY(fetchedEvent->created().secsTo(QDateTime::currentDateTimeUtc()) <= 1);
    } else {
        QCOMPARE(fetchedEvent->created(), dateCreated);
    }

    if (!dateCreated_update.isNull()) {
        fetchedEvent->startUpdates();
        fetchedEvent->setCreated(dateCreated_update.toUTC());
        fetchedEvent->endUpdates();
        QVERIFY(mFormat->modifyComponents(*fetchedEvent, mNotebook.uid(), DBUpdate));

        fetchEvent(&fetchedEvent, event->uid().toUtf8());
        QVERIFY(fetchedEvent);
        QCOMPARE(fetchedEvent->created(), dateCreated_update);
    }
}

// Check that lastModified field is not modified by storage,
void tst_sqliteformat::testLastModified()
{
    QDateTime dt(QDate(2019, 07, 26), QTime(11, 41), Qt::LocalTime);
    Event::Ptr event(new Event);
    event->setDtStart(dt.addDays(1));
    event->setSummary("Modified date test event");
    event->setLastModified(dt);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));
    QCOMPARE(event->lastModified(), dt);

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    QCOMPARE(fetchedEvent->lastModified(), dt);
}

void tst_sqliteformat::testUrl_data()
{
    QTest::addColumn<QUrl>("url");

    QTest::newRow("no URL")
        << QUrl();
    QTest::newRow("simple URL")
        << QUrl("http://example.org/dav/123-456-789.ics");
    QTest::newRow("percent encoded URL")
        << QUrl("https://example.org/dav%20user/123-456-789.ics");
}

void tst_sqliteformat::testUrl()
{
    QFETCH(QUrl, url);

    Event::Ptr event(new Event);
    event->setDtStart(QDateTime(QDate(2021, 1, 4), QTime(15, 37),
                                Qt::LocalTime));
    event->setSummary("URL test event");
    event->setUrl(url);
    QCOMPARE(event->url(), url);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    QCOMPARE(fetchedEvent->url(), url);
}

void tst_sqliteformat::testThisAndFuture()
{
    Event::Ptr event(new Event);
    event->setDtStart(QDateTime(QDate(2022, 1, 17), QTime(10, 0)));
    event->recurrence()->setDaily(1);
    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));
    Incidence::Ptr exception(event->clone());
    exception->clearRecurrence();
    exception->setRecurrenceId(event->dtStart().addDays(3));
    exception->setDtStart(QDateTime(QDate(2022, 1, 20), QTime(9, 0)));
    exception->setThisAndFuture(true);
    QVERIFY(mFormat->modifyComponents(*exception, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    QVERIFY(!fetchedEvent->thisAndFuture());
    Event::Ptr fetchedException;
    fetchEvent(&fetchedException, event->uid().toUtf8(), exception->recurrenceId());
    QVERIFY(fetchedException);
    QVERIFY(fetchedException->thisAndFuture());
}

void tst_sqliteformat::testColor()
{
    const QString &red = QString::fromLatin1("red");
    Event::Ptr event(new Event);
    event->setDtStart(QDateTime(QDate(2021, 1, 4), QTime(15, 59),
                                Qt::LocalTime));
    event->setSummary("Color test event");
    event->setColor(red);
    QCOMPARE(event->color(), red);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    QCOMPARE(fetchedEvent->color(), red);

    const QString &green = QString::fromLatin1("green");
    fetchedEvent->setColor(green);
    QCOMPARE(fetchedEvent->color(), green);

    QVERIFY(mFormat->modifyComponents(*fetchedEvent, mNotebook.uid(), DBUpdate));

    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    QCOMPARE(fetchedEvent->color(), green);
}

void tst_sqliteformat::testAttachments()
{
    Event::Ptr event(new Event);
    event->setSummary("testing attachments.");
    Attachment uriAttach(QString::fromUtf8("http://example.org/foo.png"),
                         QString::fromUtf8("image/png"));
    uriAttach.setLabel(QString::fromUtf8("Foo image"));
    uriAttach.setShowInline(true);
    uriAttach.setLocal(false);
    event->addAttachment(uriAttach);
    Attachment binAttach(QByteArray("qwertyuiop").toBase64(),
                         QString::fromUtf8("audio/ogg"));
    binAttach.setShowInline(false);
    binAttach.setLocal(true);
    event->addAttachment(binAttach);
    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr without(new Event);
    without->setSummary("testing events without attachment.");
    QVERIFY(mFormat->modifyComponents(*without, mNotebook.uid(), DBInsert));

    Event::Ptr another(new Event);
    another->setSummary("testing another event with an attachment.");
    another->addAttachment(uriAttach);
    QVERIFY(mFormat->modifyComponents(*another, mNotebook.uid(), DBInsert));

    Event::Ptr fetchedEvent;
    fetchEvent(&fetchedEvent, event->uid().toUtf8());
    QVERIFY(fetchedEvent);
    const Attachment::List attachments = fetchedEvent->attachments();
    QCOMPARE(attachments.length(), 2);
    QCOMPARE(attachments[0], uriAttach);
    QCOMPARE(attachments[1], binAttach);

    fetchEvent(&fetchedEvent, without->uid().toUtf8());
    QVERIFY(fetchedEvent);
    QVERIFY(fetchedEvent->attachments().isEmpty());
}

void tst_sqliteformat::testAttendees()
{
    Event::Ptr event(new Event);
    event->setSummary("testing attendees.");
    event->setDtStart(QDateTime(QDate(2022, 2, 23), QTime(14, 33)));

    event->setOrganizer(Person(QString::fromLatin1("Alice"),
                               QString::fromLatin1("alice@example.org")));
    event->addAttendee(Attendee(event->organizer().name(),
                                event->organizer().email(), true,
                                Attendee::Accepted,
                                Attendee::Chair));
    event->addAttendee(Attendee(QString::fromLatin1("Bob"),
                                QString::fromLatin1("bob@example.org"),
                                true,
                                Attendee::Tentative,
                                Attendee::OptParticipant));
    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    // Organizer is in the attendee list.
    Event::Ptr fetched;
    fetchEvent(&fetched, event->uid().toUtf8());
    QVERIFY(fetched);
    QCOMPARE(fetched->organizer(), event->organizer());
    QCOMPARE(fetched->attendees(), event->attendees());

    fetched->setOrganizer(Person(QString::fromLatin1("Carl"),
                                 QString::fromLatin1("carl@example.org")));
    QVERIFY(mFormat->modifyComponents(*fetched, mNotebook.uid(), DBUpdate));

    // ensure reloaded event doesn't have organizer added to attendees
    fetchEvent(&fetched, event->uid().toUtf8());
    QVERIFY(fetched);
    QCOMPARE(fetched->organizer(), fetched->organizer());
    QCOMPARE(fetched->attendees(), fetched->attendees());
}

// Test various way of describing all day events in iCal format.
void tst_sqliteformat::testIcalAllDay_data()
{
    QTest::addColumn<QString>("uid");
    QTest::addColumn<QString>("vEvent");
    QTest::addColumn<bool>("allDay");

    QTest::newRow("local time")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A70")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART:20190607T000000\n"
                          "DTEND:20190608T000000\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A70\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT")
        << false;
    QTest::newRow("UTC")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A71")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART:20190607T000000Z\n"
                          "DTEND:20190608T000000Z\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A71\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT")
        << false;
    QString zid = QString::fromLatin1(QTimeZone::systemTimeZoneId());
    QTest::newRow("system time zone")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A72")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART;TZID=%1:20190607T000000\n"
                          "DTEND;TZID=%1:20190608T000000\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A72\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT").arg(zid)
        << false;
    QTest::newRow("floating date")
        << QStringLiteral("14B902BC-8D24-4A97-8541-63DF7FD41A73")
        << QStringLiteral("BEGIN:VEVENT\n"
                          "DTSTART:20190607\n"
                          "UID:14B902BC-8D24-4A97-8541-63DF7FD41A73\n"
                          "SUMMARY:Test03\n"
                          "END:VEVENT")
        << true;
}

void tst_sqliteformat::testIcalAllDay()
{
    QFETCH(QString, uid);
    QFETCH(QString, vEvent);
    QFETCH(bool, allDay);

    const QString icsData =
        QStringLiteral("BEGIN:VCALENDAR\n"
                       "PRODID:-//NemoMobile.org/Nemo//NONSGML v1.0//EN\n"
                       "VERSION:2.0\n") + vEvent +
        QStringLiteral("\nEND:VCALENDAR");
    ICalFormat format;
    MemoryCalendar::Ptr calendar(new MemoryCalendar(QTimeZone::systemTimeZone()));
    QVERIFY(format.fromString(calendar, icsData));
    Event::Ptr event = calendar->event(uid);
    QVERIFY(event);
    QCOMPARE(event->allDay(), allDay);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetched;
    fetchEvent(&fetched, event->uid().toUtf8());
    QVERIFY(fetched);
    QCOMPARE(fetched->allDay(), allDay);
    QCOMPARE(event->dtStart(), fetched->dtStart());
    QCOMPARE(event->dtEnd(), fetched->dtEnd());
}

typedef struct ExpandedIncidenceValidity {
        QDateTime dtStart;
        QDateTime dtEnd;
} ExpandedIncidenceValidity;
typedef QPair<ExpandedIncidenceValidity, Incidence::Ptr> ExpandedIncidence;
typedef QVector<ExpandedIncidence> ExpandedIncidenceList;
static ExpandedIncidenceList rawExpandedIncidences(const Calendar &calendar, const QDateTime &start, const QDateTime &end)
{
    ExpandedIncidenceList eventList;

    OccurrenceIterator it(calendar, start, end);
    while (it.hasNext()) {
        it.next();
        if (calendar.isVisible(it.incidence())) {
            const QDateTime sdt = it.occurrenceStartDate();
            const Duration
                enlapsed(it.incidence()->dateTime(Incidence::RoleDisplayStart),
                         it.incidence()->dateTime(Incidence::RoleDisplayEnd),
                         Duration::Seconds);
            const ExpandedIncidenceValidity eiv = {sdt, enlapsed.end(sdt)};
            eventList.append(qMakePair(eiv, it.incidence()));
        }
    }

    return eventList;
}

Q_DECLARE_METATYPE(QTimeZone)
void tst_sqliteformat::testRecurrenceExpansion_data()
{
    QTest::addColumn<QTimeZone>("eventTimeZone");
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
        << QTimeZone() // ClockTime
        << QByteArray("Australia/Brisbane")
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00")
                          << QStringLiteral("2019-11-11T02:00:00")
                          << QStringLiteral("2019-11-12T03:00:00")
                          << QStringLiteral("2019-11-13T02:00:00")
                          << QStringLiteral("2019-11-14T02:00:00")
                          << QStringLiteral("2019-11-15T02:00:00")
                          << QStringLiteral("2019-11-18T02:00:00"));

    QTest::newRow("created in Brisbane, expanded in Brisbane")
        << QTimeZone("Australia/Brisbane")
        << QByteArray("Australia/Brisbane")
        << QString("2019-11-18T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+10:00")
                          << QStringLiteral("2019-11-11T02:00:00+10:00")
                          << QStringLiteral("2019-11-12T03:00:00+10:00")
                          << QStringLiteral("2019-11-13T02:00:00+10:00")
                          << QStringLiteral("2019-11-14T02:00:00+10:00")
                          << QStringLiteral("2019-11-15T02:00:00+10:00")
                          << QStringLiteral("2019-11-18T02:00:00+10:00"));

    QTest::newRow("created in Brisbane, expanded in Paris")
        << QTimeZone("Australia/Brisbane")
        << QByteArray("Europe/Paris") // up to the end of the 18th in Paris time includes the morning of the 19th in Brisbane time
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+10:00")
                          << QStringLiteral("2019-11-11T02:00:00+10:00")
                          << QStringLiteral("2019-11-12T03:00:00+10:00")
                          << QStringLiteral("2019-11-13T02:00:00+10:00")
                          << QStringLiteral("2019-11-14T02:00:00+10:00")
                          << QStringLiteral("2019-11-15T02:00:00+10:00")
                          << QStringLiteral("2019-11-18T02:00:00+10:00")
                          << QStringLiteral("2019-11-19T02:00:00+10:00"));

    QTest::newRow("created in Paris, expanded in Paris")
        << QTimeZone("Europe/Paris")
        << QByteArray("Europe/Paris")
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+01:00")
                          << QStringLiteral("2019-11-11T02:00:00+01:00")
                          << QStringLiteral("2019-11-12T03:00:00+01:00")
                          << QStringLiteral("2019-11-13T02:00:00+01:00")
                          << QStringLiteral("2019-11-14T02:00:00+01:00")
                          << QStringLiteral("2019-11-15T02:00:00+01:00")
                          << QStringLiteral("2019-11-18T02:00:00+01:00"));

    QTest::newRow("created in Paris, expanded in Brisbane")
        << QTimeZone("Europe/Paris")
        << QByteArray("Australia/Brisbane")
        << QString("2019-11-19T00:00:00Z")
        << (QStringList() << QStringLiteral("2019-11-08T02:00:00+01:00")
                          << QStringLiteral("2019-11-11T02:00:00+01:00")
                          << QStringLiteral("2019-11-12T03:00:00+01:00")
                          << QStringLiteral("2019-11-13T02:00:00+01:00")
                          << QStringLiteral("2019-11-14T02:00:00+01:00")
                          << QStringLiteral("2019-11-15T02:00:00+01:00")
                          << QStringLiteral("2019-11-18T02:00:00+01:00"));

}

// Verify that expansion of recurring event takes timezone into account
void tst_sqliteformat::testRecurrenceExpansion()
{
    QFETCH(QTimeZone, eventTimeZone);
    QFETCH(QByteArray, expansionTimeZone);
    QFETCH(QString, intervalEnd);
    QFETCH(QStringList, expectedEvents);

    const QByteArray TZenv(qgetenv("TZ"));
    // Ensure testing of the creation of the event
    // is done in a timezone different from the event
    // time zone and from the expansionTimeZone.
    qputenv("TZ", "UTC");

    // Create an event which occurs every weekday of every week,
    // starting from Friday the 8th of November, from 2 am until 3 am.
    Event::Ptr event = Event::Ptr(new Event);
    event->setUid(QStringLiteral("tst_recurrenceExpansion:%1:%2:%3").arg(eventTimeZone.id(), expansionTimeZone, intervalEnd));
    event->setLocation(QStringLiteral("Test location"));
    event->setAllDay(false);
    event->setDescription(QStringLiteral("Test description"));
    if (eventTimeZone.isValid()) {
        event->setDtStart(QDateTime(QDate(2019,11,8),
                                    QTime(02,00,00),
                                    eventTimeZone));
        event->setDtEnd(QDateTime(QDate(2019,11,8),
                                  QTime(03,00,00),
                                  eventTimeZone));
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

    RecurrenceRule * const rule = new RecurrenceRule();
    rule->setRecurrenceType(RecurrenceRule::rWeekly);
    rule->setStartDt(event->dtStart());
    rule->setFrequency(1);
    rule->setByDays(QList<RecurrenceRule::WDayPos>()
            << RecurrenceRule::WDayPos(0, 1)   // monday
            << RecurrenceRule::WDayPos(0, 2)   // tuesday
            << RecurrenceRule::WDayPos(0, 3)   // wednesday
            << RecurrenceRule::WDayPos(0, 4)   // thursday
            << RecurrenceRule::WDayPos(0, 5)); // friday
    event->recurrence()->addRRule(rule);
    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    // Create also an exception on the 12th.
    Incidence::Ptr exception(event->clone());
    exception->clearRecurrence();
    exception->setRecurrenceId(event->dtStart().addDays(4));
    exception->setDtStart(event->dtStart().addSecs(3600 * 24 * 4 + 3600));
    exception.staticCast<Event>()->setDtEnd(event->dtEnd().addSecs(3600 * 24 * 4 + 3600));
    QVERIFY(mFormat->modifyComponents(*exception, mNotebook.uid(), DBInsert));

    qputenv("TZ", expansionTimeZone);

    Event::Ptr fetched;
    fetchEvent(&fetched, event->uid().toUtf8());
    QVERIFY(fetched);
    QDateTime match = fetched->recurrence()->getNextDateTime(event->dtStart());
    QCOMPARE(match, event->dtStart().addDays(3)); // skip the weekend

    MemoryCalendar calendar(expansionTimeZone);
    calendar.addIncidence(fetched);
    fetchEvent(&fetched, event->uid().toUtf8(), exception->recurrenceId());
    QVERIFY(fetched);
    calendar.addIncidence(fetched);
    ExpandedIncidenceList expandedEvents
        = rawExpandedIncidences(calendar, QDateTime(QDate(2019, 11, 05)),
                                QDateTime(QDate(2019, 11, 18), QTime(23, 59, 59)));

    const DateTimeList timesInInterval = event->recurrence()->timesInInterval(
            QDateTime::fromString(QStringLiteral("2019-11-05T00:00:00Z"), Qt::ISODate),
            QDateTime::fromString(intervalEnd, Qt::ISODate));

    QCOMPARE(expandedEvents.size(), expectedEvents.size());
    if (eventTimeZone.isValid()) {
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
        const QDateTime dt = QDateTime::fromString(expectedEvents.at(i), Qt::ISODate);
        if (eventTimeZone.isValid()) {
            QCOMPARE(expandedEvents.at(i).first.dtStart, dt);
            if (dt.date() != QDate(2019,11,12)) // timesInInterval returns the original time
                QCOMPARE(timesInInterval.at(i), dt);
        } else {
            QDateTime tsExpEvent = expandedEvents.at(i).first.dtStart;
            QDateTime tsTimeInInterval = timesInInterval.at(i);
            tsExpEvent.setTimeSpec(Qt::LocalTime);
            tsTimeInInterval.setTimeSpec(Qt::LocalTime);
            QCOMPARE(tsExpEvent, dt);
            if (dt.date() != QDate(2019,11,12))
                QCOMPARE(tsTimeInInterval, dt);
        }
    }

    if (TZenv.isEmpty()) {
        qunsetenv("TZ");
    } else {
        qputenv("TZ", TZenv);
    }
}

void tst_sqliteformat::testRawEvents_data()
{
    QTest::addColumn<QDate>("date");
    QTest::addColumn<QTime>("startTime");
    QTest::addColumn<QTime>("endTime");
    QTest::addColumn<QTimeZone>("timeZone");
    QTest::addColumn<QByteArray>("exceptionTimeZone");
    QTest::addColumn<bool>("rangeCutsOffFirst");
    QTest::addColumn<bool>("secondExceptionApplies");
    QTest::addColumn<bool>("rangeCutsOffLast");

    QTest::newRow("non all day event in clock time with exception in Europe/Helsinki")
        << QDate(2010, 01, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone()
        << QByteArray("Europe/Helsinki")
        << false
        << (QDateTime(QDate(2019, 01, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 01, 03), QTime(00, 00), QTimeZone("Europe/Helsinki"))) == 0)
        << false;

    QTest::newRow("non all day event in clock time with exception in America/Toronto")
        << QDate(2010, 01, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone()
        << QByteArray("America/Toronto")
        << false
        << (QDateTime(QDate(2019, 01, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 01, 03), QTime(00, 00), QTimeZone("America/Toronto"))) == 0)
        << false;

    QTest::newRow("non all day event in Europe/Helsinki with exception in Europe/Helsinki")
        << QDate(2010, 02, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone("Europe/Helsinki")
        << QByteArray("Europe/Helsinki")
        << (QDateTime(QDate(2011, 02, 01), QTime(12, 0), QTimeZone("Europe/Helsinki")).secsTo(
            QDateTime(QDate(2011, 02, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << true // event tz and exception tz are equal
        << (QDateTime(QDate(2011, 02, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 02, 04), QTime(12, 00, 00), QTimeZone("Europe/Helsinki"))) > 0);

    QTest::newRow("non all day event in Europe/Helsinki with exception in America/Toronto")
        << QDate(2010, 02, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone("Europe/Helsinki")
        << QByteArray("America/Toronto")
        << (QDateTime(QDate(2011, 02, 01), QTime(12, 0), QTimeZone("Europe/Helsinki")).secsTo(
            QDateTime(QDate(2011, 02, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false // event tz and exception tz are unequal
        << (QDateTime(QDate(2011, 02, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 02, 04), QTime(12, 00, 00), QTimeZone("Europe/Helsinki"))) > 0);

    QTest::newRow("non all day event in Pacific/Midway with exception in Europe/Helsinki")
        << QDate(2010, 03, 01)
        << QTime(8, 0) << QTime(9, 0)
        << QTimeZone("Pacific/Midway")
        << QByteArray("Europe/Helsinki")
        << (QDateTime(QDate(2011, 03, 01), QTime(12, 0), QTimeZone("Pacific/Midway")).secsTo(
            QDateTime(QDate(2011, 03, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 03, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 03, 04), QTime(8, 00, 00), QTimeZone("Pacific/Midway"))) > 0);

    QTest::newRow("non all day event in Pacific/Midway with exception in America/Toronto")
        << QDate(2010, 03, 01)
        << QTime(8, 0) << QTime(9, 0)
        << QTimeZone("Pacific/Midway")
        << QByteArray("America/Toronto")
        << (QDateTime(QDate(2011, 03, 01), QTime(12, 0), QTimeZone("Pacific/Midway")).secsTo(
            QDateTime(QDate(2011, 03, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 03, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 03, 04), QTime(8, 00, 00), QTimeZone("Pacific/Midway"))) > 0);

    QTest::newRow("all day event stored as local clock with exception in Europe/Helsinki")
        << QDate(2010, 04, 01)
        << QTime(0, 0) << QTime()
        << QTimeZone()
        << QByteArray("Europe/Helsinki")
        << false
        << (QDateTime(QDate(2019, 04, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 04, 03), QTime(00, 00), QTimeZone("Europe/Helsinki"))) == 0)
        << false;

    QTest::newRow("all day event stored as local clock with exception in America/Toronto")
        << QDate(2010, 04, 01)
        << QTime(0, 0) << QTime()
        << QTimeZone()
        << QByteArray("America/Toronto")
        << false
        << (QDateTime(QDate(2019, 04, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 04, 03), QTime(00, 00), QTimeZone("America/Toronto"))) == 0)
        << false;

    QTest::newRow("all day event stored as date only with exception in Europe/Helsinki")
        << QDate(2010, 05, 01)
        << QTime() << QTime()
        << QTimeZone()
        << QByteArray("Europe/Helsinki")
        << false
        << (QDateTime(QDate(2019, 05, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 05, 03), QTime(00, 00), QTimeZone("Europe/Helsinki"))) == 0)
        << false;

    QTest::newRow("all day event stored as date only with exception in America/Toronto")
        << QDate(2010, 05, 01)
        << QTime() << QTime()
        << QTimeZone()
        << QByteArray("America/Toronto")
        << false
        << (QDateTime(QDate(2019, 05, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 05, 03), QTime(00, 00), QTimeZone("America/Toronto"))) == 0)
        << false;

    QTest::newRow("non all day event in clock time with exception in Australia/Brisbane")
        << QDate(2011, 06, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone()
        << QByteArray("Australia/Brisbane")
        << false
        << (QDateTime(QDate(2019, 06, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 06, 03), QTime(00, 00), QTimeZone("Australia/Brisbane"))) == 0)
        << false;

    QTest::newRow("non all day event in Europe/Helsinki with exception in Australia/Brisbane")
        << QDate(2011, 06, 01)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone("Europe/Helsinki")
        << QByteArray("Australia/Brisbane")
        << (QDateTime(QDate(2011, 06, 01), QTime(12, 0), QTimeZone("Europe/Helsinki")).secsTo(
            QDateTime(QDate(2011, 06, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 06, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 06, 04), QTime(12, 00, 00), QTimeZone("Europe/Helsinki"))) > 0);

    QTest::newRow("non all day event in Pacific/Midway with exception in Australia/Brisbane")
        << QDate(2011, 06, 01)
        << QTime(8, 0) << QTime(9, 0)
        << QTimeZone("Pacific/Midway")
        << QByteArray("Australia/Brisbane")
        << (QDateTime(QDate(2011, 06, 01), QTime(12, 0), QTimeZone("Pacific/Midway")).secsTo(
            QDateTime(QDate(2011, 06, 01), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 06, 04), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 06, 04), QTime(8, 00, 00), QTimeZone("Pacific/Midway"))) > 0);

    QTest::newRow("all day event stored as local clock with exception in Australia/Brisbane")
        << QDate(2011, 07, 01)
        << QTime(0, 0) << QTime()
        << QTimeZone()
        << QByteArray("Australia/Brisbane")
        << false
        << (QDateTime(QDate(2019, 07, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 07, 03), QTime(00, 00), QTimeZone("Australia/Brisbane"))) == 0)
        << false;

    QTest::newRow("all day event stored as date only with exception in Australia/Brisbane")
        << QDate(2011, 07, 01)
        << QTime() << QTime()
        << QTimeZone()
        << QByteArray("Australia/Brisbane")
        << false
        << (QDateTime(QDate(2019, 07, 03), QTime(00, 00), Qt::LocalTime).secsTo(
            QDateTime(QDate(2019, 07, 03), QTime(00, 00), QTimeZone("Australia/Brisbane"))) == 0)
        << false;

    QTest::newRow("non all day event in America/Toronto with exception in Australia/Brisbane")
        << QDate(2011, 8, 1)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone("America/Toronto")
        << QByteArray("Australia/Brisbane")
        << (QDateTime(QDate(2011, 8, 1), QTime(12, 0), QTimeZone("America/Toronto")).secsTo(
            QDateTime(QDate(2011, 8, 1), QTime(00, 0), Qt::LocalTime)) > 0)
        << false
        << (QDateTime(QDate(2011, 8, 4), QTime(23, 59, 59), Qt::LocalTime).secsTo(
            QDateTime(QDate(2011, 8, 4), QTime(12, 00, 00), QTimeZone("America/Toronto"))) > 0);

    QTest::newRow("non all day event in America/Toronto with exception in America/Toronto")
        << QDate(2011, 8, 1)
        << QTime(12, 0) << QTime(13, 0)
        << QTimeZone("America/Toronto")
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
void tst_sqliteformat::testRawEvents()
{
    QFETCH(QDate, date);
    QFETCH(QTime, startTime);
    QFETCH(QTime, endTime);
    QFETCH(QTimeZone, timeZone);
    QFETCH(QByteArray, exceptionTimeZone);
    QFETCH(bool, rangeCutsOffFirst);
    QFETCH(bool, secondExceptionApplies);
    QFETCH(bool, rangeCutsOffLast);

    const QTimeZone exceptionSpec(exceptionTimeZone);
    const QTimeZone expansionSpec = QTimeZone::systemTimeZone();

    Event::Ptr event(new Event);
    if (startTime.isValid()) {
        if (timeZone.isValid()) {
            event->setDtStart(QDateTime(date, startTime, timeZone));
        } else {
            event->setDtStart(QDateTime(date, startTime, Qt::LocalTime));
        }
        if (endTime.isValid() && timeZone.isValid()) {
            event->setDtEnd(QDateTime(date, endTime, timeZone));
        } else if (endTime.isValid()) {
            event->setDtEnd(QDateTime(date, endTime, Qt::LocalTime));
        } else if (startTime == QTime(0, 0)) {
            event->setAllDay(true);
        }
    } else {
        event->setDtStart(QDateTime(date));
        event->setAllDay(true);
    }
    event->setSummary(QStringLiteral("testing rawExpandedIncidences()"));

    Recurrence *recurrence = event->recurrence();
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

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetched;
    fetchEvent(&fetched, event->uid().toUtf8());
    QVERIFY(fetched);
    QCOMPARE(fetched->allDay(), event->allDay());
    Recurrence *fetchRecurrence = fetched->recurrence();
    QVERIFY(fetchRecurrence);
    QCOMPARE(fetchRecurrence->allDay(), recurrence->allDay());

    MemoryCalendar calendar(QTimeZone::systemTimeZone());
    calendar.addIncidence(fetched);
    // should return occurrence for expected days and omit exceptions
    ExpandedIncidenceList events
        = rawExpandedIncidences(calendar, QDateTime(date),
                                QDateTime(date.addDays(3), QTime(23, 59, 59)));

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

void tst_sqliteformat::testRawEvents_nonRecur_data()
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

void tst_sqliteformat::testRawEvents_nonRecur()
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

    const QByteArray TZenv(qgetenv("TZ"));
    qputenv("TZ", expansionTimeZone);

    const QTimeZone spec(timeZone);
    Event::Ptr event(new Event);
    if (spec.isValid()) {
        event->setDtStart(QDateTime(startDate, startTime, spec));
        event->setDtEnd(QDateTime(endDate, endTime, spec));
    } else {
        event->setDtStart(QDateTime(startDate, startTime, Qt::LocalTime));
        event->setDtEnd(QDateTime(endDate, endTime, Qt::LocalTime));
    }
    event->setSummary(QStringLiteral("testing rawExpandedIncidences, non-recurring: %2").arg(eventUid));
    event->setUid(eventUid);

    QVERIFY(mFormat->modifyComponents(*event, mNotebook.uid(), DBInsert));

    Event::Ptr fetched;
    fetchEvent(&fetched, event->uid().toUtf8());
    QVERIFY(fetched);
    if (spec.isValid()) {
        QCOMPARE(fetched->dtStart(), QDateTime(startDate, startTime, spec));
        QCOMPARE(fetched->dtEnd(), QDateTime(endDate, endTime, spec));
    } else {
        QCOMPARE(fetched->dtStart(), QDateTime(startDate, startTime, Qt::LocalTime));
        QCOMPARE(fetched->dtEnd(), QDateTime(endDate, endTime, Qt::LocalTime));
    }

    MemoryCalendar calendar(QTimeZone::systemTimeZone());
    calendar.addIncidence(fetched);
    ExpandedIncidenceList events
        = rawExpandedIncidences(calendar, QDateTime(rangeStartDate),
                                QDateTime(rangeEndDate, QTime(23, 59, 59)));

    QCOMPARE(events.size(), expectFound ? 1 : 0);
    if (expectFound) {
        QCOMPARE(events[0].second->summary(), event->summary());
        QCOMPARE(events[0].first.dtStart, event->dtStart());
        QCOMPARE(events[0].first.dtEnd, event->dtEnd());
    }

    if (TZenv.isEmpty()) {
        qunsetenv("TZ");
    } else {
        qputenv("TZ", TZenv);
    }
}

void tst_sqliteformat::testCalendarProperties()
{
    Notebook::Ptr notebook(new Notebook(QStringLiteral("Notebook"), QString()));
    const QString uid = notebook->uid();

    QCOMPARE(notebook->customPropertyKeys().count(), 0);
    const QByteArray propKey("a key");
    const QString propValue = QStringLiteral("a value");
    notebook->setCustomProperty(propKey, propValue);
    QCOMPARE(notebook->customPropertyKeys().count(), 1);
    QCOMPARE(notebook->customProperty(propKey), propValue);

    sqlite3_stmt *stmt = nullptr;
    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), INSERT_CALENDARS,
                                sizeof(INSERT_CALENDARS), &stmt, nullptr));
    QVERIFY(mFormat->modifyCalendars(*notebook, DBInsert, stmt, false));
    sqlite3_finalize(stmt);

    notebook.clear();
    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), SELECT_CALENDARS_ALL,
                                sizeof(SELECT_CALENDARS_ALL), &stmt, nullptr));
    bool isDefault;
    Notebook::Ptr nb;
    while ((nb = mFormat->selectCalendars(stmt, &isDefault))) {
        if (nb->uid() == uid) {
            notebook = nb;
        }
    }
    sqlite3_finalize(stmt);

    QVERIFY(notebook);
    QCOMPARE(notebook->customPropertyKeys().count(), 1);
    QCOMPARE(notebook->customProperty(propKey), propValue);

    const QByteArray propKey2("a second key");
    const QString propValue2 = QStringLiteral("another value");
    notebook->setCustomProperty(propKey2, propValue2);
    QCOMPARE(notebook->customPropertyKeys().count(), 2);
    QCOMPARE(notebook->customProperty(propKey2), propValue2);

    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), UPDATE_CALENDARS,
                                sizeof(UPDATE_CALENDARS), &stmt, nullptr));
    QVERIFY(mFormat->modifyCalendars(*notebook, DBUpdate, stmt, false));
    sqlite3_finalize(stmt);

    notebook.clear();
    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), SELECT_CALENDARS_ALL,
                                sizeof(SELECT_CALENDARS_ALL), &stmt, nullptr));
    while ((nb = mFormat->selectCalendars(stmt, &isDefault))) {
        if (nb->uid() == uid) {
            notebook = nb;
        }
    }
    sqlite3_finalize(stmt);

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

    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), UPDATE_CALENDARS,
                                sizeof(UPDATE_CALENDARS), &stmt, nullptr));
    QVERIFY(mFormat->modifyCalendars(*notebook, DBUpdate, stmt, false));
    sqlite3_finalize(stmt);

    notebook.clear();
    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), SELECT_CALENDARS_ALL,
                                sizeof(SELECT_CALENDARS_ALL), &stmt, nullptr));
    while ((nb = mFormat->selectCalendars(stmt, &isDefault))) {
        if (nb->uid() == uid) {
            notebook = nb;
        }
    }
    sqlite3_finalize(stmt);

    QVERIFY(notebook);
    QCOMPARE(notebook->customPropertyKeys().count(), 1);
    QCOMPARE(notebook->customProperty(propKey), propValue);
    QCOMPARE(notebook->customProperty(propKey2), QString());

    QVERIFY(!sqlite3_prepare_v2(mFormat->database(), DELETE_CALENDARS,
                                sizeof(DELETE_CALENDARS), &stmt, nullptr));
    QVERIFY(mFormat->modifyCalendars(*notebook, DBDelete, stmt, false));
    sqlite3_finalize(stmt);

    // Need to check by hand that property entries have been deleted.
    int rv;
    const char *query = SELECT_CALENDARPROPERTIES_BY_ID;
    int qsize = sizeof(SELECT_CALENDARPROPERTIES_BY_ID);
    rv = sqlite3_prepare_v2(mFormat->database(), query, qsize, &stmt, NULL);
    QCOMPARE(rv, 0);
    const QByteArray id(uid.toUtf8());
    rv = sqlite3_bind_text(stmt, 1, id.constData(), id.length(), SQLITE_STATIC);
    QCOMPARE(rv, 0);
    rv = sqlite3_step(stmt);
    QCOMPARE(rv, SQLITE_DONE);
}

#include "tst_sqliteformat.moc"
QTEST_MAIN(tst_sqliteformat)
