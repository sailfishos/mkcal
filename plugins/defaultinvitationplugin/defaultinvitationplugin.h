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

using namespace KCalCore;

/**
 * \brief DefaultInvitationPlugin class.
 *
 *  When no other plugin has been found for a particular notebook, then this plugin
 *  will try to send the invitation using QMF's "default account".
 *  Therefore this plugin, unlike others, will not have to use the accountId.
 */
class DefaultInvitationPlugin : public QObject, public InvitationHandlerInterface, public ServiceInterface
{
    Q_OBJECT
    Q_INTERFACES(InvitationHandlerInterface)
    Q_INTERFACES(ServiceInterface)

public:
    //! \brief DefaultInvitationPlugin constructor class.
    DefaultInvitationPlugin();

    //! \brief DefaultInvitationPlugin destructor class.
    ~DefaultInvitationPlugin();

    //! \reimp InvitationHandler KCalCore
    bool sendInvitation(const QString &accountId, const QString &notebookId, const Incidence::Ptr &invitation, const QString &body);
    bool sendUpdate(const QString &accountId, const Incidence::Ptr &invitation, const QString &body);
    bool sendResponse(const QString &accountId, const Incidence::Ptr &invitation, const QString &body);
    QString pluginName() const;
    //! \reimp_end

    //! \reimp ServiceHandler mKCal
    QIcon icon() const;

    bool multiCalendar() const;

    QString emailAddress(const mKCal::Notebook::Ptr &notebook);

    QString displayName(const mKCal::Notebook::Ptr &notebook) const;

    bool downloadAttachment(const mKCal::Notebook::Ptr &notebook, const QString &uri, const QString &path);

    bool shareNotebook(const mKCal::Notebook::Ptr &notebook, const QStringList &sharedWith);

    QStringList sharedWith(const mKCal::Notebook::Ptr &notebook);

    QString serviceName() const;

    ErrorCode error() const;

    //! \reimp_end

private:
  //@cond PRIVATE
  Q_DISABLE_COPY( DefaultInvitationPlugin )
  class Private;
  Private *const d;
  //@endcond
};

#endif
