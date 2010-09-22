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
#include <QMailAddress>
#include <QMailBase64Codec>
#include "transmitemail.h"
#endif

using namespace KCalCore;

const QString name("DefaultInvitationPlugin");
const QString vcalHead("BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Nokia//Maemo Calendar//EN\r\n"); //TODO official product name?
const QString vcalFoot("END:VCALENDAR\r\n");

//@cond PRIVATE
class DefaultInvitationPlugin::Private
{
public:
  Private() : mStore( 0 ), mDefaultAccount( 0 ), mInit ( false ){}
  ~Private() {
    uninit();
  }

  void init() {

    if ( !mInit ) {
      mStore = QMailStore::instance();
      Q_ASSERT(mStore);
//      QMailAccountKey byDefault = QMailAccountKey::status( QMailAccount::PreferredSender );  //Not yet done in email
      QMailAccountKey byDefault = QMailAccountKey::name( "aaaa" );
      QMailAccountIdList accounts = mStore->queryAccounts(byDefault);
      if (!accounts.count()) {
        qWarning() << "Default account was not found!";
        //        delete mStore;
        mStore = 0;
        return;
      }
      if (accounts.count() > 1) {
        qWarning( "There are more than one default account, using firsts" );
      }
      mDefaultAccount =  new QMailAccount( accounts.first() );
      mInit = true;

    }
  }

  QString defaultAddress() {

    if ( mInit ) {
      return mDefaultAccount->fromAddress().address();
    } else {
      return QString();
    }
  }

  void uninit() {

    delete mStore;
    mStore = 0;
    delete mDefaultAccount;
    mDefaultAccount = 0;
    mInit = false;
  }

  bool sendMail( const QStringList &recipients, const QString &subject,
                 const QString &body, const QString &attachment ) {

    if ( !mInit ) {
      return false;
    }

    // Build a message
    QMailMessage message;
    // Setup account which should be used to send a message
    message.setParentAccountId( mDefaultAccount->id() );
    // Put message to standard outbox folder fo that account
    message.setParentFolderId( QMailFolderId( QMailFolder::OutboxFolder ) );
    // Setup message status
    message.setStatus( QMailMessage::Outbox );
    // Define recipeint's address
    QList<QMailAddress> addresses;
    foreach( const QString &mail, recipients) {
      addresses.append( QMailAddress( mail ) );
    }
    message.setTo( addresses );
    // Define from address
    message.setFrom( mDefaultAccount->fromAddress() );
    // Define subject
    message.setSubject( subject );
    // Define message body
    QMailMessagePart msg = QMailMessagePart::fromData( body,
                                                       QMailMessageContentDisposition::Attachment,
                                                       QMailMessageContentType( "plain/text" ),
                                                       QMailMessageBodyFwd::QuotedPrintable );
    message.appendPart(msg);
    // add attachment

    QMailBase64Codec encoder( QMailBase64Codec::Text );
    QByteArray base64Data = encoder.encode( attachment );
    QMailMessagePart att = QMailMessagePart::fromData( base64Data,
                                                       QMailMessageContentDisposition::Attachment,
                                                       QMailMessageContentType( "application/ics" ),
                                                       QMailMessageBodyFwd::Base64 );
    message.appendPart(att);
    message.setMultipartType(QMailMessagePartContainer::MultipartMixed);

    // send (to outbox)
    if (!mStore->addMessage(&message))
      return false;

    // initiate transmission
    QSharedPointer<QMailTransmitAction> action ( new QMailTransmitAction() );
    if ( action ) {
      action->transmitMessages( mDefaultAccount->id() );
    } else {
      qDebug( "No MailTransmistAction" );
      return false;
    }

    return true;
  }

  QMailStore *mStore;
  QMailAccount *mDefaultAccount;
  bool mInit;
};


DefaultInvitationPlugin::DefaultInvitationPlugin(): d( new Private() )
{
}

DefaultInvitationPlugin::~DefaultInvitationPlugin()
{
  delete d;
}


bool DefaultInvitationPlugin::sendInvitation(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{

  Attendee::List attendees = invitation->attendees();
  if ( attendees.size() == 0 ) {
    qDebug("No attendees");
    return false;
  }

  d->init();

  invitation->setOrganizer( d->defaultAddress() );

  ICalFormat icf;
  QString ical = vcalHead + icf.toString(invitation) + vcalFoot;


  QStringList emails;
  foreach (const Attendee::Ptr &att, attendees) {
    emails.append( att->email() );
  }


  bool res = d->sendMail(emails, invitation->summary(), body, ical);

  d->uninit();
  return res;
}

bool DefaultInvitationPlugin::sendUpdate(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{
  //Same as sendResponse
  return sendResponse(accountId, invitation, body);
}

bool DefaultInvitationPlugin::sendResponse(const QString &accountId, const Incidence::Ptr &invitation, const QString &body)
{
  Q_UNUSED(accountId);    // not needed by this plugin: we use the default account

  d->init();
  // Is there an organizer?
  Person::Ptr organizer = invitation->organizer();
  if (organizer->isEmpty() || organizer->email().isEmpty()) { // we do not have an organizer
    qWarning() << "sendResponse() called with wrong invitation: there is no organizer!";
    return false;
  }

  qDebug() << d->defaultAddress();
  // Check: Am I one of the attendees? Had the organizer requested RSVP from me?
  Attendee::Ptr me = invitation->attendeeByMail( d->defaultAddress() );
  if (me == 0 || !me->RSVP()) {
    qWarning() << "sendResponse() called with wrong invitation: we are not invited or no response is expected.";
    return false;
  }

  ICalFormat icf;
  QString ical = vcalHead + icf.toString(invitation) + vcalFoot;
  //    QString base64Data = ical;

  bool res = d->sendMail(QStringList( organizer->email() ), invitation->summary(), body, ical);

  d->uninit();
  return res;

}

QString DefaultInvitationPlugin::pluginName() const
{
  return name;
}

Q_EXPORT_PLUGIN2(defaultinvitationplugin, DefaultInvitationPlugin);
