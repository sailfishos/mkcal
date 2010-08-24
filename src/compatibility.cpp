/*
  This file is part of the kcalcore library.

  Copyright (c) 2002 Cornelius Schumacher <schumacher@kde.org>
  Copyright (C) 2003-2004 Reinhold Kainhofer <reinhold@kainhofer.com>

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

  @brief
  Classes that provide compatibility to older or "broken" calendar formats.

  @author Cornelius Schumacher \<schumacher@kde.org\>
  @author Reinhold Kainhofer \<reinhold@kainhofer.com\>
*/

#include "compatibility.h"
#include <incidence.h>

#include <kdebug.h>

#include <QtCore/QRegExp>
#include <QtCore/QString>

using namespace mKCal;

Compatibility::Ptr CompatibilityFactory::createCompatibility( const QString &productId )
{
  Compatibility::Ptr compat;

  int symbian = productId.indexOf( "Symbian" );
  int maemo = productId.indexOf( "N900" ); //So far they suffer the same problems :)

  if ( symbian >= 0 || maemo >= 0) {
    kDebug() << "Generating compatibility for old Nokia Phones";
    compat = Compatibility::Ptr  ( new CompatNokiaPhones );
  }

  if ( !compat ) {
    compat = Compatibility::Ptr ( new Compatibility );
  }

  return compat;
}

Compatibility::Compatibility()
{
}

Compatibility::~Compatibility()
{
}

void Compatibility::fixAll( const KCalCore::Incidence::Ptr &incidence, Compatibility::DirectionType type )
{
  Q_UNUSED( incidence );
  Q_UNUSED( type );
}

void Compatibility::fixElement( Compatibility::FixType element, const KCalCore::Incidence::Ptr &incidence, Compatibility::DirectionType type  )
{
  Q_UNUSED( incidence );
  Q_UNUSED( type );
  Q_UNUSED( element );
}


class CompatNokiaPhones::Private {

public:

  void fixExportAlarms( const KCalCore::Incidence::Ptr &incidence )
  {
    KCalCore::Alarm::List alarms = incidence->alarms();
    KCalCore::Alarm::List::Iterator it;
    for ( it = alarms.begin(); it != alarms.end(); ++it ) {
      (*it)->setType(KCalCore::Alarm::Audio);
    }
  }

  void fixImportAlarms( const KCalCore::Incidence::Ptr &incidence )
  {
    KCalCore::Alarm::List alarms = incidence->alarms();
    KCalCore::Alarm::List::Iterator it;
    for ( it = alarms.begin(); it != alarms.end(); ++it ) {
      (*it)->setType(KCalCore::Alarm::Display);
    }
  }


};

CompatNokiaPhones::CompatNokiaPhones()
{
  d = new CompatNokiaPhones::Private();
}

CompatNokiaPhones::~CompatNokiaPhones()
{
  delete d;
}

void CompatNokiaPhones::fixAll( const KCalCore::Incidence::Ptr &incidence, DirectionType type )
{
  if ( !incidence ) {
    return;
  }

  fixElement( Compatibility::FixAlarm, incidence, type ); //Here the new ones should be added
}

void CompatNokiaPhones::fixElement( Compatibility::FixType element, const KCalCore::Incidence::Ptr &incidence, Compatibility::DirectionType type  )
{
  if ( !incidence ) {
    return;
  }

  if (type == Import) {
    d->fixImportAlarms( incidence );
  } else {
    d->fixExportAlarms( incidence );
  }
}
