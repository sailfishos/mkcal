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

#include <QTest>
#include <QDebug>
#include <QElapsedTimer>
#include <QTemporaryFile>

#include "tst_perf.h"
#include "extendedstorage.h"

tst_perf::tst_perf(QObject *parent)
    : QObject(parent)
    , db(nullptr)
{
}

static const int N_EVENTS = 200;

void tst_perf::initTestCase()
{
    QString dbFile = QString::fromLatin1(qgetenv("SQLITESTORAGEDB"));
    if (dbFile.isEmpty()) {
        db = new QTemporaryFile();
        db->open();
        dbFile = db->fileName();
    }
    ExtendedCalendar::Ptr cal(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = ExtendedStorage::Ptr(new ExtendedStorage(cal, dbFile, true));
}

void tst_perf::cleanupTestCase()
{
    m_storage.clear();
    if (db)
        QFile::remove(db->fileName() + ".changed");
    delete db;
}

void tst_perf::init()
{
    QVERIFY(m_storage->calendar()->rawEvents().isEmpty());
    QVERIFY(m_storage->open());
}

void tst_perf::cleanup()
{
    QVERIFY(m_storage->close());
    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->rawEvents().isEmpty());
}

void tst_perf::tst_save()
{
    QElapsedTimer clock;

    clock.start();
    // Create arbitrary incidences in the DB.
    for (int i = 0; i < N_EVENTS; i++) {
        const QDateTime cur = QDateTime::currentDateTimeUtc();
        KCalendarCore::Event::Ptr event(new KCalendarCore::Event);
        event->setDtStart(cur.addDays(i));
        event->setDtEnd(event->dtStart().addSecs(60 * (i + 1)));
        event->setSummary(QString::fromLatin1("summary"));
        event->setNonKDECustomProperty("X-FOO", QString::fromLatin1("a property value"));
        event->setCustomProperty("VOLATILE", "BAR", QString::fromLatin1("another property value"));
        if (i%3 == 0) {
            KCalendarCore::Alarm::Ptr alarm = event->newAlarm();
            alarm->setDisplayAlarm(QString::fromLatin1("Driiiiing"));
        }
        if (i%5 == 0) {
            event->recurrence()->setWeekly(1, cur.date().dayOfWeek());
            event->recurrence()->setEndDateTime(event->dtEnd().addDays((i + 5) * 7));
            KCalendarCore::Incidence::Ptr exc = m_storage->calendar()->createException(event, cur.addDays(7));
            QVERIFY(m_storage->calendar()->addIncidence(exc));
            i += 1;
        }
        QVERIFY(m_storage->calendar()->addIncidence(event));
    }
    QCOMPARE(m_storage->calendar()->rawEvents().count(), N_EVENTS);
    QVERIFY(m_storage->save());
    qDebug() << "SqliteStorage::save() rate " << float(clock.elapsed()) / N_EVENTS << "ms per event";
}

void tst_perf::tst_load()
{
    QElapsedTimer clock;

    clock.start();
    QVERIFY(m_storage->load());
    if (db)
        // Expected not to be N_EVENTS in case of reading from a database with arbitrary content.
        QCOMPARE(m_storage->calendar()->rawEvents().count(), N_EVENTS);
    qDebug() << "SqliteStorage::load() rate " << float(clock.elapsed()) / m_storage->calendar()->rawEvents().count() << "ms per event";
}

void tst_perf::tst_loadRange()
{
    QElapsedTimer clock;
    const QDate cur = QDateTime::currentDateTimeUtc().date();

    clock.start();
    QVERIFY(m_storage->load(cur.addDays(-2), cur.addDays(N_EVENTS * 2)));
    if (db)
        // Expected not to be N_EVENTS in case of reading from a database with arbitrary content.
        QCOMPARE(m_storage->calendar()->rawEvents().count(), N_EVENTS);
    qDebug() << "SqliteStorage::load(range) rate " << float(clock.elapsed()) / m_storage->calendar()->rawEvents().count() << "ms per event";
}

QTEST_GUILESS_MAIN(tst_perf)
