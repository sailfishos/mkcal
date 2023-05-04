/*
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
#include <QSignalSpy>

#include <KCalendarCore/Event>

#include "sqlitecalendarstorage.h"

#ifdef TIMED_SUPPORT
#include <timed-qt5/interface.h>
#include <QtCore/QMap>
#include <QtDBus/QDBusReply>
using namespace Maemo;
#endif

using namespace KCalendarCore;
using namespace mKCal;

class tst_sqlitecalendarstorage: public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testConstructorByUid();
    void testOpenClose();
    void testCalendarProperties();
    void testSaveLoad();
    void testLoadByUid();
    void testObserver();
    void testAlarms();
    void testRecurringAlarms();

private:
    void checkAlarms(const QSet<QDateTime> &alarms) const;

    SqliteCalendarStorage::Ptr mStorage;
};

void tst_sqlitecalendarstorage::init()
{
    MemoryCalendar::Ptr calendar(new MemoryCalendar(QTimeZone::systemTimeZone()));
    mStorage = SqliteCalendarStorage::Ptr(new SqliteCalendarStorage(calendar));
    QVERIFY(!mStorage->notebook());
    QVERIFY(mStorage->open());
    QVERIFY(mStorage->notebook());
}

void tst_sqlitecalendarstorage::cleanup()
{
    QVERIFY(mStorage->close());
    QVERIFY(!mStorage->notebook());
    mStorage.clear();
}

void tst_sqlitecalendarstorage::testConstructorByUid()
{
    SqliteCalendarStorage storage(mStorage->notebook()->uid());

    QCOMPARE(storage.calendar()->id(), mStorage->notebook()->uid());
    QVERIFY(storage.calendar().staticCast<MemoryCalendar>());
    QVERIFY(storage.open());
    QVERIFY(storage.close());
}

void tst_sqlitecalendarstorage::testOpenClose()
{
    // Case of an unsaved calendar.
    const QString id = mStorage->notebook()->uid();
    QVERIFY(mStorage->close());
    QVERIFY(!mStorage->notebook());
    QCOMPARE(mStorage->calendar()->id(), id);
    // Verify that closing more than one is a no-op.
    QVERIFY(mStorage->close());

    mStorage->calendar()->setName(QString::fromLatin1("Calendar name"));
    mStorage->calendar()->setAccessMode(ReadOnly);
    QVERIFY(mStorage->open());
    QVERIFY(mStorage->notebook());
    QCOMPARE(mStorage->notebook()->uid(), mStorage->calendar()->id());
    QCOMPARE(mStorage->notebook()->name(), mStorage->calendar()->name());
    QVERIFY(mStorage->notebook()->isReadOnly());
    // Verify that opening more than one is an error.
    QVERIFY(!mStorage->open());

    QVERIFY(mStorage->save());

    // Case of a saved calendar.
    const Notebook::Ptr old = mStorage->notebook();
    QVERIFY(mStorage->close());
    QVERIFY(!mStorage->notebook());
    QCOMPARE(mStorage->calendar()->id(), id);

    mStorage->calendar()->setName(QString::fromLatin1("Another calendar name"));
    mStorage->calendar()->setAccessMode(ReadWrite);
    QVERIFY(mStorage->open());
    QVERIFY(mStorage->notebook());
    QCOMPARE(mStorage->notebook()->uid(), mStorage->calendar()->id());
    QCOMPARE(mStorage->notebook()->name(), mStorage->calendar()->name());
    QVERIFY(mStorage->notebook()->isReadOnly());
    QCOMPARE(mStorage->calendar()->id(), old->uid());
    QCOMPARE(mStorage->calendar()->name(), old->name());
    QCOMPARE(mStorage->calendar()->accessMode(), ReadOnly);
}

void tst_sqlitecalendarstorage::testCalendarProperties()
{
    const Notebook::Ptr notebook = mStorage->notebook();
    QVERIFY(notebook);
    QCOMPARE(notebook->uid(), mStorage->calendar()->id());
    QCOMPARE(mStorage->calendar()->accessMode(), ReadWrite);

    notebook->setName(QString::fromLatin1("Calendar name"));
    notebook->setIsReadOnly(true);
    QVERIFY(mStorage->save());
    QCOMPARE(mStorage->calendar()->name(), notebook->name());
    QCOMPARE(mStorage->calendar()->accessMode(), ReadOnly);
}

void tst_sqlitecalendarstorage::testSaveLoad()
{
    Event::Ptr event(new Event);
    mStorage->calendar()->addIncidence(event);
    QVERIFY(mStorage->save());
    Event::Ptr event2(new Event);
    mStorage->calendar()->addIncidence(event2);
    Event::Ptr event3(new Event);
    mStorage->calendar()->addIncidence(event3);
    event3->recurrence()->setDaily(2);
    event3->setDtStart(QDateTime(QDate(2023, 5, 10), QTime(9,0)));
    Incidence::Ptr exception(event3->clone());
    exception->clearRecurrence();
    exception->setRecurrenceId(event3->dtStart().addDays(2));
    exception->setDtStart(QDateTime(QDate(2023, 5, 13), QTime(9,0)));
    mStorage->calendar()->addIncidence(exception);
    mStorage->calendar()->deleteIncidence(event);
    QVERIFY(mStorage->save());

    mStorage->calendar()->close();
    QVERIFY(mStorage->calendar()->incidences().isEmpty());

    Incidence::List deleted;
    QVERIFY(mStorage->deletedIncidences(&deleted));
    QCOMPARE(deleted.count(), 1);
    QCOMPARE(deleted[0]->uid(), event->uid());
    QVERIFY(mStorage->purgeDeletedIncidences(deleted));

    QVERIFY(mStorage->load());
    const Incidence::List list = mStorage->calendar()->incidences();
    QCOMPARE(list.count(), 3);
    int nFound = 0;
    for (const Incidence::Ptr &incidence : list) {
        if (incidence->uid() == event2->uid()) {
            nFound += 1;
        } else if (incidence->uid() == event3->uid()) {
            nFound += 1;
        } else if (incidence->uid() == exception->uid()) {
            nFound += 1;
        }
    }
    QCOMPARE(nFound, 3);

    mStorage->calendar()->deleteIncidence(event3);
    QVERIFY(mStorage->save(CalendarStorage::PurgeDeleted));
    deleted.clear();
    QVERIFY(mStorage->deletedIncidences(&deleted));
    QVERIFY(deleted.isEmpty());

    QVERIFY(mStorage->load());
    QCOMPARE(mStorage->calendar()->incidences().count(), 1);
    QCOMPARE(mStorage->calendar()->incidences()[0]->uid(), event2->uid());
}

void tst_sqlitecalendarstorage::testLoadByUid()
{
    Event::Ptr event(new Event);
    mStorage->calendar()->addIncidence(event);
    Event::Ptr event2(new Event);
    mStorage->calendar()->addIncidence(event2);
    Event::Ptr event3(new Event);
    mStorage->calendar()->addIncidence(event3);
    event3->recurrence()->setDaily(2);
    event3->setDtStart(QDateTime(QDate(2023, 5, 10), QTime(9,0)));
    Incidence::Ptr exception(event3->clone());
    exception->clearRecurrence();
    exception->setRecurrenceId(event3->dtStart().addDays(2));
    exception->setDtStart(QDateTime(QDate(2023, 5, 13), QTime(9,0)));
    mStorage->calendar()->addIncidence(exception);
    QVERIFY(mStorage->save());

    mStorage->calendar()->close();
    QVERIFY(mStorage->load(event->uid()));
    QVERIFY(mStorage->calendar()->incidence(event->uid()));
    QVERIFY(!mStorage->calendar()->incidence(event2->uid()));
    QVERIFY(!mStorage->calendar()->incidence(event3->uid()));

    QVERIFY(mStorage->load(event3->uid()));
    QVERIFY(mStorage->calendar()->incidence(event3->uid()));
    QVERIFY(mStorage->calendar()->incidence(event3->uid(), exception->recurrenceId()));
}

class TestStorageObserver: public QObject, public CalendarStorage::Observer
{
    Q_OBJECT
public:
    TestStorageObserver(CalendarStorage::Ptr storage): mStorage(storage)
    {
        mStorage->registerObserver(this);
    }
    ~TestStorageObserver()
    {
        mStorage->unregisterObserver(this);
    }

    void storageModified(CalendarStorage *storage) override
    {
        emit modified();
    }

    void storageUpdated(CalendarStorage *storage,
                        const Incidence::List &added,
                        const Incidence::List &modified,
                        const Incidence::List &deleted) override
    {
        emit updated(added, modified, deleted);
    }

signals:
    void modified();
    void updated(const KCalendarCore::Incidence::List &added,
                 const KCalendarCore::Incidence::List &modified,
                 const KCalendarCore::Incidence::List &deleted);

private:
    CalendarStorage::Ptr mStorage;
};

Q_DECLARE_METATYPE(KCalendarCore::Incidence::List);
void tst_sqlitecalendarstorage::testObserver()
{
    TestStorageObserver observer(mStorage);
    QSignalSpy updated(&observer, &TestStorageObserver::updated);
    QSignalSpy modified(&observer, &TestStorageObserver::modified);
    
    Event::Ptr event(new Event);
    mStorage->calendar()->addIncidence(event);
    QVERIFY(mStorage->save());
    QCOMPARE(updated.count(), 1);
    QList<QVariant> args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[0].value<KCalendarCore::Incidence::List>()[0]->uid(), event->uid());
    QVERIFY(args[1].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[2].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200)); // Even after 200ms the modified signal is not emitted.
    
    event->setSummary(QString::fromLatin1("Test event"));
    QVERIFY(mStorage->save());
    QCOMPARE(updated.count(), 1);
    args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QCOMPARE(args[1].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[1].value<KCalendarCore::Incidence::List>()[0]->uid(), event->uid());
    QVERIFY(args[0].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[2].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200)); // Even after 200ms the modified signal is not emitted.
    
    mStorage->calendar()->deleteIncidence(event);
    QVERIFY(mStorage->save());
    QCOMPARE(updated.count(), 1);
    args = updated.takeFirst();
    QCOMPARE(args.count(), 3);
    QCOMPARE(args[2].value<KCalendarCore::Incidence::List>().count(), 1);
    QCOMPARE(args[2].value<KCalendarCore::Incidence::List>()[0]->uid(), event->uid());
    QVERIFY(args[0].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(args[1].value<KCalendarCore::Incidence::List>().isEmpty());
    QVERIFY(modified.isEmpty());
    QVERIFY(!modified.wait(200)); // Even after 200ms the modified signal is not emitted.

    MemoryCalendar::Ptr calendar(new MemoryCalendar(QTimeZone::systemTimeZone()));
    SqliteCalendarStorage storage(calendar);
    QVERIFY(storage.open());
    Event::Ptr event2(new Event);
    event2->setSummary(QString::fromLatin1("New event added externally"));
    QVERIFY(calendar->addEvent(event2));
    QVERIFY(storage.save());
    QVERIFY(modified.wait());
    QVERIFY(updated.isEmpty());
}

void tst_sqlitecalendarstorage::testAlarms()
{
    const QDateTime dt = QDateTime::currentDateTimeUtc().addSecs(300);
    Event::Ptr ev(new Event);
    ev->setDtStart(dt);
    Alarm::Ptr alarm = ev->newAlarm();
    alarm->setDisplayAlarm(QLatin1String("Testing alarm"));
    alarm->setStartOffset(Duration(0));
    alarm->setEnabled(true);
    QVERIFY(mStorage->calendar()->addIncidence(ev));
    QVERIFY(mStorage->save());

#if defined(TIMED_SUPPORT)
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["notebook"] = mStorage->notebook()->uid();

    Timed::Interface timed;
    QVERIFY(timed.isValid());
    QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 1);
#endif

    QVERIFY(mStorage->calendar()->deleteIncidence(ev));
    QVERIFY(mStorage->save());

#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 0);
#endif

    mStorage->notebook()->setIsVisible(false);
    QVERIFY(mStorage->save());

    // Adding an event in a non visible notebook should not add alarm.
    QVERIFY(mStorage->calendar()->addIncidence(ev));
    QVERIFY(mStorage->save());
#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 0);
#endif

    // Clearing calendar to be in a situation where the calendar
    // object has just been created.
    mStorage->calendar()->close();

    // Switching the notebook to visible should activate all alarms.
    mStorage->notebook()->setIsVisible(true);
    QVERIFY(mStorage->save());
#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 1);
#endif

    // Switching the notebook to non visible should deactivate all alarms.
    mStorage->notebook()->setIsVisible(false);
    QVERIFY(mStorage->save());
#if defined(TIMED_SUPPORT)
    reply = timed.query_sync(map);
    QVERIFY(reply.isValid());
    QCOMPARE(reply.value().size(), 0);
#endif
}

void tst_sqlitecalendarstorage::checkAlarms(const QSet<QDateTime> &alarms) const
{
#if defined(TIMED_SUPPORT)
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["notebook"] = mStorage->notebook()->uid();

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

void tst_sqlitecalendarstorage::testRecurringAlarms()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime dt = QDateTime(now.date().addDays(1), QTime(12, 00));
    Event::Ptr ev(new Event);
    ev->setDtStart(dt);
    ev->recurrence()->setDaily(1);
    Alarm::Ptr alarm = ev->newAlarm();
    alarm->setDisplayAlarm(QLatin1String("Testing alarm"));
    alarm->setStartOffset(Duration(-600));
    alarm->setEnabled(true);
    QVERIFY(mStorage->calendar()->addIncidence(ev));
    QVERIFY(mStorage->save());

    // Simple recurring event
    checkAlarms(QSet<QDateTime>() << ev->dtStart());

    Incidence::Ptr exception = Calendar::createException(ev, ev->dtStart());
    exception->setDtStart(dt.addSecs(300));
    QVERIFY(mStorage->calendar()->addIncidence(exception));
    Incidence::Ptr exception2 = Calendar::createException(ev, ev->dtStart().addDays(5));
    exception2->setDtStart(dt.addDays(5).addSecs(300));
    QVERIFY(mStorage->calendar()->addIncidence(exception2));
    QVERIFY(mStorage->save());

    // Exception on the next occurrence, and second exception on the 5th occurence
    checkAlarms(QSet<QDateTime>() << exception->dtStart()
                << ev->dtStart().addDays(1) << exception2->dtStart());

    QVERIFY(mStorage->calendar()->deleteIncidence(exception));
    QVERIFY(mStorage->calendar()->deleteIncidence(exception2));
    QVERIFY(mStorage->save());

    // Exception was deleted
    checkAlarms(QSet<QDateTime>() << ev->dtStart());

    ev->recurrence()->addExDateTime(ev->dtStart());
    QVERIFY(mStorage->save());

    // exdate added
    checkAlarms(QSet<QDateTime>() << ev->dtStart().addDays(1));

    exception = Calendar::createException(ev, ev->dtStart().addDays(1));
    exception->setStatus(Incidence::StatusCanceled);
    QVERIFY(mStorage->calendar()->addIncidence(exception));
    QVERIFY(mStorage->save());

    // Cancelled next occurrence
    checkAlarms(QSet<QDateTime>() << ev->dtStart().addDays(2));

    exception = Calendar::createException(ev, ev->dtStart().addDays(4));
    exception->setSummary(QString::fromLatin1("Exception in the future."));
    QVERIFY(mStorage->calendar()->addIncidence(exception));
    QVERIFY(mStorage->save());

    // Adding an exception later than the next occurrence
    checkAlarms(QSet<QDateTime>() << exception->dtStart() << ev->dtStart().addDays(2));

    mStorage->notebook()->setIsVisible(false);
    QVERIFY(mStorage->save());

    // Alarms have been removed for non visible notebook.
    checkAlarms(QSet<QDateTime>());

    mStorage->notebook()->setIsVisible(true);
    QVERIFY(mStorage->save());

    // Alarms are reset when visible is turned on.
    checkAlarms(QSet<QDateTime>() << exception->dtStart() << ev->dtStart().addDays(2));
}

#include "tst_sqlitecalendarstorage.moc"
QTEST_MAIN(tst_sqlitecalendarstorage)
