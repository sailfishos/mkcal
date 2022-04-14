#ifndef SERVICEHANDLERIF_H
#define SERVICEHANDLERIF_H
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

#include <QtCore/QtPlugin>

#include "notebook.h"

class QString;

/**
  @file
  This file defines the common Interface to be inherited by all processes
  that handle service information

  @author Alvaro Manera \<alvaro.manera@nokia.com\>
*/

/** \brief Interface implemented by plugins for handling services.

    This plugins implement service specific things that are hidden form the
    application.

    @warning The class who implements this interface has to inherit also from QObject if the
    download of attachments on demand is supported.
    In that case then the plugin has to emit a signal downloadProgress(int percentage) to
    notify the application how the download is being done.
    */

class ServiceInterface
{

public:

    /** Error Codes that can be returned by the plugins */
    //Be aware that they are different to the ones in ServiceHandler
    //but they might need to be in sync
    enum ErrorCode {
        ErrorOk = 0,
        ErrorNoAccount,
        ErrorNotSupported,
        ErrorNoConnectivity,
        ErrorInvalidParameters,
        ErrorInternal
    };

    /** \brief returns icon of service.
        @return Path to the Icon.
    */
    virtual QString icon() const = 0;


    /** \brief returns name of service.
        @return Path to the Icon.
    */
    virtual QString uiName() const = 0;

    /** \brief is this service supporting multiple calendars.
        This name is something to be show to the user
        @return true is service supports multiple calendars otherwise false.
    */
    virtual  bool multiCalendar() const = 0;

    /** \brief returns the email address that is currently configured in the service,
    it can be different per account.
        @param notebook pointer to the notebook that we want to share
        @return email address of service
    */
    virtual QString emailAddress(const mKCal::Notebook &notebook) = 0;

    /** \brief returns the display name of account of service.
        @param notebook pointer to the notebook that we want to share
        @return display name of account of service
    */
    virtual QString displayName(const mKCal::Notebook &notebook) const = 0;

    /** \brief Start the download of an attachment.
        This cannot be a blocking operation.

        If used also the implementor of this class has to emit the following signals:

            - downloadProgress(int percentage) which will inform the application how the progress is doing.
            - downloadFinished(const QString &uri) to notify that the download is done
            - downloadError(const QString &uri) to notify that the download failed.

        There has to be a signal also to notify that the
        More than one download at a time can be started.


        @param notebook pointer to the notebook
        @param uri uri of attachment to be downloaded
        @param path path where attachment to be downloaded to
    @return True if OK, false otherwise.
    */
    virtual bool downloadAttachment(const mKCal::Notebook &notebook, const QString &uri, const QString &path) = 0;

    /** \brief start the deletion of an attachment.
        @param notebook pointer to the notebook
        @param incidence incidence of attachment to be deleted
        @param uri uri of attachment to be deleted
    @return True if OK, false otherwise.
    */
    virtual bool deleteAttachment(const mKCal::Notebook &notebook, const KCalendarCore::Incidence::Ptr &incidence,
                                  const QString &uri) = 0;

    /** \brief Share notebook.
        @param notebook pointer to the notebook that we want to share
        @param sharedWith email address or phone number of users
        @return True if OK, false otherwise.
    */
    virtual bool shareNotebook(const mKCal::Notebook &notebook, const QStringList &sharedWith) = 0;

    /** \brief Returns list of emails, phones# of the persons that a notebook is shared with.
        @param notebook pointer to the notebook
        @return list of email addresses or phone numbers
    */
    virtual QStringList sharedWith(const mKCal::Notebook &notebook) = 0;

    /** \brief The name of this service.
        It should be a uniq name specifying which service to use
        The service name is stored in the Calendars table (pluginname).
        @return The name of the service.
     */
    virtual QString serviceName() const = 0;

    /** \brief A service might have a default Notebook in the set of notebooks supported
        It can be a null value.
        If multi Calendar is supported, in some situations it might be required to select
        a default calendar from all of the ones supported. This function allows exactly that.
        @return The name of the service.
     */
    virtual QString defaultNotebook() const = 0;

    /** \brief Checks if a give Product Id obtained in an iCal file is handled by this plugin.
        In some situations special behaviour might be needed for invitation from certain
        providers. To check if this is the case, this function is used.

        For example it can be used to put it in the right notebook

        @param The string that was in the iCal file
        @return The true if it is from the service provider
     */
    virtual bool checkProductId(const QString &prodId) const = 0;

    /** \brief In case of error, more detailed information can be provided
        Sometimes the true/false is not enough, so in case of false
        more details can be obtained.
        @return the ErrorCode of what happened
      */
    virtual ErrorCode error() const = 0;

    virtual ~ServiceInterface() { }
};

Q_DECLARE_INTERFACE(ServiceInterface,
                    "com.nokia.Organiser.ServiceInterface/1.0")

#endif // SERVICEHANDLERIF_H
