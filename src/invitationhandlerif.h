#ifndef INVITATIONHANDLERIF_H
#define INVITATIONHANDLERIF_H
/*
  This file is part of the kcal library.

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

#include <QtPlugin>

#include <KCalendarCore/Incidence>

class QString;

/**
  @file
  This file defines the common Interface to be inherited by all processes
  that handle calendar invitations

  @author Alvaro Manera \<alvaro.manera@nokia.com\>
*/

/** \brief Interface implemented by plugins for handling invitations.

    The invitation is an icidence belonging to a Calendar which in turn contains an
    account field, that is a unique accountId to identify the account from the Accounts
    Subsystem. The Calendar also contains a plugin name. The named plugin, implementing
    this interface, will take care of the actual sending, using the account identified by
    the accountId.
    The user of this interface should take care of updating the invitation. The plugins should
    not modify the invitation object. The invitation is not const only for thecnical reasons.
    */
class InvitationHandlerInterface {

public:

    /** \brief Send a new Invitation to all the participants.

        @param accountId The unique id of the account
        @param notebookUid notebook uid of incidence
        @param invitation Pointer to the incidence that we want to send
        @param body The body of the reply, if any
        @return True if OK, false otherwise.
    */
  virtual bool sendInvitation(const QString &accountId, const QString &notebookUid,
                              const KCalendarCore::Incidence::Ptr &invitation, const QString &body) = 0;

    /** \brief Send a updated invitation to all the participants.
        This is used for updating invitations we sent earlier.

        @param accountId The unique id of the account
        @param invitation Pointer to the incidence that we want to send
        @param body The body of the reply, if any
        @return True if OK, false otherwise.
    */
  virtual bool sendUpdate(const QString &accountId, const KCalendarCore::Incidence::Ptr &invitation,
                          const QString &body) = 0;

    /** \brief Send the updated invitation back to the Organiser.
        The attendance values should have been updated earlier by the caller.

        @param accountId The unique id of the account
        @param invitation Pointer to the incidence that we want to send
        @param body The body of the reply, if any
        @return True if OK, false otherwise.
      */
  virtual bool sendResponse(const QString &accountId, const KCalendarCore::Incidence::Ptr &invitation,
                            const QString &body) = 0;

    /** \brief The name of this plugin.
        It should be a uniq name specifying which plugin to use for sending invitations.
        The plugin name is stored in the Calendars table.
        @return The name of the plugin.
     */
    virtual QString pluginName() const = 0;

    virtual ~InvitationHandlerInterface() { }
};

Q_DECLARE_INTERFACE(InvitationHandlerInterface,
                    "org.kde.Organizer.InvitationHanderInterface/1.0")

#endif // INVITATIONHANDLERIF_H
