/* * This file is part of Harmattan Organiser GUI *
 *
 * Copyright (C) 2009-2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * Contact: Andrey Moiseenko <andrey.moiseenko@nokia.com>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved. Copying,
 * including reproducing, storing, adapting or translating, any or all of
 * this material requires the prior written consent of Nokia Corporation.
 * This material also contains confidential information which may not be
 * disclosed to others without the prior written consent of Nokia.
 */

#ifndef DEFAULTINVITATIONPLUGIN_H
#define DEFAULTINVITATIONPLUGIN_H

#include <invitationhandlerif.h>
#include "servicehandlerif.h"
#include <QtCore/QObject>

using namespace KCalendarCore;

/**
 * \brief DefaultInvitationPlugin class.
 *
 *  When no other plugin has been found for a particular notebook, then this plugin
 *  will try to send the invitation using QMF's "default account" or using account id
 *  of the account if it supports an email service.
 */
class DefaultInvitationPlugin : public QObject, public InvitationHandlerInterface, public ServiceInterface
{
    Q_OBJECT
    Q_INTERFACES(InvitationHandlerInterface)
    Q_INTERFACES(ServiceInterface)
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.mkcal.DefaultInvitationHandlerInterface")

public:
    DefaultInvitationPlugin();
    ~DefaultInvitationPlugin();

    //! \reimp InvitationHandler KCalendarCore
    bool sendInvitation(const QString &accountId, const QString &notebookId, const Incidence::Ptr &invitation,
                        const QString &body);
    bool sendUpdate(const QString &accountId, const Incidence::Ptr &invitation, const QString &body);
    bool sendResponse(const QString &accountId, const Incidence::Ptr &invitation, const QString &body);
    QString pluginName() const;
    //! \reimp_end

    //! \reimp ServiceHandler mKCal
    QString icon() const;

    QString uiName() const;

    bool multiCalendar() const;

    QString emailAddress(const mKCal::Notebook &notebook);

    QString displayName(const mKCal::Notebook &notebook) const;

    bool downloadAttachment(const mKCal::Notebook &notebook, const QString &uri, const QString &path);

    bool deleteAttachment(const mKCal::Notebook &notebook, const Incidence::Ptr &incidence, const QString &uri);

    bool shareNotebook(const mKCal::Notebook &notebook, const QStringList &sharedWith);

    QStringList sharedWith(const mKCal::Notebook &notebook);

    QString serviceName() const;

    QString defaultNotebook() const;

    bool checkProductId(const QString &prodId) const;

    ErrorCode error() const;

    //! \reimp_end

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(DefaultInvitationPlugin)
    class Private;
    Private *const d;
    //@endcond
};

#endif
