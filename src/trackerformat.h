/*
  This file is part of the mkcal library.

  Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
  Contact: Alvaro Manera <alvaro.manera@nokia.com>

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
#ifdef MKCAL_TRACKER_SUPPORT
/**
  @file
  This file is part of the API for handling calendar data and
  defines the TrackerFormat class.

  Deprecated!!

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/

#ifndef MKCAL_TRACKERFORMAT_H
#define MKCAL_TRACKERFORMAT_H

#include "mkcal_export.h"
#include "extendedstorage.h"

#include <incidence.h>

#include <QtCore/QHash>

class QDBusInterface;

namespace mKCal {

class TrackerStorage;

/**
  @brief
  Tracker format implementation.

  This class implements the Tracker format. It provides methods for
  loading/saving/converting Tracker format data into the internal
  representation as Calendar and Incidences.
*/
class MKCAL_EXPORT TrackerFormat : public QObject
{
  Q_OBJECT

  public:
    /**
      Constructs a new Tracker Format object.

      @param storage pointer to associated storage
      @param tracker tracker DBus interface
      @param synchronuousMode set true to wait for completion
    */
    TrackerFormat( TrackerStorage *storage, QDBusInterface *tracker,
                   bool synchronuousMode = false );

    /**
      Destructor.
    */
    virtual ~TrackerFormat();

    /**
      Cancel any ongoing action.
    */
    void cancel();

    /**
      Update incidences data in Components table.

      @param list list of incidences to update
      @param dbop database operation
      @return true if the insert was successful; false otherwise.
    */
    bool modifyComponents( QHash<KCalCore::Incidence::Ptr,QString> *list, DBOperation dbop );

    /**
      Update incidence data in Components table.

      @param incidence incidence to update
      @param dbop database operation
      @return true if the insert was successful; false otherwise.
    */
    bool modifyComponent( const KCalCore::Incidence::Ptr &incidence,
                          const QString &notebook, DBOperation dbop );

    /**
      Select incidences from Components and ComponentDetails tables.

      @param list pointer to result list
      @param start is the starting date
      @param end is the ending date
      @param dbop operation for after
      @param after select incidences after given datetime
      @param notebook select incidences from this notebook
      @param uid incidence uid
      @param incidence check for possible duplicates
      @return true if the select was successful; false otherwise.
    */
    bool selectComponents( QHash<KCalCore::Incidence::Ptr,QString> *list,
                           const QDate &start, const QDate &end,
                           DBOperation dbop, const KDateTime &after,
                           const QString &notebook, const QString &uid,
                           const KCalCore::Incidence::Ptr &incidence );

  private:
    //@cond PRIVATE
    Q_DISABLE_COPY( TrackerFormat )
    class Private;
    Private *const d;
    //@endcond
};

}

#endif

#endif //MKCAL_TRACKER_SUPPORT
