#ifndef MKCAL_SERVICEHANDLER_H
#define MKCAL_SERVICEHANDLER_H
/*
  This file is part of the libextendedkcal library.

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

#include <incidence.h>
#include "mkcal_export.h"
#include "notebook.h"
#include "extendedcalendar.h"
#include "extendedstorage.h"
#include "servicehandlerif.h"

const QString defaultName = "DefaultInvitationPlugin";

class ServiceHandlerPrivate;

namespace mKCal {


  /** Singleton class to get the exact handler (plugin) of the service
  */
  class MKCAL_EXPORT ServiceHandler
  {
  private:
    /** Constructor, is a singleton so you cannot do anything
      */
    ServiceHandler();

    /** Desctructor
      */
    ~ServiceHandler();

    ServiceHandlerPrivate* const d;

  public:

    /** Error Codes that can be returned by the plugins */
    //Right now they are the same as defined in ServiceHandlerIf
    //But semantically it doesn't make sense that they are defined
    //there and at some point they might be different.
    enum ErrorCode {
      ErrorOk = 0,
      ErrorNoAccount,
      ErrorNotSupported,
      ErrorNoConnectivity,
      ErrorInvalidParameters,
      ErrorInternal
    };

    /** Obtain an instance of the ServiceHandler.
      @return The instance that handles all the services
      */
    static ServiceHandler& instance()
    {
      static ServiceHandler singleton;
      return singleton;
    }

    /** Send the invitation to the list of people stated as attendees.
      It would load the appropiate plugin to do it, and if there
      is no plugin it would use the default fall back plugin.
      @param invitation The Incidence to send
      @param body The body of the reply if any
      @param calendar Pointer to the calendar in use
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
      */
    bool sendInvitation(const KCalCore::Incidence::Ptr &invitation, const QString &body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage);

    /** Send the updated invitation to the list of people stated as attendees.
      It would load the appropiate plugin to do it, and if there
      is no plugin it would use the default fall back plugin.
      @param invitation The Incidence to udpate
      @param body The body of the reply if any
      @param calendar Pointer to the calendar in use
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
      */
    bool sendUpdate(const KCalCore::Incidence::Ptr &invitation, const QString &body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage);

    /** Send the updated invitation to the organiser.
      It would load the appropiate plugin to do it, and if there
      is no plugin it would use the default fall back plugin.
      @param invitation The Incidence to udpate
      @param body The body of the reply if any
      @param calendar Pointer to the calendar in use
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
      */
    bool sendResponse(const KCalCore::Incidence::Ptr &invitation, const QString &body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage);

    /** Icon
      It would load the appropiate plugin to do it
      @param notebook notebook
      @param storage Pointer to the storage in use
      @return Icon
      */
    QIcon icon(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage);

    /** multiCalendar
      It would load the appropiate plugin to do it
      @param notebook notebook
      @param storage Pointer to the storage in use
      @return True if multicalendar otherwise false
      */
    bool multiCalendar(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage);

    /** emailAddress
      It would load the appropiate plugin to do it
      @param notebook notebook
      @param storage Pointer to the storage in use
      @return email address
      */
    QString emailAddress(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage);

    /** displayName
      It would load the appropiate plugin to do it
      @param notebook notebook
      @param storage Pointer to the storage in use
      @return display name
      */
    QString displayName(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage);

    /** downloadAttachment
      It would load the appropiate plugin to do it
      @param notebook notebook
      @param storage Pointer to the storage in use
      @param uri uri of attachment to be downloaded
      @param path path whre attachment to be downloaded to
      @return True if OK, false in case of error
      */
    bool downloadAttachment(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage, const QString &uri, const QString &path);

    /** deleteAttachment
      It would load the appropiate plugin to do it
      @param incience incidence of attachment to be deleted
      @param notebook notebook
      @param storage Pointer to the storage in use
      @param uri uri of attachment to be deleted
      @return True if OK, false in case of error
      */
    bool deleteAttachment(const KCalCore::Incidence::Ptr &incidence, const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage, const QString &uri);

    /** Share notebook
      It would load the appropiate plugin to do it
      @param notebook Shared notebook
      @param sharedWith The list of email addresses or phone numbers of users
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
      */
    bool shareNotebook(const Notebook::Ptr &notebook, const QStringList &sharedWith, const ExtendedStorage::Ptr &storage);

    /** sharedWith
      It would load the appropiate plugin to do it
      @param notebook notebook
      @param storage Pointer to the storage in use
      @return list of users to share with
      */
    QStringList sharedWith(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage);

    /** \brief In case of error, more detailed information can be provided
        Sometimes the true/false is not enough, so in case of false
        more details can be obtained.

        @param notebook notebook
        @param storage Pointer to the storage in use
        @return the ErrorCode of what happened
      */
    ServiceHandler::ErrorCode error() const;

  };

}
#endif // MKCAL_SERVICEHANDLER_H
