#ifndef MKCAL_SERVICEHANDLER_H
#define MKCAL_SERVICEHANDLER_H
/*
  This file is part of the libmkcal library.

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

#include <KCalendarCore/Incidence>
#include <QObject>
#include "mkcal_export.h"
#include "notebook.h"
#include "servicehandlerif.h"

const QString defaultName = "DefaultInvitationPlugin";

class ServiceHandlerPrivate;

namespace mKCal {

/** Singleton class to get the exact handler (plugin) of the service
    In case of API with a notebook argument, the plugin to be used is
    determined calling `Notebook::pluginName()`.
*/
class MKCAL_EXPORT ServiceHandler : QObject
{
    Q_OBJECT
private:
    /** Constructor, is a singleton so you cannot do anything
      */
    ServiceHandler();
    ~ServiceHandler();

    ServiceHandlerPrivate *const d;

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
    static ServiceHandler &instance()
    {
        static ServiceHandler singleton;
        return singleton;
    }

    /** Send the invitation to the list of people stated as attendees.
      @param notebook notebook to use for account info
      @param invitation The Incidence to send
      @param body The body of the reply if any
      @return True if OK, false in case of error
      */
    bool sendInvitation(const Notebook::Ptr &notebook, const KCalendarCore::Incidence::Ptr &invitation,
                        const QString &body);

    /** Send the updated invitation to the list of people stated as attendees.
      @param notebook notebook to use for account info
      @param invitation The Incidence to udpate
      @param body The body of the reply if any
      @return True if OK, false in case of error
      */
    bool sendUpdate(const Notebook::Ptr &notebook, const KCalendarCore::Incidence::Ptr &invitation,
                    const QString &body);

    /** Send the updated invitation to the organiser.
      @param notebook notebook to use for account info
      @param invitation The Incidence to udpate
      @param body The body of the reply if any
      @return True if OK, false in case of error
      */
    bool sendResponse(const Notebook::Ptr &notebook, const KCalendarCore::Incidence::Ptr &invitation,
                      const QString &body);

    /** Icon
      @param serviceId the name of the service to use
      @return Icon
      */
    QString icon(const QString &serviceId);

    /** multiCalendar
      @param serviceId the name of the service to use
      @return True if multicalendar otherwise false
      */
    bool multiCalendar(const QString &serviceId);

    /** emailAddress
       Retrieve the email address of the notebook.
     */
    QString emailAddress(const Notebook::Ptr &notebook);

    /** displayName
      @param notebook notebook
      @param storage Pointer to the storage in use
      @return display name
      */
    QString displayName(const Notebook::Ptr &notebook);

    /** downloadAttachment
      @param notebook notebook
      @param storage Pointer to the storage in use
      @param uri uri of attachment to be downloaded
      @param path path whre attachment to be downloaded to
      @return Id of the attachment download. It will be used to notify changes about it. If < 0
      there was an error.
      */
    int downloadAttachment(const Notebook::Ptr &notebook, const QString &uri, const QString &path);

    /** deleteAttachment
      @param incience incidence of attachment to be deleted
      @param notebook notebook
      @param storage Pointer to the storage in use
      @param uri uri of attachment to be deleted
      @return True if OK, false in case of error
      */
    bool deleteAttachment(const KCalendarCore::Incidence::Ptr &incidence, const Notebook::Ptr &notebook,
                          const QString &uri);

    /** Share notebook
      @param notebook Shared notebook
      @param sharedWith The list of email addresses or phone numbers of users
      @param storage Pointer to the storage in use
      @return True if OK, false in case of error
      */
    bool shareNotebook(const Notebook::Ptr &notebook, const QStringList &sharedWith);

    /** sharedWith
      @param notebook notebook
      @param storage Pointer to the storage in use
      @return list of users to share with
      */
    QStringList sharedWith(const Notebook::Ptr &notebook);

    /** Try to get the notebook where to put the invitation.
      This is done based on the product Id of the invitation received. (in the iCal file).

      @param productId the id of the generator of the iCal
      @return a string with the id of the notebook. it can be null
      */
    QString defaultNotebook(const QString &productId);

    /** \brief In case of error, more detailed information can be provided
        Sometimes the true/false is not enough, so in case of false
        more details can be obtained.

        @param notebook notebook
        @param storage Pointer to the storage in use
        @return the ErrorCode of what happened
      */
    ServiceHandler::ErrorCode error() const;


    ///MultiCalendar services

    /** \brief List available Services
         There can be many available services. This method returns the ids of the plugins that handle
         those services.
         @note this id can be used in the Notebook creation to "attach" a notebook to a certain
         service.
         @return list of the ids of the plugins available
       */
    QStringList availableServices();

    /** \brief Get the Icon of a service based on the id of the plugin

      @return Path to the icon
      @see availableMulticalendarServices

      */
    QString icon(QString serviceId);

    /** \brief Get the Name to be shown on the UI of a service based on the id of the plugin

      @return Name of the service
      @see availableMulticalendarServices

      */
    QString uiName(QString serviceId);

    /** \brief Get the plugin object providing the service.

      @return the plugin object
      @see availableMulticalendarServices
      */
    ServiceInterface* service(const QString &serviceId);

signals:
    /** Monitors the progress of the download. The id is the return value got when download started */
    void downloadProgress(int id, int percentage);

    /** Informs that the download is over. The id is the return value got when download started */
    void downloadFinished(int id);

    /** Informs that the download is finished with errors. The id is the return value got when download started */
    void downloadError(int id, ErrorCode error);
};

}
#endif // MKCAL_SERVICEHANDLER_H
