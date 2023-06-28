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
#include <singlesqlitebackend_p.h>

MkcalTool::MkcalTool()
{
}

MkcalTool::~MkcalTool()
{
}

KCalendarCore::Incidence::List MkcalTool::incidencesWithAlarms(const QString &notebookUid,
                                                               const QString &uid)
{
    KCalendarCore::Incidence::List list;

    mKCal::SingleSqliteBackend storage;
    if (!storage.open()) {
        qWarning() << "Unable to open storage" << storage.databaseName();
        return list;
    }

    storage.incidences(&list, notebookUid, uid);

    return list;
}

int MkcalTool::resetAlarms(const QString &notebookUid, const QString &eventUid)
{
    return setupAlarms(notebookUid, eventUid) ? 0 : 1;
}
