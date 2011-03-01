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
/**
  @file
  This file is part of the API for handling calendar data and
  defines the TrackerModify class.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/
#ifndef MKCAL_TRACKERMODIFY_H
#define MKCAL_TRACKERMODIFY_H

#include "mkcal_export.h"
#include "extendedstorage.h"

#include <incidence.h>

namespace mKCal {

/**
  @brief
  TrackerModify implementation.

  This class implements the Tracker insert/update/delete queries.
*/
class MKCAL_EXPORT TrackerModify
{
  public:
    /**
      Constructs a new TrackerModify object.
    */
    TrackerModify();

    /**
      Destructor.
    */
    virtual ~TrackerModify();

    /**
      Update incidence data in Components table.

      @param incidence incidence to update
      @param dbop database operation
      @return true if the insert was successful; false otherwise.
    */
    bool queries( const KCalCore::Incidence::Ptr &incidence, DBOperation dbop,
                  QStringList &insertQuery, QStringList &deleteQuery, const QString &notebook );

    /**
      Query to notify tracker that an incidence was opened by the user.

      @param incidence Incidence that we want to mark as opened
      @param query The query generated if OK
      @return true if successful; false otherwise.
    */
    bool notifyOpen( const KCalCore::Incidence::Ptr &incidence, QStringList &query );

  private:
    //@cond PRIVATE
    Q_DISABLE_COPY( TrackerModify )
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond
};

}

#endif
