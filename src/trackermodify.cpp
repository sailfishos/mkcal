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
  This file is part of the API for handling calendar data and
  defines the TrackerModify class.

  @brief
  Tracker format implementation, insert/update/delete part.
  Separated from TrackerFormat so that other storages can save data
  to tracker for system wide indexing and searches to work.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/

#include "trackermodify.h"
using namespace KCalCore;

#include <kurl.h>

using namespace mKCal;

//@cond PRIVATE
class mKCal::TrackerModify::Private
{
  public:
    Private() {}
    ~Private() {}

    QString secrecy2String( Incidence::Ptr incidence )
    {
      switch( incidence->secrecy() ) {
      case Incidence::SecrecyPublic:
        return QString( "publicClassification" );
      case Incidence::SecrecyPrivate:
        return QString( "privateClassification" );
      case Incidence::SecrecyConfidential:
        return QString( "confidentialClassification" );
      default:
        return QString( "privateClassification" );
      }
    }

    QString status2String( Incidence::Ptr incidence )
    {
      switch( incidence->status() ) {
      case Incidence::StatusTentative:
        return QString( "tentativeStatus" );
      case Incidence::StatusConfirmed:
        return QString( "confirmedStatus" );
      case Incidence::StatusCompleted:
        return QString( "completedStatus" );
      case Incidence::StatusNeedsAction:
        return QString( "needsActionStatus" );
      case Incidence::StatusInProcess:
        return QString( "inProcessStatus" );
      case Incidence::StatusDraft:
        return QString( "draftStatus" );
      case Incidence::StatusFinal:
        return QString( "finalStatus" );
      case Incidence::StatusCanceled:
        if ( incidence->type() == Incidence::TypeEvent ) {
          return QString( "canceledEventStatus" );
        } else if ( incidence->type() == Incidence::TypeJournal ) {
          return QString( "canceledJournalStatus" );
        } else if ( incidence->type() == Incidence::TypeTodo ) {
          return QString( "canceledTodoStatus" );
        } else {
          return QString();
        }
      case Incidence::StatusX:
      case Incidence::StatusNone:
      default:
        return QString();
      }
    }

    QString transparency2String( Event::Ptr event )
    {
      switch( event->transparency() ) {
      case Event::Opaque:
        return QString( "opaqueTransparency" );
      case Event::Transparent:
        return QString( "transparentTransparency" );
      default:
        return QString();
      }
    }

    QString role2String( Attendee::Ptr attendee )
    {
      switch( attendee->role() ) {
      case Attendee::ReqParticipant:
        return QString( "reqParticipantRole" );
      case Attendee::OptParticipant:
        return QString( "optParticipantRole" );
      case Attendee::NonParticipant:
        return QString( "nonParticipantRole" );
      case Attendee::Chair:
        return QString( "chairRole" );
      default:
        return QString();
      }
    }

    QString partstat2String( Attendee::Ptr attendee )
    {
      switch( attendee->status() ) {
      case Attendee::NeedsAction:
        return QString( "needsActionParticipationStatus" );
      case Attendee::Accepted:
        return QString( "acceptedParticipationStatus" );
      case Attendee::Declined:
        return QString( "declinedParticipationStatus" );
      case Attendee::Tentative:
        return QString( "tentativeParticipationStatus" );
      case Attendee::Delegated:
        return QString( "delegatedParticipationStatus" );
      case Attendee::Completed:
        return QString( "completedParticipationStatus" );
      case Attendee::InProcess:
        return QString( "inProcessParticipationStatus" );
      default:
        return QString();
      }
    }

    QString action2String( Alarm::Ptr alarm )
    {
      switch( alarm->type() ) {
      case Alarm::Audio:
        return QString( "audioAction" );
      case Alarm::Display:
        return QString( "displayAction" );
      case Alarm::Email:
        return QString( "emailAction" );
      case Alarm::Procedure:
        return QString( "procedureAction" );
      default:
        return QString();
      }
    }

    QString daypos2String( int wdaypos )
    {
      switch( wdaypos ) {
      case 1:
        return QString( "monday" );
      case 2:
        return QString( "tuesday" );
      case 3:
        return QString( "wednesday" );
      case 4:
        return QString( "thursday" );
      case 5:
        return QString( "friday" );
      case 6:
        return QString( "saturday" );
      case 7:
        return QString( "sunday" );
      default:
        return QString();
      }
    }

    QString frequency2String( RecurrenceRule::PeriodType type )
    {
      switch( type ) {
      case RecurrenceRule::rSecondly:
        return QString( "secondly" );
      case RecurrenceRule::rMinutely:
        return QString( "minutely" );
      case RecurrenceRule::rHourly:
        return QString( "hourly" );
      case RecurrenceRule::rDaily:
        return QString( "daily" );
      case RecurrenceRule::rWeekly:
        return QString( "weekly" );
      case RecurrenceRule::rMonthly:
        return QString( "monthly" );
      case RecurrenceRule::rYearly:
        return QString( "yearly" );
      default:
        return QString();
      }
    }

    // The format supported by tracker is not really ISO8601, as it
    // doesn't support milliseconds. Therefore we eliminate those here.
    QString kdatetime2String( KDateTime dt, bool toUtc=true )
    {
      if ( toUtc ) {
        dt = dt.toUtc();
      }
      int msec = dt.dateTime().time().msec();

      dt = dt.addMSecs( -msec );
      return dt.toString();
    }

    QString uriAndRecurrenceId( Incidence::Ptr incidence )
    {
      if ( !incidence->hasRecurrenceId() ) {
        return incidence->uri().toString();
      } else {
        return incidence->uri().toString() + '-' + kdatetime2String( incidence->recurrenceId() );
      }
    }

    QString escapeString ( const QString &in ) {

      QString literal;

      foreach (const QChar ch, in ) {
          if (ch == QLatin1Char('\t'))
              literal.append(QLatin1String("\\\t"));
          else if (ch == QLatin1Char('\n'))
              literal.append(QLatin1String("\\\n"));
          else if (ch == QLatin1Char('\r'))
              literal.append(QLatin1String("\\\b"));
          else if (ch == QLatin1Char('\f'))
              literal.append(QLatin1String("\\\f"));
          else if (ch == QLatin1Char('\"'))
              literal.append(QLatin1String("\\\""));
          else if (ch == QLatin1Char('\''))
              literal.append(QLatin1String("\\\'"));
          else if (ch == QLatin1Char('\\'))
              literal.append(QLatin1String("\\\\"));
          else
              literal.append(ch);
      }
      return literal;
    }

    void insertRDates( Incidence::Ptr incidence, QStringList &query );
    void modifyRDate( Incidence::Ptr incidence, const KDateTime &rdate, QStringList &query );
    void insertExDates( Incidence::Ptr incidence, QStringList &query );
    void modifyExDate( Incidence::Ptr incidence, const KDateTime &exdate, QStringList &query );
    void insertAttendees( Incidence::Ptr incidence, QStringList &query );
    void modifyAttendee( Incidence::Ptr incidence, Attendee::Ptr attendee, int index,
                         QStringList &query );
    void insertAlarms( Incidence::Ptr incidence, QStringList &query );
    void modifyAlarm( Incidence::Ptr incidence, Alarm::Ptr alarm, int index, QStringList &query );
    void insertAttachments( Incidence::Ptr incidence, QStringList &query );
    void modifyAttachment( Incidence::Ptr incidence, Attachment::Ptr attachment,
                           QStringList &query );
    void insertRecurrences( Incidence::Ptr incidence, QStringList &query );
    void modifyRecurrence( Incidence::Ptr incidence, RecurrenceRule *rule, bool rrule, int index,
                           QStringList &query );

};
//@endcond

TrackerModify::TrackerModify()
  : d( new Private() )
{
}

TrackerModify::~TrackerModify()
{
  delete d;
}

bool TrackerModify::queries( const Incidence::Ptr &incidence, DBOperation dbop,
                             QStringList &insertQuery, QStringList &deleteQuery,
                             const QString &notebook )
{
  insertQuery << "INSERT { ";
  deleteQuery << "DELETE { ";

  QString type;
  switch( incidence->type() ) {
    case Incidence::TypeEvent:
      type = "Event";
      break;
    case Incidence::TypeTodo:
      type = "Todo";
      break;
    case Incidence::TypeJournal:
      type = "Journal";
      break;
    case Incidence::TypeFreeBusy:
      type = "FreeBusy";
      break;
    case Incidence::TypeUnknown:
      return false;
    }

  insertQuery << "<" << d->uriAndRecurrenceId( incidence ) << "> a ncal:" << type;

  insertQuery << ", nie:DataObject; ncal:uid <" << incidence->uid() << ">";
  if ( incidence->type() == Incidence::TypeEvent ) {
    Event::Ptr event = incidence.staticCast<Event>();
    if ( event->dtStart().isValid() ) {
      insertQuery << "; ncal:dtstart [ a ncal:NcalDateTime; ncal:dateTime \""
                  << d->kdatetime2String( event->dtStart(), false ) << "\"";
      if ( !event->dtStart().isUtc() ) {
        insertQuery << "; ncal:ncalTimezone <urn:x-ical:timezone:"
                    << event->dtStart().timeZone().name() << ">";
      }
      insertQuery << " ]";
    }
    if ( event->hasEndDate() ) {
      if ( event->allDay() ) {
        // +1 day because end date is non-inclusive.
        insertQuery << "; ncal:dtend [ a ncal:NcalDateTime; ncal:dateTime \""
                    << d->kdatetime2String( event->dtEnd().addDays( 1 ), false ) << "\"";
      } else {
        insertQuery << "; ncal:dtend [ a ncal:NcalDateTime; ncal:dateTime \""
                    << d->kdatetime2String( event->dtEnd(), false ) << "\"";
      }
      if ( !event->dtEnd().isUtc() ) {
        insertQuery << "; ncal:ncalTimezone <urn:x-ical:timezone:"
                    << event->dtEnd().timeZone().name() << ">";
      }
      insertQuery << " ]";
    }
    insertQuery << "; ncal:transp ncal:" << d->transparency2String( event );
  } else if ( incidence->type() == Incidence::TypeTodo ) {
    Todo::Ptr todo = incidence.staticCast<Todo>();
    if ( todo->hasStartDate() || todo->recurs() ) {
      insertQuery << "; ncal:dtstart [ a ncal:NcalDateTime; ncal:dateTime \""
                  << d->kdatetime2String( todo->dtStart(), false ) << "\"";
      if ( !todo->dtStart().isUtc() ) {
        insertQuery << "; ncal:ncalTimezone <urn:x-ical:timezone:"
                    << todo->dtStart().timeZone().name() << ">";
      }
      insertQuery << " ]";
    }
    if ( todo->hasDueDate() ) {
      insertQuery << "; ncal:due [ a ncal:NcalDateTime; ncal:dateTime \""
                  << d->kdatetime2String( todo->dtDue(), false ) << "\"";
      if ( !todo->dtDue().isUtc() ) {
        insertQuery << "; ncal:ncalTimezone <urn:x-ical:timezone:"
                    << todo->dtDue().timeZone().name() << ">";
      }
      insertQuery << " ]";
    }
    if ( todo->isCompleted() ) {
      if ( !todo->hasCompletedDate() ) {
        // If the todo was created by KOrganizer<2.2 it does not have
        // a correct completion date. Set one now.
        todo->setCompleted( KDateTime::currentUtcDateTime() );
      }
      insertQuery << "; ncal:completed \""
                  << d->kdatetime2String( todo->completed() ) << "\"";
    }
    insertQuery << "; ncal:percentComplete " << QString::number( todo->percentComplete() );
  } else if ( incidence->type() == Incidence::TypeJournal ) {
    Journal::Ptr journal = incidence.staticCast<Journal>();
    if ( journal->dtStart().isValid() ) {
      insertQuery << "; ncal:dtstart [ a ncal:NcalDateTime; ncal:dateTime \""
                  << d->kdatetime2String( journal->dtStart(), false ) << "\"";
      if ( !journal->dtStart().isUtc() ) {
        insertQuery << "; ncal:ncalTimezone <urn:x-ical:timezone:"
                    << journal->dtStart().timeZone().name() << ">";
      }
      insertQuery << " ]";
    }
  }

  if ( !incidence->summary().isEmpty() ) {
    insertQuery << "; ncal:summary \"" << d->escapeString( incidence->summary() ) << "\"";
  }
  if ( !incidence->categoriesStr().isEmpty() ) {
    insertQuery << "; ncal:categories \"" << incidence->categoriesStr() << "\"";
  }
  QString contacts = incidence->contacts().join( " " );
  if ( !contacts.isEmpty() ) {
    insertQuery << "; ncal:contact \"" << contacts << "\"";
  }
  insertQuery << "; ncal:class ncal:" << d->secrecy2String( incidence );
  if ( !incidence->description().isEmpty() ) {
    insertQuery << "; ncal:description \"" << d->escapeString( incidence->description() ) << "\"";
  }
  insertQuery << "; ncal:" << type.toLower()
              << "Status ncal:" << d->status2String( incidence );

  if ( incidence->type() != Incidence::TypeJournal ) {
    // NOTE duration seems to be a number in ontology
    if ( incidence->hasDuration() ) {
      insertQuery << "; ncal:duration "<< QString::number( incidence->duration().asSeconds() );
    }

    if ( !incidence->location().isEmpty() ) {
      insertQuery << "; ncal:location \"" << d->escapeString(incidence->location() ) << "\"";
    }
    if ( incidence->hasGeo() ) {
      insertQuery << "; ncal:geo \"" << QString( "%1," ).arg( incidence->geoLatitude(), 0, 'f', 6 )
                  << QString( "%1\"" ).arg( incidence->geoLongitude(), 0, 'f', 6 );
    }
    insertQuery << "; ncal:priority " << QString::number( incidence->priority() );
    QString resources = incidence->resources().join( "," );
    if ( !resources.isEmpty() ) {
      insertQuery << "; ncal:resources \"" << resources << "\"";
    }
  }

  if ( dbop != DBDelete ) {
    insertQuery << "; ncal:dtstamp \"" << d->kdatetime2String( incidence->created() ) << "\"";
  }

  insertQuery << "; ncal:created \"" << d->kdatetime2String( incidence->created() ) << "\"";
  insertQuery << "; nie:contentCreated \"" << d->kdatetime2String( incidence->created() ) << "\"";

  if ( dbop == DBUpdate ) {
    insertQuery << "; nie:contentLastModified \""
                << d->kdatetime2String( KDateTime::currentUtcDateTime() ) << "\"";
  }
  if ( dbop == DBDelete ) {
    insertQuery << "; ncal:lastModified \""
                << d->kdatetime2String( KDateTime::currentUtcDateTime() ) << "\"";
  } else {
    insertQuery << "; ncal:lastModified \""
                << d->kdatetime2String( incidence->lastModified() ) << "\"";
  }
  insertQuery << "; ncal:sequence " << QString::number( incidence->revision() );
  QString comments = incidence->comments().join( " " );
  if ( !comments.isEmpty() ) {
    insertQuery << "; ncal:comment \"" << comments << "\"";
  }
  if ( incidence->hasRecurrenceId() ) {
    insertQuery << "; ncal:recurrenceId [ a ncal:NcalDateTime; ncal:dateTime \""
                << d->kdatetime2String( incidence->recurrenceId() ) << "\"";
    if ( !incidence->recurrenceId().isUtc() ) {
      insertQuery << "; ncal:ncalTimezone <urn:x-ical:timezone:"
                  << incidence->recurrenceId().timeZone().name() << ">";
    }
    insertQuery << " ]";
  }
  if ( !incidence->relatedTo().isNull() ) {
    insertQuery << "; ncal:relatedToParent <" << incidence->relatedTo() << ">";
  }
  insertQuery << "; ncal:url <" << incidence->uri().toString() << ">";

  if ( !notebook.isEmpty() ) {
    insertQuery << "; nie:isLogicalPartOf \"<urn:x-ical:" << notebook
                << ">\" . \"<urn:x-ical:" << notebook << ">\" a ncal:Calendar";
  }

  Person::Ptr person = incidence->organizer();
  if ( !person->email().isEmpty() ) {
    insertQuery << " . _:organizer a ncal:Organizer; ncal:involvedContact [ a nco:Contact; nco:hasEmailAddress <mailto:"
                << person->email() << ">; nco:fullname \"" << person->name() << "\" ]";
    insertQuery << " . <" << d->uriAndRecurrenceId( incidence )
                << "> ncal:organizer _:organizer";
  }

  deleteQuery << "<" << d->uriAndRecurrenceId( incidence ) << "> a rdfs:Resource";
  deleteQuery << " }";

  d->insertRDates( incidence, insertQuery );
  d->insertExDates( incidence, insertQuery );
  d->insertAttendees( incidence, insertQuery );
  d->insertAlarms( incidence, insertQuery );
  d->insertAttachments( incidence, insertQuery );
  d->insertRecurrences( incidence, insertQuery );
  insertQuery << " }";

  return true;
}

bool TrackerModify::notifyOpen( const Incidence::Ptr &incidence, QStringList &query )
{
  query << "INSERT { ";

  QString type;
  switch( incidence->type() ) {
    case Incidence::TypeEvent:
      type = "Event";
      break;
    case Incidence::TypeTodo:
      type = "Todo";
      break;
    case Incidence::TypeJournal:
      type = "Journal";
      break;
    case Incidence::TypeFreeBusy:
      type = "FreeBusy";
      break;
  case Incidence::TypeUnknown:
    return false;
    }

  query << "<" << d->uriAndRecurrenceId( incidence ) << "> a ncal:" << type;

  query << "; nie:contentAccessed \""
        << d->kdatetime2String( KDateTime::currentUtcDateTime() ) << "\"";
  query << " }";

  return true;
}

//@cond PRIVATE
void TrackerModify::Private::insertRDates( Incidence::Ptr incidence, QStringList &query )
{
  DateTimeList list = incidence->recurrence()->rDateTimes();
  DateTimeList::ConstIterator it;
  for ( it = list.constBegin(); it != list.constEnd(); ++it ) {
    modifyRDate( incidence, (*it), query );
  }
}

void TrackerModify::Private::modifyRDate( Incidence::Ptr incidence, const KDateTime &rdate,
                                          QStringList &query )
{
  if ( query.size() > 1 ) {
    query << " . ";
  }

  query << "<" << uriAndRecurrenceId( incidence )
        << "> ncal:rdate [ a ncal:NcalDateTime; ncal:dateTime \""
        << kdatetime2String( rdate ) << "\"";

  if ( !rdate.isUtc() ) {
    query << "; ncal:ncalTimezone <urn:x-ical:timezone:" << rdate.timeZone().name() << ">";
  }
  query << " ]";
}

void TrackerModify::Private::insertExDates( Incidence::Ptr incidence, QStringList &query )
{
  DateTimeList list = incidence->recurrence()->exDateTimes();
  DateTimeList::ConstIterator it;
  for ( it = list.constBegin(); it != list.constEnd(); ++it ) {
    modifyExDate( incidence, (*it), query );
  }
}

void TrackerModify::Private::modifyExDate( Incidence::Ptr incidence, const KDateTime &exdate,
                                           QStringList &query )
{
  if ( query.size() > 1 ) {
    query << " . ";
  }

  query << "<" << uriAndRecurrenceId( incidence )
        << "> ncal:exdate [ a ncal:NcalDateTime; ncal:dateTime \""
        << kdatetime2String( exdate ) << "\"";

  if ( !exdate.isUtc() ) {
    query << "; ncal:ncalTimezone <urn:x-ical:timezone:" << exdate.timeZone().name() << ">";
  }
  query << " ]";
}

void TrackerModify::Private::insertAttendees( Incidence::Ptr incidence, QStringList &query )
{
  int index = 0;

  const Attendee::List &list = incidence->attendees();
  Attendee::List::ConstIterator it;
  for ( it = list.begin(); it != list.end(); ++it ) {
    modifyAttendee( incidence, *it, index++, query );
  }
}

void TrackerModify::Private::modifyAttendee( Incidence::Ptr incidence, Attendee::Ptr attendee,
                                             int index, QStringList &query )
{
  if ( query.size() > 1 ) {
    query << " . ";
  }

  query << "_:attendee" << QString::number( index ) << " a ncal:Attendee";

  query << "; ncal:involvedContact [ a nco:Contact; nco:hasEmailAddress <mailto:"
        << attendee->email() << ">; nco:fullname \"" << attendee->name() << "\" ]"; // NOTE validity not checked, that should be in Attendee

  // NOTE no ncal:cutype in kcal

  if ( !attendee->delegator().isEmpty() ) {
    query << "; ncal:delegatedFrom [ a nco:Contact; nco:hasEmailAddress <"
          << attendee->delegator() << "> ]"; // NOTE validity not checked, that should be in Attendee
  }

  if ( !attendee->delegate().isEmpty() ) {
    query << "; ncal:delegatedTo [ a nco:Contact; nco:hasEmailAddress <"
          << attendee->delegate() << "> ]"; // NOTE validity not checked, that should be in Attendee
  }

  // NOTE no ncal:member in kcal

  if ( attendee->status() != Attendee::None ) {
    query << "; ncal:partstat ncal:" << partstat2String( attendee );
  }

  query << "; ncal:role ncal:" << role2String( attendee );

  query << "; ncal:rsvp \"" << ( attendee->RSVP() ? "true" : "false" ) << "\"";

  query << " . <" << uriAndRecurrenceId( incidence )
        << "> ncal:attendee _:attendee" << QString::number( index );

}

void TrackerModify::Private::insertAlarms( Incidence::Ptr incidence, QStringList &query )
{
  int index = 0;

  const Alarm::List &list = incidence->alarms();
  Alarm::List::ConstIterator it;
  for ( it = list.begin(); it != list.end(); ++it ) {
    modifyAlarm( incidence, *it, index++, query );
  }
}

void TrackerModify::Private::modifyAlarm( Incidence::Ptr incidence, Alarm::Ptr alarm,
                                          int index, QStringList &query )
{
  if ( query.size() > 1 ) {
    query << " . ";
  }

  query << "_:alarm" << QString::number( index ) << " a ncal:Alarm";

  query << "; ncal:action ncal:" << action2String( alarm );

  if ( alarm->repeatCount() ) {
    query << "; ncal:repeat " << QString::number( alarm->repeatCount() );
    query << "; ncal:duration " << QString::number( alarm->snoozeTime().asSeconds() );
  }
  if ( alarm->startOffset().value() ) {
    query << "; ncal:trigger [ a ncal:Trigger; ncal:related ncal:startTriggerRelation; ncal:triggerDuration \""
          << QString::number( alarm->startOffset().asSeconds() ) << "\" ]";
  } else if ( alarm->endOffset().value() ) {
    query << "; ncal:trigger [ a ncal:Trigger; ncal:related ncal:endTriggerRelation; ncal:triggerDuration \""
          << QString::number( alarm->endOffset().asSeconds() ) << "\" ]";
  } else if ( alarm->hasTime() ) {
    query << "; ncal:trigger [ a ncal:Trigger; ncal:triggerDateTime \""
          << kdatetime2String( alarm->time() ) << "\" ]";
  }

  switch( alarm->type() ) {
  case Alarm::Display:
    query << "; ncal:description \"" << alarm->text() << "\"";
    break;

  case Alarm::Procedure:
    query << "; ncal:attach [ a ncal:Attachment; ncal:fmttype \"application/binary\"; ncal:attachmentUri <"
          << alarm->programFile() << "> ]";
    if ( !alarm->programArguments().isEmpty() ) {
      query << "; ncal:description \"" << alarm->programArguments() << "\"";
    }
    break;

  case Alarm::Email:
    query << "; ncal:summary \"" << alarm->mailSubject() << "\""
          << "; ncal:description \"" << alarm->mailText() << "\"";
    // NOTE attachments and attendees are joined into one attachement
    // and attendee, respectively. This is a hack but it's the only
    // simple way to do it.
    if ( alarm->mailAttachments().size() > 0 ) {
      query << "; ncal:attach [ a ncal:Attachment; ncal:fmttype \"application/binary\"; ncal:attachmentUri <"
            << alarm->mailAttachments().join( "," ) << "> ]";
    }
    if ( alarm->mailAddresses().size() > 0 ) {
      QStringList addresses;
      QStringList fullnames;
      for ( int i = 0; i < alarm->mailAddresses().size(); i++ ) {
        addresses << alarm->mailAddresses().at(i)->email();
        fullnames << alarm->mailAddresses().at(i)->name();
      }
      query << " . _:attendee" << QString::number( index )
            << " a ncal:Attendee; ncal:involvedContact [ a nco:Contact; nco:hasEmailAddress <mailto:"
            << addresses.join( "," )
            << ">; nco:fullname \"" << fullnames.join( "," )
            << "\" ] . _:alarm" << QString::number( index )
            << " ncal:attendee _:attendee" << QString::number( index );
    }
    break;

  case Alarm::Audio:
    if (!alarm->audioFile().isEmpty() ) {
      query << "; ncal:attach [ a ncal:Attachment; ncal:fmttype \"audio/basic\"; ncal:attachmentUri <"
            << alarm->audioFile() << "> ]";
    }
    break;

  default:
    break;
  }

  query << " . <" << uriAndRecurrenceId( incidence )
        << "> ncal:hasAlarm _:alarm" << QString::number( index );

}

void TrackerModify::Private::insertAttachments( Incidence::Ptr incidence, QStringList &query )
{
  const Attachment::List &list = incidence->attachments();
  Attachment::List::ConstIterator it;
  for ( it = list.begin(); it != list.end(); ++it ) {
    modifyAttachment( incidence, *it, query );
  }
}

void TrackerModify::Private::modifyAttachment( Incidence::Ptr incidence,
                                               Attachment::Ptr attachment, QStringList &query )
{
  if ( query.size() > 1 ) {
    query << " . ";
  }

  query << "<" << uriAndRecurrenceId( incidence ) << "> ncal:attach [ a ncal:Attachment";

  if ( attachment->isBinary() ) {
    query << "; ncal:attachmentContent \"" << attachment->data() << "\"";
    query << "; ncal:encoding ncal:base64Encoding"; // only base64 supported
  } else {
    query << "; ncal:attachmentUri <" << attachment->uri() << ">";
  }
  if ( !attachment->mimeType().isEmpty() ) {
    query << "; ncal:fmttype \"" << attachment->mimeType() << "\"";
  }

  query << " ]";
}

void TrackerModify::Private::insertRecurrences( Incidence::Ptr incidence, QStringList &query )
{
  int index = 0;

  const RecurrenceRule::List &list1 = incidence->recurrence()->rRules();
  RecurrenceRule::List::ConstIterator it1;
  for ( it1 = list1.begin(); it1 != list1.end(); ++it1 ) {
    modifyRecurrence( incidence, *it1, true, index++, query );
  }

  const RecurrenceRule::List &list2 = incidence->recurrence()->exRules();
  RecurrenceRule::List::ConstIterator it2;
  for ( it2 = list2.begin(); it2 != list2.end(); ++it2 ) {
    modifyRecurrence( incidence, *it2, false, index++, query );
  }
}

void TrackerModify::Private::modifyRecurrence( Incidence::Ptr incidence, RecurrenceRule *rule,
                                               bool rrule, int index, QStringList &query )
{
  if ( query.size() > 1 ) {
    query << " . ";
  }

  query << "_:recurrencerule" << QString::number( index ) << " a ncal:RecurrenceRule";

  QList<RecurrenceRule::WDayPos>::iterator j;
  QList<RecurrenceRule::WDayPos> wdList = rule->byDays();
  if ( !wdList.empty() ) {
    // NOTE take only first since ontology doesn't support lists
    for ( j = wdList.begin(); j != wdList.end(); ++j ) {
      query << "; ncal:byday [ a ncal:BydayRulePart; ncal:bydayModifier \""
            << QString::number( (*j).pos() )
            << "\"; ncal:bydayWeekday ncal:" << daypos2String( (*j).day() ) << " ]";
    }
  }

  QList<int>::iterator i;
  QList<int> byList = rule->byHours();
  if ( !byList.empty() ) {
    // NOTE take only first since ontology doesn't support lists
    for ( i = byList.begin(); i != byList.end(); ++i ) {
      query << "; ncal:byhour " << QString::number( *i );
    }
  }

  byList = rule->byMinutes();
  if ( !byList.empty() ) {
    // NOTE take only first since ontology doesn't support lists
    for ( i = byList.begin(); i != byList.end(); ++i ) {
      query << "; ncal:byminute " << QString::number( *i );
    }
  }

  byList = rule->byMonths();
  if ( !byList.empty() ) {
    for ( i = byList.begin(); i != byList.end(); ++i ) {
    // NOTE take only first since ontology doesn't support lists
      query << "; ncal:bymonth " << QString::number(*i);
    }
  }

  byList = rule->byMonthDays();
  if ( !byList.empty() ) {
    // NOTE take only first since ontology doesn't support lists
    for ( i = byList.begin(); i != byList.end(); ++i ) {
      query << "; ncal:bymonthday \"" << QString::number( *i ) << "\"";
    }
  }

  byList = rule->bySeconds();
  if ( !byList.empty() ) {
    // NOTE take only first since ontology doesn't support lists
    for ( i = byList.begin(); i != byList.end(); ++i ) {
      query << "; ncal:bysecond " << QString::number( *i );
    }
  }

  byList = rule->bySetPos();
  if ( !byList.empty() ) {
    // NOTE take only first since ontology doesn't support listse
    for ( i = byList.begin(); i != byList.end(); ++i ) {
      query << "; ncal:bysetpos " << QString::number( *i );
    }
  }

  byList = rule->byWeekNumbers();
  if ( !byList.empty() ) {
    // NOTE take only first since ontology doesn't support lists
    for ( i = byList.begin(); i != byList.end(); ++i ) {
      query << "; ncal:byweekno " << QString::number( *i );
    }
  }

  byList = rule->byYearDays();
  if ( !byList.empty() ) {
    // NOTE take only first since ontology doesn't support lists
    for ( i = byList.begin(); i != byList.end(); ++i ) {
      query << "; ncal:byyearday \"" << QString::number( *i ) << "\"";
    }
  }

  bool result;
  KDateTime endDt = rule->endDt( &result );
  if ( rule->duration() || !result ) {
    query << "; ncal:count \"" << QString::number( rule->duration() )  << "\"";
  } else {
    query << "; ncal:until \"" << kdatetime2String( endDt ) << "\"";
  }

  query << "; ncal:freq ncal:" << frequency2String( rule->recurrenceType() );

  if ( rule->frequency() ) {
    query << "; ncal:interval " << QString::number( rule->frequency() );
  }

  if ( rule->weekStart() >=1 && rule->weekStart() <= 7 ) {
    query << "; ncal:wkst ncal:" << daypos2String( rule->weekStart() );
  }

  if ( rrule ) {
    query << " . <" << uriAndRecurrenceId( incidence )
          << "> ncal:rrule _:recurrencerule" << QString::number( index );
  } else { // exrule
    query << " . <" << uriAndRecurrenceId( incidence )
          << "> ncal:exrule _:recurrencerule" << QString::number( index );
  }
}
//@endcond
