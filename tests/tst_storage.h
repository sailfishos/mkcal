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
    void tst_vtimezone_data();
    void tst_vtimezone();
    void tst_allday_data();
    void tst_allday();
    void tst_alldayUtc();
    void tst_alldayRecurrence();
    void tst_origintimes();
    void tst_recurrence();
    void tst_recurrenceExpansion_data();
    void tst_recurrenceExpansion();
    void tst_rawEvents_data();
    void tst_rawEvents();
    void tst_rawEvents_nonRecur_data();
    void tst_rawEvents_nonRecur();
    void tst_dateCreated_data();
    void tst_dateCreated();
    void tst_lastModified();
    void tst_dissociateSingleOccurrence_data();
    void tst_dissociateSingleOccurrence();
    void tst_deleted();
    void tst_modified();
    void tst_inserted();
    void tst_icalAllDay_data();
    void tst_icalAllDay();
    void tst_deleteAllEvents();
    void tst_calendarProperties();
    void tst_alarms();

private:
    void openDb(bool clear = false);
    void reloadDb();

    ExtendedCalendar::Ptr m_calendar;
    ExtendedStorage::Ptr m_storage;
};

#endif
