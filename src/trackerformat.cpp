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

#ifdef MKCAL_TRACKER_SUPPORT

/**
  @file
  This file is part of the API for handling calendar data and
  defines the TrackerFormat class.

  Deprecated!!

  @brief
  Tracker format implementation.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/
#include "trackerformat.h"
#include "trackermodify.h"
#include "trackerstorage.h"
using namespace KCalCore;

#include <kdebug.h>
#include <ksystemtimezone.h>
#include <kurl.h>

#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>

#include <iostream>
using namespace std;

using namespace mKCal;

//@cond PRIVATE
class mKCal::TrackerFormat::Private
{
  public:
    Private( TrackerStorage *storage, TrackerFormat *format, QDBusInterface *tracker,
             bool synchronuousMode )
      : mStorage( storage ),
        mFormat( format ),
        mTracker( tracker ),
        mSynchronuousMode( synchronuousMode ),
        mTimeSpec( KDateTime::UTC ),
        mOperation( DBNone ),
        mOperationList( 0 ),
        mOperationState( 0 ),
        mOperationError( 0 ),
        mOperationInsertWatcher( 0 ),
        mOperationDeleteWatcher( 0 ),
        mOperationEventsWatcher( 0 ),
        mOperationTodosWatcher( 0 ),
        mOperationJournalsWatcher( 0 ),
        mOperationRDatesWatcher( 0 ),
        mOperationExDatesWatcher( 0 ),
        mOperationAttendeesWatcher( 0 ),
        mOperationAlarmsWatcher( 0 ),
        mOperationAttachmentsWatcher( 0 ),
        mOperationRRecurrencesWatcher( 0 ),
        mOperationExRecurrencesWatcher( 0 )
    {}
    ~Private() {}

    void setSecrecy( const QString &string, Incidence::Ptr incidence )
    {
      if ( string.contains( "publicClassification" ) ) {
        incidence->setSecrecy( Incidence::SecrecyPublic );
      } else if ( string.contains( "privateClassification" ) ) {
        incidence->setSecrecy( Incidence::SecrecyPrivate );
      } else if ( string.contains( "confidentialClassification" ) ) {
        incidence->setSecrecy( Incidence::SecrecyConfidential );
      }
    }

    void setStatus( const QString &string, Incidence::Ptr incidence )
    {
      if ( string.contains( "tentativeStatus" ) ) {
        incidence->setStatus( Incidence::StatusTentative );
      } else if ( string.contains( "confirmedStatus" ) ) {
        incidence->setStatus( Incidence::StatusConfirmed );
      } else if ( string.contains( "completedStatus" ) ) {
        incidence->setStatus( Incidence::StatusCompleted );
      } else if ( string.contains( "needsActionStatus" ) ) {
        incidence->setStatus( Incidence::StatusNeedsAction );
      } else if ( string.contains( "inProcessStatus" ) ) {
        incidence->setStatus( Incidence::StatusInProcess );
      } else if ( string.contains( "draftStatus" ) ) {
        incidence->setStatus( Incidence::StatusDraft );
      } else if ( string.contains( "finalStatus" ) ) {
        incidence->setStatus( Incidence::StatusFinal );
      } else if ( string.contains( "canceled" ) ) {
        incidence->setStatus( Incidence::StatusCanceled );
      }
    }

    void setTransparency( const QString &string, Event::Ptr event )
    {
      if ( string.contains( "opaqueTransparency" ) ) {
        event->setTransparency( Event::Opaque );
      } else if ( string.contains( "transparentTransparency" ) ) {
        event->setTransparency( Event::Transparent );
      } else {
        event->setTransparency( ( Event::Transparency )-1 ); // invalid value
      }
    }

    void setGeo( const QString &string, Incidence::Ptr incidence )
    {
      QStringList list = string.split( ',' );
      if ( list.size() == 2 ) {
        float latitude = list.at( 0 ).toFloat();
        float longitude = list.at( 1 ).toFloat();
        incidence->setGeoLatitude( latitude );
        incidence->setGeoLongitude( longitude );
        incidence->setHasGeo( true );
      }
    }

    void setUid( const QString &string, Incidence::Ptr incidence )
    {
      if ( string.startsWith( QLatin1String( "urn:x-ical:" ), Qt::CaseInsensitive ) ) {
        incidence->setUid( string.mid( 11 ) );
      } else {
        if ( string.startsWith( QLatin1String( "file:////" ), Qt::CaseInsensitive ) ) {
          incidence->setUid( string.mid( 9 ) );
        } else {
          incidence->setUid( string );
        }
      }
    }

    void setRelatedTo( const QString &string, Incidence::Ptr incidence )
    {
      if ( string.startsWith( QLatin1String( "file:////" ), Qt::CaseInsensitive ) ) {
        incidence->setRelatedTo( string.mid( 9 ) );
      } else {
        incidence->setRelatedTo( string );
      }
    }

    void setRsvp( const QString &string, Attendee::Ptr attendee )
    {
      if ( string.contains( "true" ) ) {
        attendee->setRSVP( true );
      } else if ( string.contains( "false" ) ) {
        attendee->setRSVP( false );
      }
    }

    void setRole( const QString &string, Attendee::Ptr attendee )
    {
      if ( string.contains( "reqParticipantRole" ) ) {
        attendee->setRole( Attendee::ReqParticipant );
      } else if ( string.contains( "optParticipantRole" ) ) {
        attendee->setRole( Attendee::OptParticipant );
      } else if ( string.contains( "nonParticipantRole" ) ) {
        attendee->setRole( Attendee::NonParticipant );
      } else if ( string.contains( "chairRole" ) ) {
        attendee->setRole( Attendee::Chair );
      }
    }

    void setPartstat( const QString &string, Attendee::Ptr attendee )
    {
      if ( string.contains( "needsActionParticipationStatus" ) ) {
        attendee->setStatus( Attendee::NeedsAction );
      }
      else if ( string.contains( "acceptedParticipationStatus" ) ) {
        attendee->setStatus( Attendee::Accepted );
      }
      else if ( string.contains( "declinedParticipationStatus" ) ) {
        attendee->setStatus( Attendee::Declined );
      }
      else if ( string.contains( "tentativeParticipationStatus" ) ) {
        attendee->setStatus( Attendee::Tentative );
      }
      else if ( string.contains( "delegatedParticipationStatus" ) ) {
        attendee->setStatus( Attendee::Delegated );
      }
      else if ( string.contains( "completedParticipationStatus" ) ) {
        attendee->setStatus( Attendee::Completed );
      }
      else if ( string.contains( "inProcessParticipationStatus" ) ) {
        attendee->setStatus( Attendee::InProcess );
      }
    }

    int getDaypos( const QString &string )
    {
      if ( string.contains( "monday" ) ) {
        return 1;
      }
      if ( string.contains( "tuesday" ) ) {
        return 2;
      }
      if ( string.contains( "wednesday" ) ) {
        return 3;
      }
      if ( string.contains( "thursday" ) ) {
        return 4;
      }
      if ( string.contains( "friday" ) ) {
        return 5;
      }
      if ( string.contains( "saturday" ) ) {
        return 6;
      }
      if ( string.contains( "sunday" ) ) {
        return 7;
      }
      return 0;
    }

    RecurrenceRule::PeriodType getFrequency( const QString &string )
    {
      if ( string.contains( "secondly" ) ) {
        return RecurrenceRule::rSecondly;
      }
      if ( string.contains( "minutely" ) ) {
        return RecurrenceRule::rMinutely;
      }
      if ( string.contains( "hourly" ) ) {
        return RecurrenceRule::rHourly;
      }
      if ( string.contains( "daily" ) ) {
        return RecurrenceRule::rDaily;
      }
      if ( string.contains( "weekly" ) ) {
        return RecurrenceRule::rWeekly;
      }
      if ( string.contains( "monthly" ) ) {
        return RecurrenceRule::rMonthly;
      }
      if ( string.contains( "yearly" ) ) {
        return RecurrenceRule::rYearly;
      }
      return RecurrenceRule::rNone;
    }
    void setAction( const QString &string, Alarm::Ptr alarm )
    {
      if ( string.contains( "audioAction" ) ) {
        alarm->setType( Alarm::Audio );
      } else if ( string.contains( "displayAction" ) ) {
        alarm->setType( Alarm::Display );
      } else if ( string.contains( "emailAction" ) ) {
        alarm->setType( Alarm::Email );
      } else if ( string.contains( "procedureAction" ) ) {
        alarm->setType( Alarm::Procedure );
      } else {
        alarm->setType( Alarm::Invalid );
      }
    }
    void parseIncidence( const QStringList &list, int i, Incidence::Ptr incidence,
                         QString &notebook )
    {
      if ( !list.at( i ).isEmpty() ) {
        incidence->setSummary( list.at( i ) );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->setCategories( list.at( i ) );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->addContact( list.at( i ) );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->setDuration( Duration( list.at( i ).toInt() ) );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        setSecrecy( list.at( i ), incidence );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->setLocation( list.at( i ) );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->setDescription( list.at( i ) );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        setStatus( list.at( i ), incidence );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        setGeo( list.at( i ), incidence );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->setPriority( list.at( i ).toInt() );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->setResources( list.at( i ).split( ',' ) );
      }
      i++;
      incidence->setCreated( KDateTime::fromString( list.at( i++ ) ).toUtc() );
      i++; // skip another time stamp
      if ( !list.at(i).isEmpty() ) {
        incidence->setLastModified( KDateTime::fromString( list.at( i ) ) .toUtc() );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->setRevision( list.at( i ).toInt() );
      }
      i++;
      if ( !list.at( i ).isEmpty() ) {
        incidence->addComment( list.at( i ) );
      }
      i++;

      KDateTime datetime = KDateTime::fromString( list.at( i++ ) );
      KTimeZone timezone =
        KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
      if ( datetime.isValid() && timezone.isValid() ) {
        incidence->setRecurrenceId( datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
      } else if ( datetime.isValid() && !datetime.isUtc() ) {
        incidence->setRecurrenceId( datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
      } else {
        incidence->setRecurrenceId( datetime );
      }

      if ( !list.at( i ).isEmpty() ) {
        setRelatedTo( list.at( i ), incidence );
      }
      i++;
      i++; // skip uri
      setUid( list.at( i++ ), incidence );
      if ( !list.at( i ).isEmpty() ) {
        notebook = QString( list.at( i ).mid( 12, list.at( i ).size() - 13 ) );
      }
      i++; // skip urn:x-ical: and surrounding ""
      QString name( list.at( i++ ) );
      QString email( list.at( i++ ) );
      if ( !email.isNull() || !name.isNull() ) {
        if ( email.startsWith( QLatin1String( "mailto:" ), Qt::CaseInsensitive ) ) {
          email.remove( 0, 7 ); // skip mailto:
        }
        incidence->setOrganizer( Person::Ptr( new Person( name, email ) ) );
      }
    }

    Event::Ptr parseEvent( const QStringList &list, QString &notebook )
    {
      if ( list.size() == 30 ) {
        int i = 0;

        Event::Ptr event = Event::Ptr( new Event() );

        i++; // skip

        KDateTime datetime = KDateTime::fromString( list.at( i++ ) );
        KTimeZone timezone =
          KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() ) {
          if ( timezone.isValid() ) {
            event->setDtStart( datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
          } else if ( !datetime.isUtc() ) {
            event->setDtStart( datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
          } else {
            event->setDtStart( datetime );
          }
        }

        datetime = KDateTime::fromString( list.at( i++ ) );
        timezone = KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() ) {
          if ( timezone.isValid() ) {
            event->setDtEnd( datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
          } else if ( !datetime.isUtc() ) {
            event->setDtEnd( datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
          } else {
            event->setDtEnd( datetime );
          }
        }

        if ( !list.at( i ).isEmpty() ) {
          setTransparency( list.at( i ), event );
        }
        i++;

        parseIncidence( list, i, event, notebook );

        kDebug() << "parseEvent" << event->uid();

        return event;
      }
      return Event::Ptr();
    }

    Todo::Ptr parseTodo( const QStringList &list, QString &notebook )
    {
      if ( list.size() == 31 ) {
        int i = 0;

        Todo::Ptr todo = Todo::Ptr( new Todo() );

        i++; // skip

        KDateTime datetime = KDateTime::fromString( list.at( i++ ) );
        KTimeZone timezone =
          KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() ) {
          if ( timezone.isValid() ) {
            todo->setDtStart( datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
          } else if ( !datetime.isUtc() ) {
            todo->setDtStart( datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
          } else {
            todo->setDtStart( datetime );
          }
          todo->setHasStartDate( true );
        }
        datetime = KDateTime::fromString( list.at( i++ ) );
        timezone = KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() ) {
          if ( timezone.isValid() ) {
            todo->setDtDue( datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
          } else if ( !datetime.isUtc() ) {
            todo->setDtDue( datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
          } else {
            todo->setDtDue( datetime );
          }
          todo->setHasDueDate( true );
        }
        KDateTime completed = KDateTime::fromString( list.at( i++ ) ).toUtc();
        if ( completed.isValid() ) {
          todo->setCompleted( completed );
          todo->setCompleted( true );
        }
        if ( !list.at( i ).isEmpty() ) {
          todo->setPercentComplete( list.at( i ).toInt() );
        }
        i++;

        parseIncidence( list, i, todo, notebook );

        kDebug() << "parseTodo" << todo->uid();

        return todo;
      }
      return Todo::Ptr();
    }

    Journal::Ptr parseJournal( const QStringList &list, QString &notebook )
    {
      if ( list.size() == 23 ) {
        int i = 0;

        Journal::Ptr journal = Journal::Ptr( new Journal() );

        i++; // skip

        KDateTime datetime = KDateTime::fromString( list.at( i++ ) );
        KTimeZone timezone =
          KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() ) {
          if ( timezone.isValid() ) {
            journal->setDtStart( datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
          } else if ( !datetime.isUtc() ) {
            journal->setDtStart( datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
          } else {
            journal->setDtStart( datetime );
          }
        }
        if ( !list.at( i ).isEmpty() ) {
          journal->setSummary( list.at( i ) );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          journal->setCategories( list.at( i ) );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          journal->addContact( list.at( i ) );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          setSecrecy( list.at( i ), journal );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          journal->setDescription( list.at( i ) );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          setStatus( list.at( i ), journal );
        }
        i++;
        i++; // skip geo
        journal->setCreated( KDateTime::fromString( list.at( i++ ) ) .toUtc() );
        i++; // skip another time stamp
        if ( !list.at( i ).isEmpty() ) {
          journal->setLastModified( KDateTime::fromString( list.at( i ) ).toUtc() );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          journal->setRevision( list.at( i ) .toInt() );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          journal->addComment( list.at( i ) );
        }
        i++;

        datetime = KDateTime::fromString( list.at( i++ ) );
        timezone = KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() && timezone.isValid() ) {
          journal->setRecurrenceId( datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
        } else if ( datetime.isValid() && !datetime.isUtc() ) {
          journal->setRecurrenceId( datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
        } else {
          journal->setRecurrenceId( datetime );
        }

        if ( !list.at( i ).isEmpty() ) {
          setRelatedTo( list.at( i ), journal );
        }
        i++;
        i++; // skip uri
        setUid( list.at( i++ ), journal );
        if ( !list.at( i ).isEmpty() ) {
          notebook = QString( list.at( i ).mid( 12, list.at( i ).size() - 13 ) );
        }
        i++; // skip urn:x-ical: and surrounding ""

        QString name( list.at( i++ ) );
        QString email( list.at( i++ ) );
        if ( !email.isNull() || !name.isNull() ) {
          if ( email.startsWith( QLatin1String( "mailto:" ), Qt::CaseInsensitive ) ) {
            email.remove( 0, 7 ); // skip mailto:
          }
          journal->setOrganizer( Person::Ptr( new Person( name, email ) ) );
        }

        kDebug() << "parseJournal" << journal->uid();

        return journal;
      }
      return Journal::Ptr();
    }

    bool parseRDate( const QStringList &list, Incidence::Ptr incidence )
    {
      if ( list.size() == 2 ) {
        int i = 0;

        KDateTime datetime = KDateTime::fromString( list.at( i++ ) );
        KTimeZone timezone =
          KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() ) {
          if ( timezone.isValid() ) {
            incidence->recurrence()->addRDateTime(
              datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
          } else if ( !datetime.isUtc() ) {
            incidence->recurrence()->addRDateTime(
              datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
          } else {
            incidence->recurrence()->addRDateTime( datetime );
          }
        }

        kDebug() << "parseRDate" << datetime.toString();

        return true;
      }
      return false;
    }

    bool parseExDate( const QStringList &list, Incidence::Ptr incidence )
    {
      if ( list.size() == 2 ) {
        int i = 0;

        KDateTime datetime = KDateTime::fromString( list.at( i++ ) );
        KTimeZone timezone =
          KSystemTimeZones::zone( list.at( i++ ).mid( 20 ) ); // skip urn:x-ical:timezone:
        if ( datetime.isValid() ) {
          if ( timezone.isValid() ) {
            incidence->recurrence()->addExDateTime(
              datetime.toTimeSpec( KDateTime::Spec( timezone ) ) );
          } else if ( !datetime.isUtc() ) {
            incidence->recurrence()->addExDateTime(
              datetime.toTimeSpec( KDateTime::Spec::LocalZone() ) );
          } else {
            incidence->recurrence()->addExDateTime( datetime );
          }
        }

        kDebug() << "parseExDate" << datetime.toString();

        return true;
      }
      return false;
    }

    Attendee::Ptr parseAttendee( const QStringList &list )
    {
      if ( list.size() == 7 ) {
        int i = 0;

        Attendee::Ptr attendee = Attendee::Ptr( new Attendee( list.at( i++ ), list.at( i++ ) ) );

        if ( !list.at( i ).isEmpty() ) {
          attendee->setDelegator( list.at( i ) );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          attendee->setDelegate( list.at( i ) );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          setPartstat( list.at( i ), attendee );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          setRole( list.at( i ), attendee );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          setRsvp( list.at( i ), attendee );
        }
        i++;

        kDebug() << "parseAttendee" << attendee->name() << attendee->email();

        return attendee;
      }
      return Attendee::Ptr();
    }

    Alarm::Ptr parseAlarm( const QStringList &list, Incidence::Ptr incidence )
    {
      if ( list.size() == 9 ) { // should be 11, see selectAlarms
        int i = 0;

        Alarm ::Ptr alarm = incidence->newAlarm();

        alarm->setEnabled( true ); // NOTE is this ok always?
        setAction( list.at( i++ ), alarm );
        if ( !list.at( i ).isEmpty() ) {
          alarm->setRepeatCount( list.at( i ).toInt() );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          alarm->setSnoozeTime( Duration( list.at( i ).toInt(), Duration::Seconds ) );
        }
        i++;

        if ( !list.at( i ).isEmpty() ) {
          alarm->setTime( KDateTime::fromString( list.at( i ) ).toUtc() );
        }
        i++;
        QString relation( list.at( i++ ) );
        QString offset( list.at( i++ ) );
        if ( !alarm->hasTime() ) {
          if ( relation.contains( "startTriggerRelation" ) ) {
            alarm->setStartOffset( Duration( offset.toInt(), Duration::Seconds ) );
          } else if ( relation.contains( "endTriggerRelation" ) ) {
            alarm->setEndOffset( Duration( offset.toInt(), Duration::Seconds ) );
          }
        }
        QString summary( list.at( i++ ) );
        QString description( list.at( i++ ) );
        QString attachments( list.at( i++ ) );
        //QString addresses( list.at( i++ ) ); // see selectAlarms
        //QString fullnames( list.at( i++ ) ); // see selectAlarms

        switch( alarm->type() ) {
        case Alarm::Display:
          alarm->setText( description );
          break;
        case Alarm::Procedure:
          alarm->setProgramFile( attachments );
          alarm->setProgramArguments( description );
          break;
        case Alarm::Email:
          alarm->setMailSubject( summary );
          alarm->setMailText( description );
          if ( !attachments.isEmpty() ) {
            alarm->setMailAttachments( attachments.split( ',' ) );
          }
          /* see selectAlarms
          if (!addresses.isEmpty()) {
            if (addresses.startsWith(QLatin1String("mailto:"), Qt::CaseInsensitive))
              addresses.remove(0, 7); // skip mailto:

            QList<Person> persons;
            QStringList emails = addresses.split(',');
            QStringList names = fullnames.split(',');
            for (int i = 0; i < emails.size(); i++)
              persons.append(Person(names.at(i), emails.at(i)));
            alarm->setMailAddresses(persons);
          }
          */
          break;
        case Alarm::Audio:
          alarm->setAudioFile(attachments);
          break;
        default:
          break;
        }

        kDebug() << "parseAlarm" << int( alarm->type() );

        return alarm;
      }
      return Alarm::Ptr();
    }

    Attachment::Ptr parseAttachment( const QStringList &list )
    {
      Attachment::Ptr attachment;

      if ( list.size() == 4 ) {
        int i = 0;

        QString base64( list.at( i++ ) );
        QString encoding( list.at( i++ ) );
        QString uri( list.at( i++ ) );
        QString fmttype( list.at( i++ ) );

        if ( !base64.isEmpty() && encoding.contains( "base64Encoding" ) ) {
          attachment = Attachment::Ptr( new Attachment( base64, fmttype ) );
          kDebug() << "parseAttachment (base64)";
        } else {
          attachment = Attachment::Ptr( new Attachment( uri, fmttype ) );
          kDebug() << "parseAttachment" << attachment->uri();
        }
      }
      return attachment;
    }

    RecurrenceRule *parseRecurrence( const QStringList &list )
    {
      if ( list.size() == 15 ) {
        int i = 0;

        RecurrenceRule *rule = new RecurrenceRule();
        rule->setRecurrenceType( getFrequency( list.at( i++ ) ) );
        if ( !list.at( i ).isEmpty() ) {
          rule->setFrequency( list.at( i ).toInt() );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          rule->setWeekStart( getDaypos( list.at( i ) ) );
        }
        i++;

        QList<int> byList;
        QStringList byL;
        QStringList byL2;
        QString by;
        QString by2;
        // BYDAY is a special case, since it's not an int list
        QList<RecurrenceRule::WDayPos> wdList;
        RecurrenceRule::WDayPos pos;
        wdList.clear();
        byList.clear();
        by2 = list.at( i++ ); // pos
        by = list.at( i++ );  // day
        if ( !by.isEmpty() ) {
          byL = by.split( ' ' );
          if ( !by2.isEmpty() ) {
            byL2 = by2.split( ' ' );
          }
          for ( int i = 0; i < byL.size(); ++i ) {
            if ( !by2.isEmpty() ) {
              pos.setDay( getDaypos( byL.at( i ) ) );
              pos.setPos( byL2.at( i ).toInt() );
              wdList.append( pos );
            } else {
              pos.setDay( getDaypos( byL.at( i ) ) );
              wdList.append( pos );
            }
          }
          if ( !wdList.isEmpty() ) {
            rule->setByDays( wdList );
          }
        }

#define readSetByList( field, setfunc )                                 \
        by = list.at( field );                                          \
        if ( !by.isEmpty() ) {                                          \
          byList.clear();                                               \
          byL = by.split( ' ' );                                        \
          for ( QStringList::Iterator it = byL.begin(); it != byL.end(); ++it ) {  \
            byList.append( (*it).toInt() );                             \
          }                                                             \
          if ( !byList.isEmpty() ) {                                    \
            rule->setfunc( byList );                                    \
          }                                                             \
        }

        // BYSECOND, MINUTE and HOUR, MONTHDAY, YEARDAY, WEEKNUMBER, MONTH
        // and SETPOS are standard int lists, so we can treat them with the
        // same macro
        readSetByList( i++, setByHours );
        readSetByList( i++, setByMinutes );
        readSetByList( i++, setByMonths );
        readSetByList( i++, setByMonthDays );
        readSetByList( i++, setBySeconds );
        readSetByList( i++, setBySetPos );
        readSetByList( i++, setByWeekNumbers );
        readSetByList( i++, setByYearDays );

#undef readSetByList

        if ( !list.at( i ).isEmpty() ) {
          rule->setDuration( list.at( i ).toInt() );
        }
        i++;
        if ( !list.at( i ).isEmpty() ) {
          rule->setEndDt( KDateTime::fromString( list.at( i ) ).toUtc() );
        }
        i++;

        kDebug() << "parseRecurrence";

        return rule;
      }
      return 0;
    }
    TrackerStorage *mStorage;
    TrackerFormat *mFormat;
    QDBusInterface *mTracker;
    bool mSynchronuousMode;
    KDateTime::Spec mTimeSpec;

    DBOperation mOperation;
    QHash<Incidence::Ptr,QString> *mOperationList;
    QHash<Incidence::Ptr,QString>::const_iterator mOperationListIterator;
    int mOperationState;
    bool mOperationError;
    QString mOperationErrorMessage;
    QDBusPendingCallWatcher *mOperationInsertWatcher;
    QDBusPendingCallWatcher *mOperationDeleteWatcher;
    QDBusPendingCallWatcher *mOperationEventsWatcher;
    QDBusPendingCallWatcher *mOperationTodosWatcher;
    QDBusPendingCallWatcher *mOperationJournalsWatcher;
    QDBusPendingCallWatcher *mOperationRDatesWatcher;
    QDBusPendingCallWatcher *mOperationExDatesWatcher;
    QDBusPendingCallWatcher *mOperationAttendeesWatcher;
    QDBusPendingCallWatcher *mOperationAlarmsWatcher;
    QDBusPendingCallWatcher *mOperationAttachmentsWatcher;
    QDBusPendingCallWatcher *mOperationRRecurrencesWatcher;
    QDBusPendingCallWatcher *mOperationExRecurrencesWatcher;

    bool selectComponentDetails();
    void selectRDates( Incidence::Ptr incidence );
    void selectExDates( Incidence::Ptr incidence );
    void selectAttendees( Incidence::Ptr incidence );
    void selectAlarms( Incidence::Ptr incidence );
    void selectAttachments( Incidence::Ptr incidence );
    void selectRecurrences( Incidence::Ptr incidence );
    void continueSelectComponentDetails();
    void continueSelectComponents();
    void continueModifyComponents();

  public slots:
    void selectRRecurrencesFinished( QDBusPendingCallWatcher *watcher );
    void selectExRecurrencesFinished( QDBusPendingCallWatcher *watcher );
    void selectRDatesFinished( QDBusPendingCallWatcher *watcher );
    void selectExDatesFinished( QDBusPendingCallWatcher *watcher );
    void selectAttendeesFinished( QDBusPendingCallWatcher *watcher );
    void selectAlarmsFinished( QDBusPendingCallWatcher *watcher );
    void selectAttachmentsFinished( QDBusPendingCallWatcher *watcher );
    void modifyInsertFinished( QDBusPendingCallWatcher *watcher );
    void modifyDeleteFinished( QDBusPendingCallWatcher *watcher );
    void selectEventsFinished( QDBusPendingCallWatcher *watcher );
    void selectTodosFinished( QDBusPendingCallWatcher *watcher );
    void selectJournalsFinished( QDBusPendingCallWatcher *watcher );
};
//@endcond

TrackerFormat::TrackerFormat( TrackerStorage *storage, QDBusInterface *tracker,
                              bool synchronuousMode )
  : d( new Private( storage, this, tracker, synchronuousMode ) )
{
}

TrackerFormat::~TrackerFormat()
{
  cancel();

  delete d;
}

void TrackerFormat::cancel()
{
  if (d->mOperationInsertWatcher) {
    delete d->mOperationInsertWatcher;
    d->mOperationInsertWatcher = 0;
  }
  if (d->mOperationDeleteWatcher) {
    delete d->mOperationDeleteWatcher;
    d->mOperationDeleteWatcher = 0;
  }
  if (d->mOperationEventsWatcher) {
    delete d->mOperationEventsWatcher;
    d->mOperationEventsWatcher = 0;
  }
  if (d->mOperationTodosWatcher) {
    delete d->mOperationTodosWatcher;
    d->mOperationTodosWatcher = 0;
  }
  if (d->mOperationJournalsWatcher) {
    delete d->mOperationJournalsWatcher;
    d->mOperationJournalsWatcher = 0;
  }
  if (d->mOperationRDatesWatcher) {
    delete d->mOperationRDatesWatcher;
    d->mOperationRDatesWatcher = 0;
  }
  if (d->mOperationExDatesWatcher) {
    delete d->mOperationExDatesWatcher;
    d->mOperationExDatesWatcher = 0;
  }
  if (d->mOperationAttendeesWatcher) {
    delete d->mOperationAttendeesWatcher;
    d->mOperationAttendeesWatcher = 0;
  }
  if (d->mOperationAlarmsWatcher) {
    delete d->mOperationAlarmsWatcher;
    d->mOperationAlarmsWatcher = 0;
  }
  if (d->mOperationAttachmentsWatcher) {
    delete d->mOperationAttachmentsWatcher;
    d->mOperationAttachmentsWatcher = 0;
  }
  if (d->mOperationRRecurrencesWatcher) {
    delete d->mOperationRRecurrencesWatcher;
    d->mOperationRRecurrencesWatcher = 0;
  }
  if (d->mOperationExRecurrencesWatcher) {
    delete d->mOperationExRecurrencesWatcher;
    d->mOperationExRecurrencesWatcher = 0;
  }
}

bool TrackerFormat::modifyComponents( QHash<Incidence::Ptr,QString> *list, DBOperation dbop )
{
  d->mOperation = dbop;
  d->mOperationState = 0;
  d->mOperationError = false;
  d->mOperationList = list;
  d->mOperationListIterator = d->mOperationList->constBegin();

  if ( d->mSynchronuousMode ) {
    while ( d->mOperationListIterator != d->mOperationList->constEnd() ) {
      // Process next incidence.
      if ( !modifyComponent( d->mOperationListIterator.key(), d->mOperationListIterator.value(),
                             d->mOperation ) ) {
        d->mStorage->saved( true, d->mOperationErrorMessage );
        return false;
      }
      d->mStorage->saved( d->mOperationListIterator.key() );
      d->mOperationListIterator++;
    }
    d->mStorage->saved( false, "save completed" );
  } else {
    if ( d->mOperationListIterator != d->mOperationList->constEnd() ) {
      // Process next incidence.
      modifyComponent( d->mOperationListIterator.key(), d->mOperationListIterator.value(),
                       d->mOperation );
    } else {
      // Nothing to process.
      d->mStorage->saved( false, "save completed" );
    }
  }
  return true;
}

bool TrackerFormat::modifyComponent( const Incidence::Ptr &incidence, const QString &notebook,
                                     DBOperation dbop )
{
  QStringList insertQuery;
  QStringList deleteQuery;

  TrackerModify modify;

  if ( !modify.queries( incidence, dbop, insertQuery, deleteQuery, notebook ) ) {
    kError() << "cannot build modify queries for" << incidence->uid();
    return false;
  }
  if ( dbop != DBInsert ) {
    QString query = deleteQuery.join( QString() );
#ifndef QT_NO_DEBUG
    // Use cerr to print only queries.
    cerr << endl << query.toAscii().constData() << endl;
    //kDebug() << "tracker query:" << select;
#endif
    QDBusPendingCall call = d->mTracker->asyncCall( "SparqlUpdate", query );
    if ( d->mOperationDeleteWatcher ) {
      delete d->mOperationDeleteWatcher;
    }
    d->mOperationDeleteWatcher = new QDBusPendingCallWatcher( call );
    if ( d->mSynchronuousMode ) {
      d->mOperationDeleteWatcher->waitForFinished();
      d->modifyDeleteFinished( d->mOperationDeleteWatcher );
      if ( d->mOperationError ) {
        return false;
      }
    } else {
      connect( d->mOperationDeleteWatcher,
               SIGNAL(finished(QDBusPendingCallWatcher*)),
               this, SLOT(d->modifyDeleteFinished(QDBusPendingCallWatcher*)) );
    }
  }
  QString query = insertQuery.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << query.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = d->mTracker->asyncCall( "SparqlUpdate", query );
  if ( d->mOperationInsertWatcher ) {
    delete d->mOperationInsertWatcher;
  }
  d->mOperationInsertWatcher = new QDBusPendingCallWatcher( call );
  if ( d->mSynchronuousMode ) {
    d->mOperationInsertWatcher->waitForFinished();
    d->modifyInsertFinished( d->mOperationInsertWatcher );
    if ( d->mOperationError ) {
      return false;
    }
  } else {
    connect( d->mOperationInsertWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             this, SLOT(d->modifyInsertFinished(QDBusPendingCallWatcher*)) );
  }
  return true;
}

//@cond PRIVATE
void TrackerFormat::Private::modifyInsertFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply< > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  }
  if ( !mSynchronuousMode ) {
    continueModifyComponents();
  }
}

void TrackerFormat::Private::modifyDeleteFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply< > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  }
  if ( !mSynchronuousMode ) {
    continueModifyComponents();
  }
}

void TrackerFormat::Private::continueModifyComponents()
{
  mOperationState++;
  if ( mOperation == DBInsert || mOperationState > 1 ) {
    if ( mOperationError ) {
      // Error, don't continue.
      mStorage->saved( true, mOperationErrorMessage );
    } else {
      // All queries and responses processed for this incidence.
      mStorage->saved( mOperationListIterator.key() );
      mOperationState = 0;
      mOperationListIterator++;
      if ( mOperationListIterator != mOperationList->constEnd() ) {
        // Process next incidence.
        mFormat->modifyComponent( mOperationListIterator.key(), mOperationListIterator.value(),
                                  mOperation );
      } else {
        // No more incidences to process.
        mStorage->saved( false, "save completed" );
      }
    }
  }
}
//@endcond

bool TrackerFormat::selectComponents( QHash<Incidence::Ptr,QString> *list,
                                      const QDate &start, const QDate &end,
                                      DBOperation dbop, const KDateTime &after,
                                      const QString &notebook, const QString &uid,
                                      const Incidence::Ptr &incidence )
{
  QStringList query;
  QStringList equery;
  QStringList tquery;
  QStringList jquery;

  if ( dbop == DBSelectRecurring || dbop == DBSelectAttendee ) {
    equery << "SELECT DISTINCT ?event ?dtstart ?dtstartzone ?dtend ?dtendzone ?transp ?summary ?categories ?contact ?duration ?class ?location ?description ?status ?geo ?priority ?resources ?dtstamp ?created ?lastModified ?sequence ?comment ?recurrenceId ?recurrenceIdzone ?relatedToParent ?url ?uid ?calendar ?organizerName ?organizerEmail WHERE {";
    tquery << "SELECT DISTINCT ?todo ?dtstart ?dtstartzone ?due ?duezone ?completed ?percentComplete ?summary ?categories ?contact ?duration ?class ?location ?description ?status ?geo ?priority ?resources ?dtstamp ?created ?lastModified ?sequence ?comment ?recurrenceId ?recurrenceIdzone ?relatedToParent ?url ?uid ?calendar ?organizerName ?organizerEmail WHERE {";
    jquery << "SELECT DISTINCT ?journal ?dtstart ?dtstartzone ?summary ?categories ?contact ?class ?description ?status ?geo ?dtstamp ?created ?lastModified ?sequence ?comment ?recurrenceId ?recurrenceIdzone ?relatedToParent ?url ?uid ?calendar ?organizerName ?organizerEmail WHERE {";
  } else {
    equery << "SELECT ?event ?dtstart ?dtstartzone ?dtend ?dtendzone ?transp ?summary ?categories ?contact ?duration ?class ?location ?description ?status ?geo ?priority ?resources ?dtstamp ?created ?lastModified ?sequence ?comment ?recurrenceId ?recurrenceIdzone ?relatedToParent ?url ?uid ?calendar ?organizerName ?organizerEmail WHERE {";
    tquery << "SELECT ?todo ?dtstart ?dtstartzone ?due ?duezone ?completed ?percentComplete ?summary ?categories ?contact ?duration ?class ?location ?description ?status ?geo ?priority ?resources ?dtstamp ?created ?lastModified ?sequence ?comment ?recurrenceId ?recurrenceIdzone ?relatedToParent ?url ?uid ?calendar ?organizerName ?organizerEmail WHERE {";
    jquery << "SELECT ?journal ?dtstart ?dtstartzone ?summary ?categories ?contact ?class ?description ?status ?geo ?dtstamp ?created ?lastModified ?sequence ?comment ?recurrenceId ?recurrenceIdzone ?relatedToParent ?url ?uid ?calendar ?organizerName ?organizerEmail WHERE {";
  }

  if ( uid.isEmpty() ) {
    equery << " ?event a ncal:Event";
    tquery << " ?todo a ncal:Todo";
    jquery << " ?journal a ncal:Journal";
    if ( incidence ) {
      QStringList duplicates;
      if ( incidence->dtStart().isValid() ) {
        duplicates << "; ncal:dtstart [ ncal:dateTime \"" << incidence->dtStart().toString() << "\"";
        if ( !incidence->dtStart().isUtc() ) {
          duplicates << "; ncal:ncalTimezone <urn:x-ical:timezone:"
                     << incidence->dtStart().timeZone().name() << ">";
        }
        duplicates << " ]";
      }
      if ( !incidence->summary().isEmpty() ) {
        duplicates << "; ncal:summary \"" << incidence->summary() << "\"";
      }

      if ( duplicates.size() == 0 ) {
        // Cannot search for duplicates.
        d->mStorage->loaded( false, "load completed" );
        return true;
      }
      equery << duplicates;
      tquery << duplicates;
      jquery << duplicates;
    }
  } else {
    equery << " <urn:x-ical:" << uid << "> ncal:url ?event";
    tquery << " <urn:x-ical:" << uid << "> ncal:url ?todo";
    jquery << " <urn:x-ical:" << uid << "> ncal:url ?journal";
  }

  if ( dbop == DBSelectRecurring ) {
    equery << " . OPTIONAL { ?event ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?event ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?event ncal:dtend [ ncal:dateTime ?dtend ] } . OPTIONAL { ?event ncal:dtend [ ncal:ncalTimezone ?dtendzone ] } . OPTIONAL { ?event ncal:transp ?transp } . OPTIONAL { ?event ncal:summary ?summary } . OPTIONAL { ?event ncal:categories ?categories } . OPTIONAL { ?event ncal:contact ?contact } . OPTIONAL { ?event ncal:duration ?duration } . OPTIONAL { ?event ncal:class ?class } . OPTIONAL { ?event ncal:location ?location } . OPTIONAL { ?event ncal:description ?description } . OPTIONAL { ?event ncal:eventStatus ?status } . OPTIONAL { ?event ncal:geo ?geo } . OPTIONAL { ?event ncal:priority ?priority } . OPTIONAL { ?event ncal:resources ?resources } . OPTIONAL { ?event ncal:dtstamp ?dtstamp } . OPTIONAL { ?event ncal:created ?created } . OPTIONAL { ?event ncal:lastModified ?lastModified } . OPTIONAL { ?event ncal:sequence ?sequence } . OPTIONAL { ?event ncal:comment ?comment } . OPTIONAL { ?event ncal:rrule ?rrule } . OPTIONAL { ?event ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?event ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?event ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?event ncal:url ?url } . OPTIONAL { ?event ncal:uid ?uid } . OPTIONAL { ?event nie:isLogicalPartOf ?calendar } . OPTIONAL { ?event ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
    tquery << " . OPTIONAL { ?todo ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?todo ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?todo ncal:due [ ncal:dateTime ?due ] } . OPTIONAL { ?todo ncal:due [ ncal:ncalTimezone ?duezone ] } . OPTIONAL { ?todo ncal:completed ?completed } . OPTIONAL { ?todo ncal:percentComplete ?percentComplete } . OPTIONAL { ?todo ncal:summary ?summary } . OPTIONAL { ?todo ncal:categories ?categories } . OPTIONAL { ?todo ncal:contact ?contact } . OPTIONAL { ?todo ncal:duration ?duration } . OPTIONAL { ?todo ncal:class ?class } . OPTIONAL { ?todo ncal:location ?location } . OPTIONAL { ?todo ncal:description ?description } . OPTIONAL { ?todo ncal:todoStatus ?status } . OPTIONAL { ?todo ncal:geo ?geo } . OPTIONAL { ?todo ncal:priority ?priority } . OPTIONAL { ?todo ncal:resources ?resources } . OPTIONAL { ?todo ncal:dtstamp ?dtstamp } . OPTIONAL {?todo ncal:created ?created } . OPTIONAL { ?todo ncal:lastModified ?lastModified } . OPTIONAL { ?todo ncal:sequence ?sequence } . OPTIONAL { ?todo ncal:comment ?comment } . OPTIONAL { ?todo ncal:rrule ?rrule } . OPTIONAL { ?todo ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?todo ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?todo ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?todo ncal:url ?url } . OPTIONAL { ?todo ncal:uid ?uid } . OPTIONAL { ?todo nie:isLogicalPartOf ?calendar } . OPTIONAL { ?todo ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
    jquery << " . OPTIONAL { ?journal ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?journal ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?journal ncal:summary ?summary } . OPTIONAL { ?journal ncal:categories ?categories } . OPTIONAL { ?journal ncal:contact ?contact } . OPTIONAL { ?journal ncal:class ?class } . OPTIONAL { ?journal ncal:description ?description } . OPTIONAL { ?journal ncal:journalStatus ?status } . OPTIONAL { ?journal ncal:geo ?geo } . OPTIONAL { ?journal ncal:dtstamp ?dtstamp } . OPTIONAL { ?journal ncal:created ?created } . OPTIONAL { ?journal ncal:lastModified ?lastModified } . OPTIONAL { ?journal ncal:sequence ?sequence } . OPTIONAL { ?journal ncal:comment ?comment } . OPTIONAL { ?journal ncal:rrule ?rrule } . OPTIONAL { ?journal ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?journal ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?journal ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?journal  ncal:url ?url } . OPTIONAL { ?journal ncal:uid ?uid } . OPTIONAL { ?journal nie:isLogicalPartOf ?calendar } . OPTIONAL { ?journal ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
  } else if ( dbop == DBSelectAttendee ) {
    equery << " . OPTIONAL { ?event ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?event ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?event ncal:dtend [ ncal:dateTime ?dtend ] } . OPTIONAL { ?event ncal:dtend [ ncal:ncalTimezone ?dtendzone ] } . OPTIONAL { ?event ncal:transp ?transp } . OPTIONAL { ?event ncal:summary ?summary } . OPTIONAL { ?event ncal:categories ?categories } . OPTIONAL { ?event ncal:contact ?contact } . OPTIONAL { ?event ncal:duration ?duration } . OPTIONAL { ?event ncal:class ?class } . OPTIONAL { ?event ncal:location ?location } . OPTIONAL { ?event ncal:description ?description } . OPTIONAL { ?event ncal:eventStatus ?status } . OPTIONAL { ?event ncal:geo ?geo } . OPTIONAL { ?event ncal:priority ?priority } . OPTIONAL { ?event ncal:resources ?resources } . OPTIONAL { ?event ncal:dtstamp ?dtstamp } . OPTIONAL { ?event ncal:created ?created } . OPTIONAL { ?event ncal:lastModified ?lastModified } . OPTIONAL { ?event ncal:sequence ?sequence } . OPTIONAL { ?event ncal:comment ?comment } . OPTIONAL { ?event ncal:attendee ?attendee }  . OPTIONAL { ?event ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?event ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?event ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?event ncal:url ?url } . OPTIONAL { ?event ncal:uid ?uid } . OPTIONAL { ?event nie:isLogicalPartOf ?calendar } . OPTIONAL { ?event ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
    tquery << " . OPTIONAL { ?todo ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?todo ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?todo ncal:due [ ncal:dateTime ?due ] } . OPTIONAL { ?todo ncal:due [ ncal:ncalTimezone ?duezone ] } . OPTIONAL { ?todo ncal:completed ?completed } . OPTIONAL { ?todo ncal:percentComplete ?percentComplete } . OPTIONAL { ?todo ncal:summary ?summary } . OPTIONAL { ?todo ncal:categories ?categories } . OPTIONAL { ?todo ncal:contact ?contact } . OPTIONAL { ?todo ncal:duration ?duration } . OPTIONAL { ?todo ncal:class ?class } . OPTIONAL { ?todo ncal:location ?location } . OPTIONAL { ?todo ncal:description ?description } . OPTIONAL { ?todo ncal:todoStatus ?status } . OPTIONAL { ?todo ncal:geo ?geo } . OPTIONAL { ?todo ncal:priority ?priority } . OPTIONAL { ?todo ncal:resources ?resources } . OPTIONAL { ?todo ncal:dtstamp ?dtstamp } . OPTIONAL {?todo ncal:created ?created } . OPTIONAL { ?todo ncal:lastModified ?lastModified } . OPTIONAL { ?todo ncal:sequence ?sequence } . OPTIONAL { ?todo ncal:comment ?comment } . OPTIONAL { ?todo ncal:attendee ?attendee }  . OPTIONAL { ?todo ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?todo ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?todo ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?todo ncal:url ?url } . OPTIONAL { ?todo ncal:uid ?uid } . OPTIONAL { ?todo nie:isLogicalPartOf ?calendar } . OPTIONAL { ?todo ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
    jquery << " . OPTIONAL { ?journal ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?journal ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?journal ncal:summary ?summary } . OPTIONAL { ?journal ncal:categories ?categories } . OPTIONAL { ?journal ncal:contact ?contact } . OPTIONAL { ?journal ncal:class ?class } . OPTIONAL { ?journal ncal:description ?description } . OPTIONAL { ?journal ncal:journalStatus ?status } . OPTIONAL { ?journal ncal:geo ?geo } . OPTIONAL { ?journal ncal:dtstamp ?dtstamp } . OPTIONAL { ?journal ncal:created ?created } . OPTIONAL { ?journal ncal:lastModified ?lastModified } . OPTIONAL { ?journal ncal:sequence ?sequence } . OPTIONAL { ?journal ncal:comment ?comment } . OPTIONAL { ?journal ncal:attendee ?attendee }  . OPTIONAL { ?journal ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?journal ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?journal ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?journal  ncal:url ?url } . OPTIONAL { ?journal ncal:uid ?uid } . OPTIONAL { ?journal nie:isLogicalPartOf ?calendar } . OPTIONAL { ?journal ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
  } else {
    equery << " . OPTIONAL { ?event ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?event ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?event ncal:dtend [ ncal:dateTime ?dtend ] } . OPTIONAL { ?event ncal:dtend [ ncal:ncalTimezone ?dtendzone ] } . OPTIONAL { ?event ncal:transp ?transp } . OPTIONAL { ?event ncal:summary ?summary } . OPTIONAL { ?event ncal:categories ?categories } . OPTIONAL { ?event ncal:contact ?contact } . OPTIONAL { ?event ncal:duration ?duration } . OPTIONAL { ?event ncal:class ?class } . OPTIONAL { ?event ncal:location ?location } . OPTIONAL { ?event ncal:description ?description } . OPTIONAL { ?event ncal:eventStatus ?status } . OPTIONAL { ?event ncal:geo ?geo } . OPTIONAL { ?event ncal:priority ?priority } . OPTIONAL { ?event ncal:resources ?resources } . OPTIONAL { ?event ncal:dtstamp ?dtstamp } . OPTIONAL { ?event ncal:created ?created } . OPTIONAL { ?event ncal:lastModified ?lastModified } . OPTIONAL { ?event ncal:sequence ?sequence } . OPTIONAL { ?event ncal:comment ?comment } . OPTIONAL { ?event ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?event ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?event ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?event ncal:url ?url } . OPTIONAL { ?event ncal:uid ?uid } . OPTIONAL { ?event nie:isLogicalPartOf ?calendar } . OPTIONAL { ?event ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
    tquery << " . OPTIONAL { ?todo ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?todo ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?todo ncal:due [ ncal:dateTime ?due ] } . OPTIONAL { ?todo ncal:due [ ncal:ncalTimezone ?duezone ] } . OPTIONAL { ?todo ncal:completed ?completed } . OPTIONAL { ?todo ncal:percentComplete ?percentComplete } . OPTIONAL { ?todo ncal:summary ?summary } . OPTIONAL { ?todo ncal:categories ?categories } . OPTIONAL { ?todo ncal:contact ?contact } . OPTIONAL { ?todo ncal:duration ?duration } . OPTIONAL { ?todo ncal:class ?class } . OPTIONAL { ?todo ncal:location ?location } . OPTIONAL { ?todo ncal:description ?description } . OPTIONAL { ?todo ncal:todoStatus ?status } . OPTIONAL { ?todo ncal:geo ?geo } . OPTIONAL { ?todo ncal:priority ?priority } . OPTIONAL { ?todo ncal:resources ?resources } . OPTIONAL { ?todo ncal:dtstamp ?dtstamp } . OPTIONAL {?todo ncal:created ?created } . OPTIONAL { ?todo ncal:lastModified ?lastModified } . OPTIONAL { ?todo ncal:sequence ?sequence } . OPTIONAL { ?todo ncal:comment ?comment } . OPTIONAL { ?todo ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?todo ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?todo ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?todo ncal:url ?url } . OPTIONAL { ?todo ncal:uid ?uid } . OPTIONAL { ?todo nie:isLogicalPartOf ?calendar } . OPTIONAL { ?todo ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
    jquery << " . OPTIONAL { ?journal ncal:dtstart [ ncal:dateTime ?dtstart ] } . OPTIONAL { ?journal ncal:dtstart [ ncal:ncalTimezone ?dtstartzone ] } . OPTIONAL { ?journal ncal:summary ?summary } . OPTIONAL { ?journal ncal:categories ?categories } . OPTIONAL { ?journal ncal:contact ?contact } . OPTIONAL { ?journal ncal:class ?class } . OPTIONAL { ?journal ncal:description ?description } . OPTIONAL { ?journal ncal:journalStatus ?status } . OPTIONAL { ?journal ncal:geo ?geo } . OPTIONAL { ?journal ncal:dtstamp ?dtstamp } . OPTIONAL { ?journal ncal:created ?created } . OPTIONAL { ?journal ncal:lastModified ?lastModified } . OPTIONAL { ?journal ncal:sequence ?sequence } . OPTIONAL { ?journal ncal:comment ?comment } . OPTIONAL { ?journal ncal:recurrenceId [ ncal:dateTime ?recurrenceId ] } . OPTIONAL { ?journal ncal:recurrenceId [ ncal:ncalTimezone ?recurrenceIdzone ] } . OPTIONAL { ?journal ncal:relatedToParent ?relatedToParent } . OPTIONAL { ?journal  ncal:url ?url } . OPTIONAL { ?journal ncal:uid ?uid } . OPTIONAL { ?journal nie:isLogicalPartOf ?calendar } . OPTIONAL { ?journal ncal:organizer [ ncal:involvedContact [ nco:fullname ?organizerName; nco:hasEmailAddress ?organizerEmail ] ] }";
  }

  if ( !notebook.isEmpty() ) {
    equery << " . ?event nie:isLogicalPartOf \"<urn:x-ical:" << notebook << ">\"";
    tquery << " . ?todo nie:isLogicalPartOf \"<urn:x-ical:" << notebook << ">\"";
    jquery << " . ?journal nie:isLogicalPartOf \"<urn:x-ical:" << notebook << ">\"";
  }

  if ( start.isValid() ) {
    KDateTime kdate( start, KDateTime::Spec( KDateTime::UTC ) );
    equery << " . FILTER ( ?dtstart >= \"" << kdate.toString() << "\"^^xsd:dateTime )";
    tquery << " . FILTER ( ?dtstart >= \"" << kdate.toString() << "\"^^xsd:dateTime )";
    jquery << " . FILTER ( ?dtstart >= \"" << kdate.toString() << "\"^^xsd:dateTime )";
  }
  if ( end.isValid() ) {
    KDateTime kdate( end, KDateTime::Spec( KDateTime::UTC ) );
    equery << " . FILTER ( ?dtstart <= \"" << kdate.toString() << "\"^^xsd:dateTime )";
    tquery << " . FILTER ( ?dtstart <= \"" << kdate.toString() << "\"^^xsd:dateTime )";
    jquery << " . FILTER ( ?dtstart <= \"" << kdate.toString() << "\"^^xsd:dateTime )";
  }
  if ( after.isValid() ) {
    switch( dbop ) {
    case DBInsert:
      equery << " . FILTER ( ?created > \"" << after.toUtc().toString() << "\"^^xsd:dateTime )";
      tquery << " . FILTER ( ?created > \"" << after.toUtc().toString() << "\"^^xsd:dateTime )";
      jquery << " . FILTER ( ?created > \"" << after.toUtc().toString() << "\"^^xsd:dateTime )";
      break;
    case DBUpdate:
      equery << " . FILTER ( ?lastModified > \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime && \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime >= ?created )" ;
      tquery << " . FILTER ( ?lastModified > \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime && \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime >= ?created )" ;
      jquery << " . FILTER ( ?lastModified > \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime && \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime >= ?created )" ;
      break;
    case DBDelete:
      equery << " . FILTER ( ?lastModified > \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime && \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime >= ?created )" ;
      tquery << " . FILTER ( ?lastModified > \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime && \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime >= ?created )" ;
      jquery << " . FILTER ( ?lastModified > \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime && \"" << after.toUtc().toString()
             << "\"^^xsd:dateTime >= ?created )" ;
      break;
    default:
      break;
    }
  } else {
    switch( dbop ) {
    case DBSelectPlain:
      equery << " . FILTER ( !bound(?dtstart) && !bound(?dtend) )";
      tquery << " . FILTER ( !bound(?dtstart) && !bound(?due) )";
      jquery << " . FILTER ( !bound(?dtstart) )";
      break;
    case DBSelectGeo:
      equery << " . FILTER ( bound(?geo) )";
      tquery << " . FILTER ( bound(?geo) )";
      jquery << " . FILTER ( bound(?geo) )";
      break;
    case DBSelectRecurring:
      equery << " . FILTER ( bound(?recurrenceId) || bound(?rrule) )";
      tquery << " . FILTER ( bound(?recurrenceId) || bound(?rrule) )";
      jquery << " . FILTER ( bound(?recurrenceId) || bound(?rrule) )";
      break;
    case DBSelectAttendee:
      equery << " . FILTER ( bound(?attendee) )";
      tquery << " . FILTER ( bound(?attendee) )";
      jquery << " . FILTER ( bound(?attendee) )";
      break;
    default:
      break;
    }
  }

  equery << " }";
  tquery << " }";
  jquery << " }";

  d->mOperation = dbop;
  d->mOperationList = list;
  d->mOperationState = 0;
  d->mOperationError = false;

  // Events.
  QString select = equery.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = d->mTracker->asyncCall( "SparqlQuery", select );
  if ( d->mOperationEventsWatcher ) {
    delete d->mOperationEventsWatcher;
  }
  d->mOperationEventsWatcher = new QDBusPendingCallWatcher( call );
  if ( d->mSynchronuousMode ) {
    d->mOperationEventsWatcher->waitForFinished();
    d->selectEventsFinished( d->mOperationEventsWatcher );
    if ( d->mOperationError ) {
      // Error, don't continue.
      d->mStorage->loaded( true, d->mOperationErrorMessage );
      return false;
    }
  } else {
    connect( d->mOperationEventsWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             this, SLOT(selectEventsFinished(QDBusPendingCallWatcher*)) );
  }

  // Todos.
  select = tquery.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  call = d->mTracker->asyncCall( "SparqlQuery", select );
  if ( d->mOperationTodosWatcher ) {
    delete d->mOperationTodosWatcher;
  }
  d->mOperationTodosWatcher = new QDBusPendingCallWatcher( call );
  if ( d->mSynchronuousMode ) {
    d->mOperationTodosWatcher->waitForFinished();
    d->selectTodosFinished( d->mOperationTodosWatcher );
    if ( d->mOperationError ) {
      // Error, don't continue.
      d->mStorage->loaded( true, d->mOperationErrorMessage );
      return false;
    }
  } else {
    connect( d->mOperationTodosWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             this, SLOT(d->selectTodosFinished(QDBusPendingCallWatcher*)) );
  }

  // Journals.
  select = jquery.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  call = d->mTracker->asyncCall( "SparqlQuery", select );
  if ( d->mOperationJournalsWatcher ) {
    delete d->mOperationJournalsWatcher;
  }
  d->mOperationJournalsWatcher = new QDBusPendingCallWatcher( call );
  if ( d->mSynchronuousMode ) {
    d->mOperationJournalsWatcher->waitForFinished();
    d->selectJournalsFinished( d->mOperationJournalsWatcher );
    if ( d->mOperationError ) {
      // Error, don't continue.
      d->mStorage->loaded( true, d->mOperationErrorMessage );
      return false;
    }
    // Finally select component details.
    d->mOperationListIterator = d->mOperationList->constBegin();
    while ( d->selectComponentDetails() ) {
      if ( d->mOperationError ) {
        d->mStorage->loaded( true, d->mOperationErrorMessage );
        return false;
      }
      d->mStorage->loaded( d->mOperationListIterator.key() );
      d->mOperationListIterator++;
    }
    d->mStorage->loaded( false, "load completed" );
  } else {
    connect( d->mOperationJournalsWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             this, SLOT(d->selectJournalsFinished(QDBusPendingCallWatcher*)) );
  }
  return true;
}

//@cond PRIVATE
void TrackerFormat::Private::selectEventsFinished( QDBusPendingCallWatcher *watcher )
{
  QString notebook;
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      Event::Ptr event = parseEvent( vector.at( i ), notebook );
      if ( event ) {
        if ( ( event->created().isValid() && mOperation != DBDelete ) ||
             ( !( event->created().isValid() ) && mOperation == DBDelete ) ) {
          // Not deleted (or if it is, we want it).
          mOperationList->insert( event, notebook );
        }
      }
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponents();
  }
}

void TrackerFormat::Private::selectTodosFinished( QDBusPendingCallWatcher *watcher )
{
  QString notebook;
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      Todo::Ptr todo = parseTodo( vector.at( i ), notebook );
      if ( todo ) {
        if ( ( todo->created().isValid() && mOperation != DBDelete ) ||
             ( !( todo->created().isValid() ) && mOperation == DBDelete ) ) {
          // Not deleted (or if it is, we want it).
          mOperationList->insert( todo, notebook );
        }
      }
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponents();
  }
}

void TrackerFormat::Private::selectJournalsFinished( QDBusPendingCallWatcher *watcher )
{
  QString notebook;
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      Journal::Ptr journal = parseJournal( vector.at( i ), notebook );
      if ( journal ) {
        if ( ( journal->created().isValid() && mOperation != DBDelete ) ||
             ( !( journal->created().isValid() ) && mOperation == DBDelete ) ) {
          // Not deleted (or if it is, we want it).
          mOperationList->insert( journal, notebook );
        }
      }
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponents();
  }
}

void TrackerFormat::Private::continueSelectComponents()
{
  mOperationState++;
  if ( mOperationState == 3 ) {
    if ( mOperationError ) {
      // Error, don't continue.
      mStorage->loaded( true, mOperationErrorMessage );
    } else {
      // Events, todos and journals queried and processed.
      mOperationState = 0;
      mOperationListIterator = mOperationList->constBegin();
      if ( !selectComponentDetails() ) {
        // No components to process.
        mStorage->loaded( false, "no incidences to load" );
      }
    }
  }
}

bool TrackerFormat::Private::selectComponentDetails()
{
  if ( mOperationListIterator != mOperationList->constEnd() ) {
    selectRDates( mOperationListIterator.key() );
    selectExDates( mOperationListIterator.key() );
    selectAttendees( mOperationListIterator.key() );
    selectAlarms( mOperationListIterator.key() );
    selectAttachments( mOperationListIterator.key() );
    selectRecurrences( mOperationListIterator.key() );
    return true;
  }
  return false;
}

void TrackerFormat::Private::selectRDates( Incidence::Ptr incidence )
{
  QStringList query;

  query << "SELECT ?datetime ?timezone WHERE { <" << incidence->uri().toString()
        << "> a ncal:UnionParentClass; ncal:rdate ?rdate . ?rdate ncal:dateTime ?datetime . OPTIONAL { ?rdate ncal:ncalTimezone ?timezone } }";

  QString select = query.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = mTracker->asyncCall( "SparqlQuery", select );
  if ( mOperationRDatesWatcher ) {
    delete mOperationRDatesWatcher;
  }
  mOperationRDatesWatcher = new QDBusPendingCallWatcher( call );
  if ( mSynchronuousMode ) {
    mOperationRDatesWatcher->waitForFinished();
    selectRDatesFinished( mOperationRDatesWatcher );
  } else {
    connect( mOperationRDatesWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             mFormat, SLOT(selectRDatesFinished(QDBusPendingCallWatcher*)) );
  }
}

void TrackerFormat::Private::selectRDatesFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      parseRDate( vector.at( i ), mOperationListIterator.key() );
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponentDetails();
  }
}

void TrackerFormat::Private::selectExDates( Incidence::Ptr incidence )
{
  QStringList query;

  query << "SELECT ?datetime ?timezone WHERE { <" << incidence->uri().toString()
        << "> a ncal:UnionParentClass; ncal:exdate ?exdate . ?exdate ncal:dateTime ?datetime . OPTIONAL { ?exdate ncal:ncalTimezone ?timezone } }";

  QString select = query.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = mTracker->asyncCall( "SparqlQuery", select );
  if ( mOperationExDatesWatcher ) {
    delete mOperationExDatesWatcher;
  }
  mOperationExDatesWatcher = new QDBusPendingCallWatcher( call );
  if ( mSynchronuousMode ) {
    mOperationExDatesWatcher->waitForFinished();
    selectExDatesFinished( mOperationExDatesWatcher );
  } else {
    connect( mOperationExDatesWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             mFormat, SLOT(selectExDatesFinished(QDBusPendingCallWatcher*)) );
  }
}

void TrackerFormat::Private::selectExDatesFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      parseExDate( vector.at( i ), mOperationListIterator.key() );
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponentDetails();
  }
}

void TrackerFormat::Private::selectAttendees( Incidence::Ptr incidence )
{
  QStringList query;

  query << "SELECT ?involvedContactFullname ?involvedContactEmailAddress ?delegatedFromEmailAddress ?delegatedToEmailAddress ?partstat ?role ?rsvp WHERE { <" << incidence->uri().toString()
        << "> a ncal:UnionParentClass; ncal:attendee ?attendee . ?attendee ncal:involvedContact ?involvedContact . ?involvedContact a nco:Contact; nco:fullname ?involvedContactFullname; nco:hasEmailAddress ?involvedContactEmailAddress . OPTIONAL { ?attendee ncal:delegatedFrom [ nco:hasEmailAddress ?delegatedFromEmailAddress ] } . OPTIONAL { ?attendee ncal:delegatedTo [ nco:hasEmailAddress ?delegatedToEmailAddress ] } . OPTIONAL { ?attendee ncal:partstat ?partstat } . OPTIONAL { ?attendee ncal:role ?role } . OPTIONAL { ?attendee ncal:rsvp ?rsvp } }";

  QString select = query.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = mTracker->asyncCall( "SparqlQuery", select );
  if ( mOperationAttendeesWatcher ) {
    delete mOperationAttendeesWatcher;
  }
  mOperationAttendeesWatcher = new QDBusPendingCallWatcher( call );
  if ( mSynchronuousMode ) {
    mOperationAttendeesWatcher->waitForFinished();
    selectAttendeesFinished( mOperationAttendeesWatcher );
  } else {
    connect( mOperationAttendeesWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             mFormat, SLOT(selectAttendeesFinished(QDBusPendingCallWatcher*)) );
  }
}

void TrackerFormat::Private::selectAttendeesFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      Attendee::Ptr attendee = parseAttendee( vector.at( i ) );
      if ( attendee ) {
        mOperationListIterator.key()->addAttendee( attendee, false );
      }
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponentDetails();
  }
}

void TrackerFormat::Private::selectAlarms( Incidence::Ptr incidence )
{
  QStringList query;

  // This should be the correct query, but the last OPTIONAL is so slow that
  // it has been left out. Make a tracker bug out of this one, if it is needed
  // and still slow.
  //query << "SELECT ?action ?repeat ?duration ?triggerDateTime ?related ?triggerDuration ?summary ?description ?attachments ?addresses ?fullnames WHERE { <" << incidence->uri().toString() << "> a ncal:UnionParentClass; ncal:hasAlarm ?alarm . ?alarm a ncal:Alarm; ncal:action ?action; ncal:trigger ?trigger . OPTIONAL { ?alarm  ncal:repeat ?repeat } . OPTIONAL { ?alarm ncal:duration ?duration } . OPTIONAL { ?trigger ncal:triggerDateTime ?triggerDateTime } . OPTIONAL { ?trigger ncal:related ?related; ncal:triggerDuration ?triggerDuration } . OPTIONAL { ?alarm ncal:summary ?summary } . OPTIONAL { ?alarm ncal:description ?description } . OPTIONAL { ?alarm ncal:attach [ ncal:attachmentUri ?attachments ] } . OPTIONAL { ?alarm ncal:attendee [ ncal:involvedContact [ nco:hasEmailAddress ?addresses; nco:fullname ?fullnames ] ] } }";

  query << "SELECT ?action ?repeat ?duration ?triggerDateTime ?related ?triggerDuration ?summary ?description ?attachments WHERE { <" << incidence->uri().toString() << "> a ncal:UnionParentClass; ncal:hasAlarm ?alarm . ?alarm a ncal:Alarm; ncal:action ?action; ncal:trigger ?trigger . OPTIONAL { ?alarm  ncal:repeat ?repeat } . OPTIONAL { ?alarm ncal:duration ?duration } . OPTIONAL { ?trigger ncal:triggerDateTime ?triggerDateTime } . OPTIONAL { ?trigger ncal:related ?related; ncal:triggerDuration ?triggerDuration } . OPTIONAL { ?alarm ncal:summary ?summary } . OPTIONAL { ?alarm ncal:description ?description } . OPTIONAL { ?alarm ncal:attach [ ncal:attachmentUri ?attachments ] } }";

  QString select = query.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = mTracker->asyncCall( "SparqlQuery", select );
  if ( mOperationAlarmsWatcher ) {
    delete mOperationAlarmsWatcher;
  }
  mOperationAlarmsWatcher = new QDBusPendingCallWatcher( call );
  if ( mSynchronuousMode ) {
    mOperationAlarmsWatcher->waitForFinished();
    selectAlarmsFinished( mOperationAlarmsWatcher );
  } else {
    connect( mOperationAlarmsWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             mFormat, SLOT(selectAlarmsFinished(QDBusPendingCallWatcher*)) );
  }
}

void TrackerFormat::Private::selectAlarmsFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      parseAlarm( vector.at( i ), mOperationListIterator.key() );
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponentDetails();
  }
}

void TrackerFormat::Private::selectAttachments( Incidence::Ptr incidence )
{
  QStringList query;

  query << "SELECT ?attachmentContent ?encoding ?attachmentUri ?fmttype WHERE { <" << incidence->uri().toString()
        << "> a ncal:UnionParentClass; ncal:attach ?attach . OPTIONAL { ?attach ncal:attachmentContent ?attachmentContent } . OPTIONAL { ?attach ncal:encoding ?encoding } . OPTIONAL { ?attach ncal:attachmentUri ?attachmentUri } . OPTIONAL { ?attach ncal:fmttype ?fmttype } }";

  QString select = query.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = mTracker->asyncCall( "SparqlQuery", select );
  if ( mOperationAttachmentsWatcher ) {
    delete mOperationAttachmentsWatcher;
  }
  mOperationAttachmentsWatcher = new QDBusPendingCallWatcher( call );
  if ( mSynchronuousMode ) {
    mOperationAttachmentsWatcher->waitForFinished();
    selectAttachmentsFinished( mOperationAttachmentsWatcher );
  } else {
    connect( mOperationAttachmentsWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             mFormat, SLOT(selectAttachmentsFinished(QDBusPendingCallWatcher*)) );
  }
}

void TrackerFormat::Private::selectAttachmentsFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      Attachment::Ptr attachment = parseAttachment( vector.at( i ) );
      if ( attachment ) {
        mOperationListIterator.key()->addAttachment( attachment );
      }
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponentDetails();
  }
}

void TrackerFormat::Private::selectRecurrences( Incidence::Ptr incidence )
{
  QStringList query1;
  QStringList query2;

  query1 << "SELECT ?freq ?interval ?wkst GROUP_CONCAT(?bydayModifier, ' ') AS daymodifierlist GROUP_CONCAT(?bydayWeekday, ' ') AS dayweeklist GROUP_CONCAT(?byhour, ' ') AS hourlist GROUP_CONCAT(?byminute, ' ') AS minutelist GROUP_CONCAT(?bymonth, ' ') AS monthlist GROUP_CONCAT(?bymonthday, ' ') AS monthdaylist GROUP_CONCAT(?bysecond, ' ') AS secondlist GROUP_CONCAT(?bysetpos, ' ') AS setposlist GROUP_CONCAT(?byweekno, ' ') AS weeknolist GROUP_CONCAT(?byyearday, ' ') AS yeardaylist ?count ?until WHERE { <" << incidence->uri().toString() << "> a ncal:UnionParentClass; ncal:rrule ?rrule . ?rrule a ncal:RecurrenceRule; . OPTIONAL { ?rrule ncal:freq ?freq } . OPTIONAL { ?rrule ncal:interval ?interval } . OPTIONAL { ?rrule ncal:wkst ?wkst } . OPTIONAL { ?rrule ncal:byday ?byday . ?byday a ncal:BydayRulePart; ncal:bydayModifier ?bydayModifier; ncal:bydayWeekday ?bydayWeekday } . OPTIONAL { ?rrule ncal:byhour ?byhour } . OPTIONAL { ?rrule ncal:byminute ?byminute } . OPTIONAL { ?rrule ncal:bymonth ?bymonth } . OPTIONAL { ?rrule ncal:bymonthday ?bymonthday } . OPTIONAL { ?rrule ncal:bysecond ?bysecond } . OPTIONAL { ?rrule ncal:bysetpos ?bysetpos } . OPTIONAL { ?rrule ncal:byweekno ?byweekno } . OPTIONAL { ?rrule ncal:byyearday ?byyearday } . OPTIONAL { ?rrule ncal:count ?count } . OPTIONAL { ?rrule ncal:until ?until } } GROUP BY ?rrule";

  QString select1 = query1.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select1.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  QDBusPendingCall call = mTracker->asyncCall( "SparqlQuery", select1 );
  if ( mOperationRRecurrencesWatcher ) {
    delete mOperationRRecurrencesWatcher;
  }
  mOperationRRecurrencesWatcher = new QDBusPendingCallWatcher( call );
  if ( mSynchronuousMode ) {
    mOperationRRecurrencesWatcher->waitForFinished();
    selectRRecurrencesFinished( mOperationRRecurrencesWatcher );
  } else {
    connect( mOperationRRecurrencesWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             mFormat, SLOT(selectRRecurrencesFinished(QDBusPendingCallWatcher*)) );
  }

  query2 << "SELECT ?freq ?interval ?wkst GROUP_CONCAT(?bydayModifier, ' ') AS daymodifierlist GROUP_CONCAT(?bydayWeekday, ' ') AS dayweeklist GROUP_CONCAT(?byhour, ' ') AS hourlist GROUP_CONCAT(?byminute, ' ') AS minutelist GROUP_CONCAT(?bymonth, ' ') AS monthlist GROUP_CONCAT(?bymonthday, ' ') AS monthdaylist GROUP_CONCAT(?bysecond, ' ') AS secondlist GROUP_CONCAT(?bysetpos, ' ') AS setposlist GROUP_CONCAT(?byweekno, ' ') AS weeknolist GROUP_CONCAT(?byyearday, ' ') AS yeardaylist ?count ?until WHERE { <" << incidence->uri().toString() << "> a ncal:UnionParentClass; ncal:exrule ?exrule . ?exrule a ncal:RecurrenceRule; . OPTIONAL { ?exrule ncal:freq ?freq } . OPTIONAL { ?exrule ncal:interval ?interval } . OPTIONAL { ?exrule ncal:wkst ?wkst } . OPTIONAL { ?exrule ncal:byday ?byday . ?byday a ncal:BydayRulePart; ncal:bydayModifier ?bydayModifier; ncal:bydayWeekday ?bydayWeekday } . OPTIONAL { ?exrule ncal:byhour ?byhour } . OPTIONAL { ?exrule ncal:byminute ?byminute } . OPTIONAL { ?exrule ncal:bymonth ?bymonth } . OPTIONAL { ?exrule ncal:bymonthday ?bymonthday } . OPTIONAL { ?exrule ncal:bysecond ?bysecond } . OPTIONAL { ?exrule ncal:bysetpos ?bysetpos } . OPTIONAL { ?exrule ncal:byweekno ?byweekno } . OPTIONAL { ?exrule ncal:byyearday ?byyearday } . OPTIONAL { ?exrule ncal:count ?count } . OPTIONAL { ?exrule ncal:until ?until } }  GROUP BY ?exrule";

  QString select2 = query2.join( QString() );
#ifndef QT_NO_DEBUG
  // Use cerr to print only queries.
  cerr << endl << select2.toAscii().constData() << endl;
  //kDebug() << "tracker query:" << select;
#endif
  call = mTracker->asyncCall( "SparqlQuery", select2 );
  if ( mOperationExRecurrencesWatcher ) {
    delete mOperationExRecurrencesWatcher;
  }
  mOperationExRecurrencesWatcher = new QDBusPendingCallWatcher( call );
  if ( mSynchronuousMode ) {
    mOperationExRecurrencesWatcher->waitForFinished();
    selectExRecurrencesFinished( mOperationExRecurrencesWatcher );
  } else {
    connect( mOperationExRecurrencesWatcher,
             SIGNAL(finished(QDBusPendingCallWatcher*)),
             mFormat, SLOT(selectExRecurrencesFinished(QDBusPendingCallWatcher*)) );
  }
}

void TrackerFormat::Private::selectRRecurrencesFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      RecurrenceRule *rule = parseRecurrence( vector.at( i ) );
      if ( rule ) {
        mOperationListIterator.key()->recurrence()->addRRule( rule );
      }
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponentDetails();
  }
}

void TrackerFormat::Private::selectExRecurrencesFinished( QDBusPendingCallWatcher *watcher )
{
  QDBusPendingReply<QVector<QStringList> > reply = *watcher;
  if ( reply.isError() ) {
    kError() << "tracker query error:" << reply.error().message();
    if ( !mOperationError ) {
      mOperationErrorMessage = reply.error().message();
    }
    mOperationError = true;
  } else {
    QVector<QStringList> vector = reply.value();
    for ( int i = 0; i < vector.size(); ++i ) {
      RecurrenceRule *rule = parseRecurrence( vector.at( i ) );
      if ( rule ) {
        mOperationListIterator.key()->recurrence()->addExRule( rule );
      }
    }
  }
  if ( !mSynchronuousMode ) {
    continueSelectComponentDetails();
  }
}

void TrackerFormat::Private::continueSelectComponentDetails()
{
  mOperationState++;
  if ( mOperationState == 7 ) {
    if ( mOperationError ) {
      // Error, don't continue.
      mStorage->loaded( true, mOperationErrorMessage );
    } else {
      // All queries and responses processed for this incidence.
      mStorage->loaded( mOperationListIterator.key() );
      mOperationState = 0;
      mOperationListIterator++;
      if ( !selectComponentDetails() ) {
        // No more incidences to process.
        mStorage->loaded( false, "load completed" );
      }
    }
  }
}
//@endcond

#endif
