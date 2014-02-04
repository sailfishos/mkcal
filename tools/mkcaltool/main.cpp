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

#include <QtCore/QCoreApplication>

#include "mkcaltool.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (argc == 4 && 0 == ::strcmp(argv[1], "--reset-alarms")) {
        QString notebookUid = argv[2];
        QString eventUid = argv[3];
        MkcalTool mkcalTool;
        exit(mkcalTool.resetAlarms(notebookUid, eventUid));
    }
    exit(0);
}
