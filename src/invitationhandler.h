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
  This file is part of the API for handling invitations
  in a common way, no matter the service, transport, etc.

  @author Alvaro Manera \<alvaro.manera@nokia.com \>
*/
#ifndef MKCAL_INVITATIONHANDLER_H
#define MKCAL_INVITATIONHANDLER_H

#include "mkcal_export.h"
#include "extendedcalendar.h"
#include "extendedstorage.h"
#include "notebook.h"

#include <incidence.h>

#include <QtCore/QString>

const QString defaultName = "DefaultInvitationPlugin";

class InvitationHandlerPrivate;

namespace mKCal {

class ExtendedStorage;

/** Singleton class to get the exact handler (plugin) of the invitation

  @author Alvaro Manera \<alvaro.manera@nokia.com \>
  */
class MKCAL_EXPORT InvitationHandler
{
  private:
    /**
      Constructor, is a singleton so you cannot do anything
    */
    InvitationHandler();

    /**
      Destructor
    */
    ~InvitationHandler();

    InvitationHandlerPrivate *const d;

  public:

    /**
      Obtain an instance of the PluginHandler.
      @return The instance that handles all the InvitationPlugins
    */
    static InvitationHandler &instance()
    {
      static InvitationHandler singleton;
      return singleton;
    }

    /** Send the invitation to the list of people stated as attendees.
      It would load the appropriate plugin to do it, and if there
      is no plugin it would use the default fall back plugin.
      @param invitation The Incidence to send
      @param body The body of the reply if any
      @param calendar Pointer to the calendar in use
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
    */
    bool sendInvitation( const KCalCore::Incidence::Ptr &invitation, const QString &body,
                         const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage );

    /** Send the updated invitation to the list of people stated as attendees.
      It would load the appropriate plugin to do it, and if there
      is no plugin it would use the default fall back plugin.
      @param invitation The Incidence to udpate
      @param body The body of the reply if any
      @param calendar Pointer to the calendar in use
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
    */
    bool sendUpdate( const KCalCore::Incidence::Ptr &invitation, const QString &body,
                     const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage );

    /** Send the updated invitation to the organiser.
      It would load the appropriate plugin to do it, and if there
      is no plugin it would use the default fall back plugin.
      @param invitation The Incidence to udpate
      @param body The body of the reply if any
      @param calendar Pointer to the calendar in use
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
    */
    bool sendResponse( const KCalCore::Incidence::Ptr &invitation, const QString &body,
                       const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage );

    /** Share notebook
      It would load the appropriate plugin to do it
      @param notebook Shared notebook
      @param sharedWith The email address or phone number of user
      @param calendar Pointer to the calendar in use
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
    */
    bool shareNotebook( const Notebook::Ptr &notebook, const QStringList &sharedWith,
                        const ExtendedStorage::Ptr &storage );
};

}
#endif

