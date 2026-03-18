/*
  Copyright (c) 2026 Guido Berhoerster <guido+mkcal@berhoerster.name>

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

#ifndef MKCAL_COMPAT_H
#define MKCAL_COMPAT_H

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
     #define QDATETIME_CTOR_LOCAL_TZ (QTimeZone::LocalTime)
     #define QDATETIME_CTOR_UTC_TZ (QTimeZone::UTC)
#else
    #define QDATETIME_CTOR_LOCAL_TZ (Qt::LocalTime)
    #define QDATETIME_CTOR_UTC_TZ (Qt::UTC)
#endif

#endif
