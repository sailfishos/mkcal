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


#include "defaultinvitationplugin.h"
#include <extendedcalendar.h>
#include <QDebug>
#include <icalformat.h>
#include <QTimer>

#ifdef MKCAL_FOR_MEEGO
#include <QMailAccount>
#include <QMailStore>
#include <QMailBase64Codec>
#include "transmitemail.h"
#endif

using namespace KCalCore;

const QString name("DefaultInvitationPlugin");
const QString vcalHead("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Nokia//Maemo Calendar//EN\r\n"); //TODO official product name?
const QString vcalFoot("END:VCALENDAR\r\n");

DefaultInvitationPlugin::DefaultInvitationPlugin()
{
}

bool DefaultInvitationPlugin::sendInvitation(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{
    Q_UNUSED(accountId);
    Q_UNUSED(invitation);
    Q_UNUSED(body);
    qDebug() << "*** DefaultInvitationPlugin::sendInvitation";
    return false;
}

bool DefaultInvitationPlugin::sendUpdate(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{
    Q_UNUSED(accountId);
    Q_UNUSED(invitation);
    Q_UNUSED(body);
    qDebug() << "*** DefaultInvitationPlugin::sendUpdate";
    return false;
}

bool DefaultInvitationPlugin::sendResponse(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{
    Q_UNUSED(accountId);    // not needed by this plugin: we use the default account

#ifdef MKCAL_FOR_MEEGO
    // Is there an organizer?
    Person::Ptr organizer = invitation->organizer();
    if (organizer->isEmpty() || organizer->email().isEmpty()) { // we do not have an organizer
        qWarning() << "sendResponse() called with wrong invitation: there is no organizer!";
        return false;
    }

    // TODO get default account from Messaging Framework.
    // Until then:  ------------>
    QString accountName("organisertest01@gmail.com");    // our "default" account's name
    QMailStore* store = QMailStore::instance();
    Q_ASSERT(store);
    QMailAccountKey byName = QMailAccountKey::name(accountName);
    QMailAccountIdList accounts = store->queryAccounts(byName);
    if (!accounts.count()) {
        qWarning("Account with '%s' name was not found!", accountName.toAscii().data());
        return false;
    }
    if (accounts.count() > 1)
        qWarning("There are more the 1 accounts with name '%s'.", accountName.toAscii().data());
    QMailAccount account(accounts.first());
    // <--------------- temporary hack ends here

    // Check: Am I one of the attendees? Had the organizer requested RSVP from me?
    Attendee::Ptr me = invitation->attendeeByMail(account.fromAddress().address());
    if (me == 0 || !me->RSVP()) {
        qWarning() << "sendResponse() called with wrong invitation: we are not invited or no response is expected.";
        return false;
    }


    // Build a message
    QMailMessage message;
    // Setup account which should be used to send a message
    message.setParentAccountId(account.id());
    // Put message to standard outbox folder fo that account
    message.setParentFolderId(QMailFolderId(QMailFolder::OutboxFolder));
    // Setup message status
    message.setStatus(QMailMessage::Outbox);
    // Define recipeint's address
    message.setTo(QMailAddress(organizer->email()));
    // Define from address
    message.setFrom(account.fromAddress());
    // Define subject
    message.setSubject(invitation->summary());
    // Define message body
    QMailMessagePart msg = QMailMessagePart::fromData(body, QMailMessageContentDisposition::Attachment,
            QMailMessageContentType("plain/text"), QMailMessageBodyFwd::QuotedPrintable);
    message.appendPart(msg);
    // add invitation
    ICalFormat icf;
    QString ical = vcalHead + icf.toString(invitation) + vcalFoot;
//    QString base64Data = ical;
    QMailBase64Codec encoder(QMailBase64Codec::Text);
    QByteArray base64Data = encoder.encode(ical);
    QMailMessagePart att = QMailMessagePart::fromData(base64Data, QMailMessageContentDisposition::Attachment,
            QMailMessageContentType("application/ics"), QMailMessageBodyFwd::Base64);
    message.appendPart(att);
    message.setMultipartType(QMailMessagePartContainer::MultipartMixed);

    // send (to outbox)
    if (!store->addMessage(&message))
        return false;

    // initiate transmission
    TransmitHelper* transmitHelper = new TransmitHelper(account.id());
    //TODO? connect(&transmitHelper, SIGNAL(done()), something, SLOT(messageSentNotification()));
    QTimer::singleShot(1000, transmitHelper, SLOT(transmit()));
#else
    Q_UNUSED(invitation);
    Q_UNUSED(body);
#endif
    return true;
}

QString DefaultInvitationPlugin::pluginName() const
{
    return name;
}

Q_EXPORT_PLUGIN2(defaultinvitationplugin, DefaultInvitationPlugin);
