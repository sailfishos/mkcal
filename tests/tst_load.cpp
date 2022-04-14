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

#include <QObject>
#include <QTest>
#include <QDebug>

#include "extendedcalendar.h"
#include "extendedstorage.h"

using namespace mKCal;

class tst_load: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testAll();
    void testById();
    void testSeries();
    void testByInstanceIdentifier();
    void testByDate();
    void testRange();
    void testRange_data();

private:
    ExtendedStorage::Ptr mStorage;
    QString mCreatedNotebookUid;
};

void tst_load::init()
{
    ExtendedCalendar::Ptr calendar(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    mStorage = ExtendedCalendar::defaultStorage(calendar);
    mStorage->open();

    if (mStorage->defaultNotebookId().isEmpty()) {
        Notebook notebook({}, QString::fromLatin1("Default"), {}, {},
                          false, false, false, false, true);
        QVERIFY(mStorage->setDefaultNotebook(notebook));
        mCreatedNotebookUid = notebook.uid();
    }
}

void tst_load::cleanup()
{
    if (!mCreatedNotebookUid.isEmpty()) {
        QVERIFY(mStorage->deleteNotebook(mCreatedNotebookUid));
    }
    mStorage.clear();
}

void tst_load::testAll()
{
    int length0 = mStorage->calendar()->events().length();
    QVERIFY(mStorage->load());
    length0 = mStorage->calendar()->events().length() - length0;

    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(QDate(2022, 3, 14), QTime(11, 56)));
    QVERIFY(mStorage->calendar()->addEvent(event));
    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setDtStart(QDateTime(QDate(2022, 3, 14), QTime(11, 57)));
    QVERIFY(mStorage->calendar()->addEvent(event2));
    QVERIFY(mStorage->save());
    
    ExtendedCalendar::Ptr calendar(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    ExtendedStorage::Ptr storage = ExtendedCalendar::defaultStorage(calendar);
    QVERIFY(storage->open());

    QVERIFY(calendar->events().isEmpty());

    QVERIFY(storage->load());
    QCOMPARE(calendar->events().length() - length0, 2);
    QVERIFY(calendar->incidence(event->uid()));
    QVERIFY(calendar->incidence(event2->uid()));
    QVERIFY(storage->isRecurrenceLoaded());
    QDateTime start, end;
    QVERIFY(!storage->getLoadDates(QDate(), QDate(), &start, &end));
    QVERIFY(!storage->getLoadDates(QDate(2022, 3, 14), QDate(2022, 3, 15), &start, &end));

    QVERIFY(mStorage->calendar()->deleteIncidence(event2));
    QVERIFY(mStorage->calendar()->deleteIncidence(event));
    QVERIFY(mStorage->save(ExtendedStorage::PurgeDeleted));
}

void tst_load::testById()
{
    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
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

    QVERIFY(mStorage->calendar()->addEvent(event));
    QVERIFY(mStorage->calendar()->addEvent(occurrence));
    QVERIFY(mStorage->save());

    ExtendedCalendar::Ptr calendar(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    ExtendedStorage::Ptr storage = ExtendedCalendar::defaultStorage(calendar);
    QVERIFY(storage->open());

    QVERIFY(calendar->events().isEmpty());

    QVERIFY(storage->load(occurrence->uid(), occurrence->recurrenceId()));
    QCOMPARE(calendar->events().length(), 1);
    occurrence = calendar->event(occurrence->uid(),
                                 occurrence->recurrenceId());
    QVERIFY(occurrence);
    QVERIFY(calendar->deleteIncidence(occurrence));
    QVERIFY(calendar->events().isEmpty());
    QVERIFY(storage->load(occurrence->uid(), occurrence->recurrenceId()));
    QVERIFY(calendar->events().isEmpty());

    QVERIFY(storage->load(event->uid()));
    QCOMPARE(calendar->events().length(), 1);
    event = calendar->event(event->uid());
    QVERIFY(event);
    QVERIFY(calendar->deleteIncidence(event));

    QVERIFY(storage->save(ExtendedStorage::PurgeDeleted));
}

void tst_load::testSeries()
{
    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
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

    QVERIFY(mStorage->calendar()->addEvent(event));
    QVERIFY(mStorage->calendar()->addEvent(occurrence));
    QVERIFY(mStorage->calendar()->addEvent(single));
    QVERIFY(mStorage->save());

    ExtendedCalendar::Ptr calendar(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    ExtendedStorage::Ptr storage = ExtendedCalendar::defaultStorage(calendar);
    QVERIFY(storage->open());

    QVERIFY(calendar->events().isEmpty());

    QVERIFY(storage->loadSeries(event->uid()));
    QCOMPARE(calendar->events().length(), 2);
    QVERIFY(calendar->incidence(event->uid()));
    QVERIFY(calendar->incidence(occurrence->uid(), occurrence->recurrenceId()));

    QVERIFY(storage->loadSeries(single->uid()));
    QCOMPARE(calendar->events().length(), 3);
    QVERIFY(calendar->incidence(single->uid()));

    QVERIFY(mStorage->calendar()->deleteIncidence(event));
    QVERIFY(mStorage->calendar()->deleteIncidence(single));
    QVERIFY(mStorage->save(ExtendedStorage::PurgeDeleted));
}

void tst_load::testByInstanceIdentifier()
{
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

    QVERIFY(mStorage->calendar()->addEvent(event));
    QVERIFY(mStorage->calendar()->addEvent(occurrence));
    QVERIFY(mStorage->calendar()->addEvent(single));
    QVERIFY(mStorage->save());

    ExtendedCalendar::Ptr calendar(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    ExtendedStorage::Ptr storage = ExtendedCalendar::defaultStorage(calendar);
    QVERIFY(storage->open());

    QVERIFY(calendar->events().isEmpty());

    QVERIFY(storage->loadIncidenceInstance(occurrence->instanceIdentifier()));
    QVERIFY(calendar->instance(occurrence->instanceIdentifier()));

    QVERIFY(storage->loadIncidenceInstance(event->instanceIdentifier()));
    QVERIFY(calendar->instance(event->instanceIdentifier()));

    QVERIFY(storage->loadIncidenceInstance(single->instanceIdentifier()));
    QVERIFY(calendar->instance(single->instanceIdentifier()));

    QVERIFY(mStorage->calendar()->deleteIncidence(event));
    QVERIFY(mStorage->calendar()->deleteIncidence(single));
    QVERIFY(mStorage->save(ExtendedStorage::PurgeDeleted));
}

void tst_load::testByDate()
{
    // Will test loading events intersecting date.
    const QDate date(2022, 3, 14);

    int length0 = mStorage->calendar()->events().length();
    QVERIFY(mStorage->load(date));
    length0 = mStorage->calendar()->events().length() - length0;

    // Plain event within the day.
    KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
    event->setDtStart(QDateTime(date, QTime(11, 56), Qt::UTC));
    QVERIFY(mStorage->calendar()->addEvent(event));
    // Plain event the day after at 00:00.
    KCalendarCore::Event::Ptr event2(new KCalendarCore::Event);
    event2->setDtStart(QDateTime(date.addDays(1), QTime(), Qt::UTC));
    QVERIFY(mStorage->calendar()->addEvent(event2));
    // Recurring daily event, intersecting date.
    KCalendarCore::Event::Ptr event3(new KCalendarCore::Event);
    event3->setDtStart(QDateTime(date.addDays(-30), QTime(12, 07), Qt::UTC));
    event3->recurrence()->setDaily(1);
    QVERIFY(mStorage->calendar()->addEvent(event3));
    // Multi-day event intersecting day.
    KCalendarCore::Event::Ptr event4(new KCalendarCore::Event);
    event4->setDtStart(QDateTime(date.addDays(-2), QTime(), Qt::UTC));
    event4->setDtEnd(QDateTime(date.addDays(+2), QTime(), Qt::UTC));
    QVERIFY(mStorage->calendar()->addEvent(event4));
    // All day event at day.
    KCalendarCore::Event::Ptr event5(new KCalendarCore::Event);
    event5->setDtStart(QDateTime(date, QTime()));
    event5->setAllDay(true);
    QVERIFY(mStorage->calendar()->addEvent(event5));
    // Plain event happening another day, but intersecting in the calendar time zone.
    KCalendarCore::Event::Ptr event6(new KCalendarCore::Event);
    event6->setDtStart(QDateTime(date.addDays(1), QTime(0, 30), QTimeZone("Europe/Paris")));
    QVERIFY(mStorage->calendar()->addEvent(event6));
    // Recurring event defined with rdates.
    KCalendarCore::Event::Ptr event7(new KCalendarCore::Event);
    event7->setDtStart(QDateTime(date.addDays(-3), QTime(12, 0), Qt::UTC));
    event7->recurrence()->addRDateTime(QDateTime(date, QTime(9, 0), Qt::UTC));
    QVERIFY(mStorage->calendar()->addEvent(event7));

    QVERIFY(mStorage->save());
    
    ExtendedCalendar::Ptr calendar(new ExtendedCalendar(QTimeZone::utc()));
    ExtendedStorage::Ptr storage = ExtendedCalendar::defaultStorage(calendar);
    QVERIFY(storage->open());

    QVERIFY(calendar->events().isEmpty());

    QVERIFY(storage->load(date));
    QVERIFY(calendar->incidence(event->uid()));
    QVERIFY(calendar->incidence(event3->uid()));
    QVERIFY(calendar->incidence(event4->uid()));
    QVERIFY(calendar->incidence(event5->uid()));
    QVERIFY(calendar->incidence(event6->uid()));
    QVERIFY(calendar->incidence(event7->uid()));
    QCOMPARE(calendar->events().length() - length0, 6);
    QVERIFY(storage->isRecurrenceLoaded());
    QDateTime start, end;
    QVERIFY(!storage->getLoadDates(date, date.addDays(1), &start, &end));

    QVERIFY(mStorage->calendar()->deleteIncidence(event7));
    QVERIFY(mStorage->calendar()->deleteIncidence(event6));
    QVERIFY(mStorage->calendar()->deleteIncidence(event5));
    QVERIFY(mStorage->calendar()->deleteIncidence(event4));
    QVERIFY(mStorage->calendar()->deleteIncidence(event3));
    QVERIFY(mStorage->calendar()->deleteIncidence(event2));
    QVERIFY(mStorage->calendar()->deleteIncidence(event));
    QVERIFY(mStorage->save(ExtendedStorage::PurgeDeleted));
}

void tst_load::testRange_data()
{
    QTest::addColumn<QDate>("start");
    QTest::addColumn<QDate>("end");
    QTest::addColumn<bool>("shouldLoad");
    QTest::addColumn<QDateTime>("loadStart");
    QTest::addColumn<QDateTime>("loadEnd");

    QTest::newRow("non overlapping") << QDate(2022, 2, 16) << QDate(2022, 5, 8)
                                     << true
                                     << QDateTime(QDate(2022, 2, 16), {})
                                     << QDateTime(QDate(2022, 5, 8), {});
    QTest::newRow("overlapping before") << QDate(2022, 1, 1) << QDate(2022, 3, 16)
                                        << true
                                        << QDateTime(QDate(2022, 1, 11), {})
                                        << QDateTime(QDate(2022, 3, 16), {});
    QTest::newRow("overlapping after") << QDate(2022, 3, 14) << QDate(2022, 8, 22)
                                        << true
                                        << QDateTime(QDate(2022, 3, 14), {})
                                        << QDateTime(QDate(2022, 8, 20), {});
    QTest::newRow("including") << QDate(2022, 4, 14) << QDate(2022, 5, 22)
                               << true
                               << QDateTime(QDate(2022, 4, 14), {})
                               << QDateTime(QDate(2022, 5, 22), {});
    QTest::newRow("contained") << QDate(2022, 5, 8) << QDate(2022, 5, 11)
                               << false
                               << QDateTime()
                               << QDateTime();
    QTest::newRow("contained contiguous") << QDate(2022, 5, 8) << QDate(2022, 5, 18)
                                          << false
                                          << QDateTime()
                                          << QDateTime();
    QTest::newRow("open bounded") << QDate(2023, 5, 8) << QDate()
                                  << true
                                  << QDateTime(QDate(2023, 5, 8), {})
                                  << QDateTime();
    QTest::newRow("open bounded with overlap") << QDate(2022, 5, 8) << QDate()
                                               << true
                                               << QDateTime(QDate(2022, 5, 18), {})
                                               << QDateTime();
    QTest::newRow("open bounded loaded") << QDate() << QDate(2022, 1, 1)
                                         << false
                                         << QDateTime()
                                         << QDateTime();
    QTest::newRow("open bounded loaded with overlap") << QDate() << QDate(2022, 1, 13)
                                                      << true
                                                      << QDateTime(QDate(2022, 1, 11), {})
                                                      << QDateTime(QDate(2022, 1, 13), {});
}

void tst_load::testRange()
{
    QFETCH(QDate, start);
    QFETCH(QDate, end);
    QFETCH(bool, shouldLoad);
    QFETCH(QDateTime, loadStart);
    QFETCH(QDateTime, loadEnd);

    mStorage->clearLoaded();

    // Random ordering, to test that addLoadedRange() is doing
    // it correctly.
    mStorage->addLoadedRange(QDate(2022, 2, 5), QDate(2022, 2, 16));
    mStorage->addLoadedRange(QDate(2022, 8, 20), QDate(2022, 8, 22));
    mStorage->addLoadedRange(QDate(), QDate(2022, 1, 11));
    mStorage->addLoadedRange(QDate(2022, 5, 12), QDate(2022, 5, 18));
    mStorage->addLoadedRange(QDate(2022, 5, 8), QDate(2022, 5, 12));

    QDateTime lStart, lEnd;
    QCOMPARE(mStorage->getLoadDates(start, end, &lStart, &lEnd), shouldLoad);
    if (shouldLoad) {
        QCOMPARE(lStart, loadStart);
        QCOMPARE(lEnd, loadEnd);
    }
}

#include "tst_load.moc"
QTEST_MAIN(tst_load)
