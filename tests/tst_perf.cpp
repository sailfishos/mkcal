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
#include "sqlitestorage.h"

tst_perf::tst_perf(QObject *parent)
    : QObject(parent)
    , mDb(nullptr)
{
}

static const int N_EVENTS = 2000;

void tst_perf::initTestCase()
{
    QString dbFile = QString::fromLatin1(qgetenv("SQLITESTORAGEDB"));
    if (dbFile.isEmpty()) {
        mDb = new QTemporaryFile();
        mDb->open();
        dbFile = mDb->fileName();
    }
    ExtendedCalendar::Ptr cal(new ExtendedCalendar(QTimeZone::systemTimeZone()));
    m_storage = ExtendedStorage::Ptr(new SqliteStorage(cal, dbFile, true));
    mBackend = new SingleSqliteBackend(dbFile);
}

void tst_perf::cleanupTestCase()
{
    m_storage.clear();
    delete mBackend;
    if (mDb)
        QFile::remove(mDb->fileName() + ".changed");
    delete mDb;
}

void tst_perf::init()
{
    QVERIFY(m_storage->calendar()->rawEvents().isEmpty());
    QVERIFY(m_storage->open());
    QVERIFY(mBackend->open());
}

void tst_perf::cleanup()
{
    QVERIFY(mBackend->close());
    QVERIFY(m_storage->close());
    m_storage->calendar()->close();
    QVERIFY(m_storage->calendar()->rawEvents().isEmpty());
}

static KCalendarCore::Incidence::List generate()
{
    KCalendarCore::Incidence::List list;
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
            KCalendarCore::Incidence::Ptr exc = KCalendarCore::Calendar::createException(event, cur.addDays(7));
            list.append(exc);
            i += 1;
        }
        list.append(event);
    }
    return list;
}

void tst_perf::tst_save()
{
    QElapsedTimer clock;

    const KCalendarCore::Incidence::List list1 = generate();
    QCOMPARE(list1.count(), N_EVENTS);
    clock.start();
    for (const KCalendarCore::Incidence::Ptr &incidence : list1) {
        QVERIFY(m_storage->calendar()->addIncidence(incidence));
    }
    qDebug() << "ExtendedCalendar::addIncidence() rate " << float(clock.elapsed()) / N_EVENTS << "ms per event";
    clock.restart();
    QVERIFY(m_storage->save());
    qDebug() << "SqliteStorage::save() rate " << float(clock.elapsed()) / N_EVENTS << "ms per event";

    const QString notebookUid = m_storage->defaultNotebook()->uid();
    const KCalendarCore::Incidence::List list2 = generate();
    QCOMPARE(list2.count(), N_EVENTS);
    clock.restart();
    QVERIFY(mBackend->deferSaving());
    for (const KCalendarCore::Incidence::Ptr &incidence : list2) {
        QVERIFY(mBackend->addIncidence(notebookUid, *incidence));
    }
    QVERIFY(mBackend->commit());
    qDebug() << "SingleSqliteBackend::addIncidence() rate " << float(clock.elapsed()) / N_EVENTS << "ms per event";
}

void tst_perf::tst_load()
{
    QElapsedTimer clock;

    clock.start();
    QVERIFY(m_storage->load());
    if (mDb)
        // Expected not to be N_EVENTS in case of reading from a database with arbitrary content.
        QCOMPARE(m_storage->calendar()->rawEvents().count(), N_EVENTS);
    qDebug() << "SqliteStorage::load() rate " << float(clock.elapsed()) / m_storage->calendar()->rawEvents().count() << "ms per event";

    clock.restart();
    KCalendarCore::Incidence::List list;
    QVERIFY(mBackend->incidences(&list, m_storage->defaultNotebook()->uid()));
    if (mDb)
        // Expected not to be N_EVENTS in case of reading from a database with arbitrary content.
        QCOMPARE(list.count(), N_EVENTS);
    qDebug() << "SingleSqliteBackend::incidences() rate " << float(clock.elapsed()) / list.count() << "ms per event";
}

void tst_perf::tst_loadRange()
{
    QElapsedTimer clock;
    const QDate cur = QDateTime::currentDateTimeUtc().date();

    clock.start();
    QVERIFY(m_storage->load(cur.addDays(-2), cur.addDays(N_EVENTS * 2)));
    if (mDb)
        // Expected not to be N_EVENTS in case of reading from a database with arbitrary content.
        QCOMPARE(m_storage->calendar()->rawEvents().count(), N_EVENTS);
    qDebug() << "SqliteStorage::load(range) rate " << float(clock.elapsed()) / m_storage->calendar()->rawEvents().count() << "ms per event";

    clock.restart();
    QHash<QString, KCalendarCore::Incidence::List> hash;
    QVERIFY(mBackend->incidences(&hash, QDateTime(cur.addDays(-2)), QDateTime(cur.addDays(N_EVENTS * 2))));
    int count = 0;
    for (QHash<QString, KCalendarCore::Incidence::List>::ConstIterator it = hash.constBegin();
         it != hash.constEnd(); it++) {
        count += it.value().count();
    }
    if (mDb)
        // Expected not to be N_EVENTS in case of reading from a database with arbitrary content.
        QCOMPARE(count, N_EVENTS);
    qDebug() << "SingleSqliteBackend::incidences(range) rate " << float(clock.elapsed()) / count << "ms per event";
}

QTEST_GUILESS_MAIN(tst_perf)
