#include <QTest>
#include <QDebug>

#include "tst_storage.h"
#include "sqlitestorage.h"


// random
const char * const NotebookId("12345678-9876-1111-2222-222222222222");


tst_storage::tst_storage(QObject *parent)
  : QObject(parent)
{
}

void tst_storage::initTestCase()
{
  openDb(true);
}

void tst_storage::cleanupTestCase()
{
  mKCal::Notebook::Ptr notebook = m_storage->notebook(NotebookId);
  m_storage->deleteNotebook(notebook);
}

void tst_storage::init()
{
}

void tst_storage::cleanup()
{
}

void tst_storage::tst_timezone()
{
  // for test sanity, verify kdatetime actually agrees timezone is for helsinki.
  // TZ environment variable and such normal methods are, of course, not supported.
  // in case this fails, might want to write Europe/Helsinki to /etc/timezone
  KDateTime localTime(QDate(2014, 1, 1));
  qDebug() << "local time offset:" << localTime.utcOffset();
  QCOMPARE(localTime.utcOffset(), 7200);
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
  auto event = KCalCore::Event::Ptr(new KCalCore::Event);

  QFETCH(QDate, startDate);
  QFETCH(int, days);

  event->setDtStart(KDateTime(startDate, QTime(), KDateTime::ClockTime));
  event->setAllDay(true);
  if (days) {
    event->setDtEnd(KDateTime(startDate.addDays(days), QTime(), KDateTime::ClockTime));
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
  reloadDb();

  auto fetchedEvent = m_calendar->event(uid);
  QVERIFY(fetchedEvent.data());
  QCOMPARE(fetchedEvent->allDay(), true);
  QCOMPARE(fetchedEvent->dtStart().date(), startDate);
  QTime time = fetchedEvent->dtStart().time();
  QVERIFY(time == QTime() || time == QTime(0, 0));

  QTime localTime = fetchedEvent->dtStart().toLocalZone().time();
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
  auto event = KCalCore::Event::Ptr(new KCalCore::Event);
  QDate startDate(2013, 12, 1);
  event->setDtStart(KDateTime(startDate, QTime(), KDateTime::UTC));
  event->setAllDay(true);
  event->setSummary("test event utc");

  QCOMPARE(event->allDay(), true);

  m_calendar->addEvent(event, NotebookId);
  m_storage->save();
  QString uid = event->uid();
  reloadDb();

  auto fetchedEvent = m_calendar->event(uid);
  QVERIFY(fetchedEvent.data());
  QVERIFY(fetchedEvent->dtStart().isUtc());

  KDateTime localStart = fetchedEvent->dtStart().toLocalZone();
  QVERIFY(localStart.time() == QTime(2, 0));

  KDateTime localEnd = fetchedEvent->dtEnd().toLocalZone();
  QVERIFY(localEnd.time() == QTime(2, 0));

  QCOMPARE(localEnd.date(), localStart.date().addDays(1));
}

void tst_storage::tst_alldayRecurrence()
{
  auto event = KCalCore::Event::Ptr(new KCalCore::Event);

  QDate startDate(2013, 12, 1);
  event->setDtStart(KDateTime(startDate, QTime(), KDateTime::ClockTime));
  event->setAllDay(true);

  KCalCore::Recurrence *recurrence = event->recurrence();
  recurrence->setWeekly(1);
  recurrence->setStartDateTime(event->dtStart());

  m_calendar->addEvent(event, NotebookId);
  m_storage->save();
  QString uid = event->uid();
  reloadDb();

  auto fetchEvent = m_calendar->event(uid);
  QVERIFY(fetchEvent);
  KCalCore::Recurrence *fetchRecurrence = fetchEvent->recurrence();
  QVERIFY(fetchRecurrence);
  KDateTime match = recurrence->getNextDateTime(KDateTime(startDate));
  QCOMPARE(match, KDateTime(startDate.addDays(7), QTime(), KDateTime::ClockTime));
}

void tst_storage::tst_origintimes()
{
  SqliteStorage *ss = dynamic_cast<SqliteStorage*>(m_storage.data());
  QVERIFY(ss);

  KDateTime utcTime(QDate(2014, 1, 15), QTime(), KDateTime::UTC);
  KDateTime clockTime(QDate(2014, 1, 15), QTime(), KDateTime::ClockTime);
  KDateTime localTime(QDate(2014, 1, 15), QTime(), KDateTime::LocalZone);

  // local origin time is the same as specific time set to utc
  // note: currently origin time of clock time is saved as time in current time zone.
  // that does not necessarily make sense, but better be careful when changing behavior there.
  QCOMPARE(ss->toOriginTime(utcTime), ss->toLocalOriginTime(utcTime));
  QCOMPARE(ss->toLocalOriginTime(clockTime), ss->toLocalOriginTime(utcTime));
  QCOMPARE(ss->toLocalOriginTime(localTime), ss->toLocalOriginTime(utcTime));
}

void tst_storage::openDb(bool clear)
{
  m_calendar = ExtendedCalendar::Ptr(new ExtendedCalendar(KDateTime::Spec::LocalZone()));
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
  }

  m_storage->loadNotebookIncidences(NotebookId);
}

void tst_storage::reloadDb()
{
  m_storage.clear();
  m_calendar.clear();
  openDb();
}


QTEST_GUILESS_MAIN(tst_storage)
