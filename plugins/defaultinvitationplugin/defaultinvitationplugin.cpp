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
#include <QTimer>

#include <KCalendarCore/ICalFormat>

#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmailaddress.h>
#include <qmailserviceaction.h>

using namespace KCalendarCore;

const QString name("DefaultInvitationPlugin");

//@cond PRIVATE
class DefaultInvitationPlugin::Private
{
public:
    Private() : mStore(0), mDefaultAccount(0), mInit(false),
        mErrorCode(ServiceInterface::ErrorOk)
    {
    }
    ~Private()
    {
        uninit();
    }

    void init()
    {
        if (!mInit) {
            mStore = QMailStore::instance();
            Q_ASSERT(mStore);
            QMailAccountKey byDefault = QMailAccountKey::status(QMailAccount::PreferredSender);
            QMailAccountIdList accounts = mStore->queryAccounts(byDefault);
            if (!accounts.count()) {
                qWarning() << "Default account was not found!";
            } else {
                if (accounts.count() > 1) {
                    qWarning("There are more than one default account, using first");
                }
                mDefaultAccount = new QMailAccount(accounts.first());
            }
            mInit = true;
        }
    }

    void uninit()
    {
        mStore = 0;
        delete mDefaultAccount;
        mDefaultAccount = 0;
        mInit = false;
    }

    QString accountEmailAddress(const QString &accountId)
    {
        if (accountId.isEmpty()) {
            return QString();
        }
        QString email;
        const QMailAccountId accId(accountId.toULongLong());
        if (!QMailStore::instance()->queryAccounts(QMailAccountKey::id(accId)).isEmpty()) {
            QMailAccount account(accId);
            if (account.id().isValid() && (account.status() & QMailAccount::CanTransmit)) {
                email = account.fromAddress().address();
            } else {
                qWarning() << "Default plugin: account" << accountId << "is invalid or cannot transmit";
            }
        }
        if (email.isEmpty()) {
            qDebug() << "Default plugin: account" << accountId << "do not have a valid email address";
            init();
            email = mDefaultAccount ? mDefaultAccount->fromAddress().address() : QString();
        }
        return email;
    }

    bool sendMail(const QString &accountId, const QStringList &recipients, const QString &subject,
                  const QString &body, const QString &attachment, bool cancel)
    {
        qDebug() << "DefaultPlugin sendMail for account " << accountId;
        if (!mInit) {
            return false;
        }
        QMailAccount *account = nullptr;
        if (mDefaultAccount) {
            account = mDefaultAccount;
        } else {
            account = new QMailAccount(QMailAccountId(accountId.toULongLong()));
            if (!(account->status() & QMailAccount::CanTransmit)) {
                delete account;
                qWarning() << "Default plugin: invalid email account and no default email account";
                return false;
            }
        }

        // Build a message
        QMailMessage message;
        // Setup account which should be used to send a message
        message.setParentAccountId(account->id());
        // Get the outbox folder ID
        QMailFolderId folderId = account->standardFolder(QMailFolder::OutboxFolder);
        if (!folderId.isValid()) {
            folderId = QMailFolder::LocalStorageFolderId;
        }
        // Put message to standard outbox folder for that account
        message.setParentFolderId(folderId);

        // Setup message status
        message.setStatus(QMailMessage::Outbox, true);
        message.setStatus(QMailMessage::Outgoing, true);
        message.setStatus(QMailMessage::ContentAvailable, true);
        message.setStatus(QMailMessage::PartialContentAvailable, true);
        message.setStatus(QMailMessage::Read, true);
        message.setStatus(QMailMessage::HasAttachments, true);
        message.setStatus(QMailMessage::CalendarInvitation, true);
        message.setDate(QMailTimeStamp(QDateTime::currentDateTime()));

        // Define recipient's address
        QList<QMailAddress> addresses;
        const QMailAddress &fromAddress = account->fromAddress();
        const QString &accountEmail = fromAddress.address();
        foreach (const QString &mail, recipients) {
            if (mail.compare(accountEmail, Qt::CaseInsensitive) != 0) {
                addresses.append(QMailAddress(mail));
            }
        }
        message.setTo(addresses);
        // Define from address
        message.setFrom(fromAddress);
        // Define subject
        message.setSubject(subject);
        message.setMessageType(QMailMessage::Email);
        message.setMultipartType(cancel ? QMailMessagePartContainerFwd::MultipartAlternative : QMailMessagePartContainerFwd::MultipartRelated);

        // Create the MIME part representing the message body
        QMailMessagePart bodyPart = QMailMessagePart::fromData(
                                        body,
                                        QMailMessageContentDisposition(QMailMessageContentDisposition::None),
                                        QMailMessageContentType("text/plain;charset=UTF-8"),
                                        QMailMessageBody::NoEncoding);
        bodyPart.removeHeaderField("Content-Disposition");

        // Create the calendar MIME part
        QMailMessagePart calendarPart = QMailMessagePart::fromData(
                                            attachment,
                                            QMailMessageContentDisposition(QMailMessageContentDisposition::None),
                                            QMailMessageContentType(cancel ? "text/calendar;method=CANCEL;charset=UTF-8"
                                                                           : "text/calendar;method=REQUEST;charset=UTF-8"),
                                            QMailMessageBody::Base64);
        calendarPart.removeHeaderField("Content-Disposition");
        calendarPart.appendHeaderField("Content-Class", "urn:content-classes:calendarmessage");

        message.appendPart(bodyPart);
        message.appendPart(calendarPart);

        // send (to outbox)
        if (!mStore->addMessage(&message))
            return false;

        // initiate transmission
        QSharedPointer<QMailTransmitAction> action(new QMailTransmitAction());
        if (action) {
            action->transmitMessages(account->id());
        } else {
            qDebug("No MailTransmistAction");
            return false;
        }

        if (account != mDefaultAccount) {
            delete account;
        }
        return true;
    }

    QMailStore *mStore;
    QMailAccount *mDefaultAccount;
    bool mInit;
    ServiceInterface::ErrorCode mErrorCode;
};


DefaultInvitationPlugin::DefaultInvitationPlugin(): d(new Private())
{
}

DefaultInvitationPlugin::~DefaultInvitationPlugin()
{
    delete d;
}

bool DefaultInvitationPlugin::sendInvitation(const QString &accountId, const QString &notebookUid,
                                             const Incidence::Ptr &invitation, const QString &body)
{
    Q_UNUSED(body);
    Q_UNUSED(notebookUid);

    d->mErrorCode = ServiceInterface::ErrorOk;

    Attendee::List attendees = invitation->attendees();
    if (attendees.size() == 0) {
        qDebug("No attendees");
        return false;
    }

    d->init();

    ICalFormat icf;
    const QString &ical = icf.createScheduleMessage(invitation, iTIPRequest);

    QStringList emails;
    foreach (const Attendee &att, attendees) {
        emails.append(att.email());
    }

    const QString &description = invitation->description();
    const bool res = d->sendMail(accountId, emails, invitation->summary(), description, ical, false);

    d->uninit();
    return res;
}

bool DefaultInvitationPlugin::sendUpdate(const QString &accountId, const Incidence::Ptr &invitation,
                                         const QString &body)
{
    Q_UNUSED(body);
    d->mErrorCode = ServiceInterface::ErrorOk;

    Attendee::List attendees = invitation->attendees();
    if (attendees.size() == 0) {
        qDebug("No attendees");
        return false;
    }

    d->init();

    ICalFormat icf;
    const QString remoteUidValue(invitation->nonKDECustomProperty("X-SAILFISHOS-REMOTE-UID"));
    const bool cancelled = invitation->status() == Incidence::StatusCanceled;

    Incidence::Ptr invitationCopy = Incidence::Ptr(invitation->clone());
    if (!remoteUidValue.isEmpty()) {
        invitationCopy->setUid(remoteUidValue);
    }
    const QString &ical = icf.createScheduleMessage(invitationCopy, cancelled ? iTIPCancel : iTIPRequest);

    QStringList emails;
    foreach (const Attendee &att, attendees) {
        emails.append(att.email());
    }

    const QString &description = invitationCopy->description();
    const bool res = d->sendMail(accountId, emails, invitationCopy->summary(), description, ical, cancelled);

    d->uninit();
    return res;
}

bool DefaultInvitationPlugin::sendResponse(const QString &accountId, const Incidence::Ptr &invitation,
                                           const QString &body)
{
    d->mErrorCode = ServiceInterface::ErrorOk;

    d->init();

    // Is there an organizer?
    const Person &organizer = invitation->organizer();
    if (organizer.isEmpty() || organizer.email().isEmpty()) { // we do not have an organizer
        qWarning() << "sendResponse() called with wrong invitation: there is no organizer!";
        return false;
    }

    // Check: Am I one of the attendees? Had the organizer requested RSVP from me?
    const Attendee &me = invitation->attendeeByMail(d->accountEmailAddress(accountId));
    if (me.isNull() || (!me.RSVP())) {
        qWarning() << "sendResponse() called with wrong invitation: we are not invited or no response is expected.";
        return false;
    }

    ICalFormat icf;
    const QString remoteUidValue(invitation->nonKDECustomProperty("X-SAILFISHOS-REMOTE-UID"));

    Incidence::Ptr invitationCopy = Incidence::Ptr(invitation->clone());
    if (!remoteUidValue.isEmpty()) {
        invitationCopy->setUid(remoteUidValue);
    }

    const QString &ical = icf.createScheduleMessage(invitationCopy, iTIPReply);

    bool res = d->sendMail(accountId, QStringList(organizer.email()), invitationCopy->summary(), body, ical, false);

    return res;
}

QString DefaultInvitationPlugin::pluginName() const
{
    d->mErrorCode = ServiceInterface::ErrorOk;
    return name;
}

QString DefaultInvitationPlugin::icon() const
{
    return QString();
}

QString DefaultInvitationPlugin::uiName() const
{
    return QLatin1String("Default");
}

bool DefaultInvitationPlugin::multiCalendar() const
{
    d->mErrorCode = ServiceInterface::ErrorNotSupported;
    return false;
}

QString DefaultInvitationPlugin::emailAddress(const mKCal::Notebook &notebook)
{
    if (!notebook.isValid()) {
        qWarning() << "Invalid notebook";
        return QString();
    }
    if (notebook.account().isEmpty()) {
        // just return quietly, it can be a local notebook
        return QString();
    }

    return d->accountEmailAddress(notebook.account());
}

QString DefaultInvitationPlugin::displayName(const mKCal::Notebook &notebook) const
{
    Q_UNUSED(notebook);
    return QString();
}

bool DefaultInvitationPlugin::downloadAttachment(const mKCal::Notebook &notebook, const QString &uri,
                                                 const QString &path)
{
    Q_UNUSED(notebook);
    Q_UNUSED(uri);
    Q_UNUSED(path);
    d->mErrorCode = ServiceInterface::ErrorNotSupported;
    return false;
}

bool DefaultInvitationPlugin::deleteAttachment(const mKCal::Notebook &notebook, const Incidence::Ptr &incidence,
                                               const QString &uri)
{
    Q_UNUSED(notebook);
    Q_UNUSED(incidence);
    Q_UNUSED(uri);
    d->mErrorCode = ServiceInterface::ErrorNotSupported;
    return false;
}

bool DefaultInvitationPlugin::shareNotebook(const mKCal::Notebook &notebook, const QStringList &sharedWith)
{
    Q_UNUSED(notebook);
    Q_UNUSED(sharedWith);
    d->mErrorCode = ServiceInterface::ErrorNotSupported;
    return false;
}

QStringList DefaultInvitationPlugin::sharedWith(const mKCal::Notebook &notebook)
{
    Q_UNUSED(notebook);
    d->mErrorCode = ServiceInterface::ErrorNotSupported;
    return QStringList();
}

QString DefaultInvitationPlugin::DefaultInvitationPlugin::serviceName() const
{
    d->mErrorCode = ServiceInterface::ErrorOk;
    return name;
}


QString DefaultInvitationPlugin::defaultNotebook() const
{
    d->mErrorCode = ServiceInterface::ErrorNotSupported;
    return QString();
}

bool DefaultInvitationPlugin::checkProductId(const QString &prodId) const
{
    Q_UNUSED(prodId);
    d->mErrorCode = ServiceInterface::ErrorNotSupported;
    return false;
}

ServiceInterface::ErrorCode DefaultInvitationPlugin::error() const
{
    return d->mErrorCode;
}
