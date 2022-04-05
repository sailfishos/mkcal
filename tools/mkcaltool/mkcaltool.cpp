/*
  This file is part of the mkcal library.

  Copyright (C) 2014 Jolla Ltd.
  Contact: Petri M. Gerdt <petri.gerdt@jollamobile.com>
  All rights reserved.

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
#include "mkcaltool.h"

#include <QtCore/QDebug>

// mkcal
#include <extendedcalendar.h>
#include <extendedstorage.h>

MkcalTool::MkcalTool()
{
}

int MkcalTool::resetAlarms(const QString &notebookUid, const QString &eventUid)
{
    mKCal::ExtendedCalendar::Ptr cal(new mKCal::ExtendedCalendar(QTimeZone::systemTimeZone()));
    mKCal::ExtendedStorage::Ptr storage = cal->defaultStorage(cal);
    storage->open();
    if (!storage->load(eventUid)) {
        qWarning() << "Unable to load event" << eventUid << "from notebook" << notebookUid;
        return 1;
    }
    KCalendarCore::Event::Ptr event = cal->event(eventUid);
    if (!event) {
        qWarning() << "Unable to fetch event" << eventUid << "from notebook" << notebookUid;
        return 1;
    }

    mKCal::StorageBackend::Collection update;
    update.insert(notebookUid, event.data());
    storage->storageUpdated({}, update, {});
    return 0;
}
