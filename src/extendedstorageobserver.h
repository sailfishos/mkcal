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
  defines the ExtendedStorageObserver to be used with ExtendedStorage.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Alvaro Manera \<alvaro.manera@nokia.com \>
*/

#ifndef MKCAL_STORAGEOBSERVER_H
#define MKCAL_STORAGEOBSERVER_H

#include <QString>


namespace mKCal {
class ExtendedStorage;

/**
   @class ExtendedStorageObserver

   The ExtendedStorageObserver class.
*/
class MKCAL_EXPORT ExtendedStorageObserver //krazy:exclude=dpointer
{
public:
    /**
       Destructor.
    */
    virtual ~ExtendedStorageObserver() {};

    /**
       Notify the Observer that a Storage has been modified.

       @param storage is a pointer to the ExtendedStorage object that
       is being observed.
       @param info uids inserted/updated/deleted, modified file etc.
    */
    virtual void storageModified(ExtendedStorage *storage, const QString &info);

    /**
       Notify the Observer that a Storage is executing an action.
       This callback is called typically for example every time
       an incidence has been loaded.

       @param storage is a pointer to the ExtendedStorage object that
       is being observed.
       @param info textual information
    */
    virtual void storageProgress(ExtendedStorage *storage, const QString &info);

    /**
       Notify the Observer that a Storage has finished an action.

       @param storage is a pointer to the ExtendedStorage object that
       is being observed.
       @param error true if action was unsuccessful; false otherwise
       @param info textual information
    */
    virtual void storageFinished(ExtendedStorage *storage, bool error, const QString &info);
};

};
#endif /* !MKCAL_STORAGEOBSERVER_H */
