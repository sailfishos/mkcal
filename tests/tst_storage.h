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
    void tst_origintimes();
    void tst_recurrence();
    void tst_rawEvents_data();
    void tst_rawEvents();
    void tst_dateCreated_data();
    void tst_dateCreated();
    void tst_dissociateSingleOccurrence_data();
    void tst_dissociateSingleOccurrence();
    void tst_deleted();
    void tst_modified();
    void tst_inserted();
    void tst_icalAllDay_data();
    void tst_icalAllDay();

private:
    void openDb(bool clear = false);
    void reloadDb();

    ExtendedCalendar::Ptr m_calendar;
    ExtendedStorage::Ptr m_storage;
};

#endif
