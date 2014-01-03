#ifndef TST_STORAGE_H
#define TST_STORAGE_H

#include <QObject>

#include "extendedcalendar.h"
#include "extendedstorage.h"

using namespace mKCal;

class tst_storage: public QObject
{
  Q_OBJECT

public:
  explicit tst_storage(QObject *parent = 0);

private slots:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void cleanup();

  void tst_timezone();
  void tst_allday_data();
  void tst_allday();
  void tst_alldayUtc();
  void tst_alldayRecurrence();

private:
  void openDb(bool clear = false);
  void reloadDb();

  ExtendedCalendar::Ptr m_calendar;
  ExtendedStorage::Ptr m_storage;
};

#endif
