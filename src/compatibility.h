/*
  This file is part of the kcalcore library.

  Copyright (c) 2002 Cornelius Schumacher <schumacher@kde.org>
  Copyright (C) 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>
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
  This file is part of the API for handling calendar data and defines
  classes for managing compatibility between different calendar formats.

  Originally comes from KCal but it has been modified.

  @author Cornelius Schumacher \<schumacher@kde.org\>
  @author Reinhold Kainhofer \<reinhold@kainhofer.com\>
  @author Alvaro Manera \<alvaro.manera@nokia.com\>
*/

#ifndef MKCAL_COMPAT_P_H
#define MKCAL_COMPAT_P_H

#include <incidence.h>
#include "mkcal_export.h"

#include <QtCore/QtGlobal> // for Q_DISABLE_COPY()

class QDate;
class QString;

namespace mKCal {

class Compatibility;

/**
  @brief
  This class provides compatibility to older or broken calendar files.

*/
class MKCAL_EXPORT Compatibility
{
public:
    /**
      A shared pointer to a Compatibility object.
    */
    typedef QSharedPointer<Compatibility> Ptr;

    /**
      Select the type of Compatibility. Importing or Exporting
    */
    enum DirectionType {
        Import,   /**< Do changes when importing from a broken device*/
        Export,   /**< Do change to export to a broken device */
    };

    /**
      Select the type of Element to Fix.
    */
    enum FixType {
        FixAlarm,   /**<  Fix the alarms*/
        FixRecurrence,   /**< Fix the recurrences */
    };

    /**
      Constructor.
    */
    Compatibility();

    /**
      Destructor.
    */
    virtual ~Compatibility();

    /**
      Fixes all the possible errors on an incidence.
      @param incidence is a pointer to an Incidence object that may
      @param type if it is to import or to export
      need its recurrence rule fixed.
    */
    virtual void fixAll( const KCalCore::Incidence::Ptr &incidence, Compatibility::DirectionType type );

    /**
      Fixes one of the possible errors of an an incidence.
      @param element The element to fix
      @param incidence is a pointer to an Incidence object that may need
      @param type if it is to import or to export
      its summary fixed.
    */
    virtual void fixElement( Compatibility::FixType element, const KCalCore::Incidence::Ptr &incidence,
                             Compatibility::DirectionType type  );

    /**
      Standard trick to add virtuals later.

      @param id is any integer unique to this class which we will use to identify the method
             to be called.
      @param data is a pointer to some glob of data, typically a struct.
    */
    virtual void virtual_hook( int id, void *data );

private:
    //@cond PRIVATE
    Q_DISABLE_COPY( Compatibility )
    class Private;
    Private *d;
    //@endcond
};

/**
  @brief
  Factory for creating the right Compatibility object.

  @internal
*/
class MKCAL_EXPORT CompatibilityFactory
{
public:
    /**
      Creates the appropriate Compat class as determined by the Product ID.

      @param productId is a string containing a valid Product ID from
      a supported calendar format.
      @return A pointer to a Compat object which is owned by the caller.
    */
    static Compatibility::Ptr createCompatibility( const QString &productId );
};

/**
  @brief
  Compatibility class for old Nokia Phones

  Old Nokia Phones, Symbian and N900 only understand the AALARM so they have
  to be modified to show those alarms.
*/
class CompatNokiaPhones : public Compatibility
{
public:

    CompatNokiaPhones();

    ~CompatNokiaPhones();
    /**
      @copydoc
      Compatibility::fixAll()
    */
    virtual void fixAll( const KCalCore::Incidence::Ptr &incidence, Compatibility::DirectionType type );

    /**
      @copydoc
      Compatibility::fixElement()
    */
    virtual void fixElement( Compatibility::FixType element, const KCalCore::Incidence::Ptr &incidence,
                             Compatibility::DirectionType type  );

private:
    //@cond PRIVATE
    class Private;
    Private *d;
    //@endcond
};

}

#endif
