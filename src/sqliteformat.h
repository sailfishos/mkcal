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
  defines the SqliteFormat class.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
*/

#ifndef MKCAL_SQLITEFORMAT_H
#define MKCAL_SQLITEFORMAT_H

#include "mkcal_export.h"
#include "extendedstorage.h"
#include "notebook.h"

#include <KCalendarCore/Incidence>

#include <sqlite3.h>

#include <QTimeZone>

namespace mKCal {

/**
  @brief
  Sqlite format implementation.

  This class implements the Sqlite format. It provides methods for
  loading/saving/converting Sqlite format data into the internal
  representation as Calendar and Incidences.
*/
class MKCAL_EXPORT SqliteFormat
{
public:
    /**
      The different types of rdates.
    */
    enum RDateType {
        RDate = 1,
        XDate,
        RDateTime,
        XDateTime
    };

    /**
      Values stored in the flag column of the calendars table.
     */
    enum CalendarFlag {
        AllowEvents   = (1 << 0),
        AllowJournals = (1 << 1),
        AllowTodos    = (1 << 2),
        Shared        = (1 << 3),
        Master        = (1 << 4),
        Synchronized  = (1 << 5),
        ReadOnly      = (1 << 6),
        Visible       = (1 << 7),
        RunTimeOnly   = (1 << 8),
        Default       = (1 << 9),
        Shareable     = (1 << 10)
    };

    /**
      Constructor a new Sqlite Format object.
    */
    SqliteFormat(sqlite3 *database, const QTimeZone &timeZone = {});

    /**
      Destructor.
    */
    virtual ~SqliteFormat();

    /**
      Ensure all tables exist.
    */
    bool init();

    /**
      Update notebook data in Calendars table.

      @param notebook notebook to update
      @param dbop database operation
      @param stmt prepared sqlite statement for calendars table
      @param isDefault if the notebook is the default one in the DB
      @return true if the operation was successful; false otherwise.
    */
    bool modifyCalendars(const Notebook &notebook, DBOperation dbop, sqlite3_stmt *stmt, bool isDefault);

    /**
      Select notebooks from Calendars table.

      @param stmt prepared sqlite statement for calendars table
      @param isDefault true if the selected notebook is the DB default one
      @return the queried notebook (caller take ownership).
    */
    Notebook* selectCalendars(sqlite3_stmt *stmt, bool *isDefault);

    /**
      Update incidence data in Components table.

      @param incidence incidence to update
      @param notebook notebook of incidence
      @param dbop database operation
      @return true if the operation was successful; false otherwise.
    */
    bool modifyComponents(const KCalendarCore::Incidence &incidence, const QString &notebook,
                          DBOperation dbop);

    bool purgeDeletedComponents(const QString &uid, const QDateTime &recurrenceId);

    sqlite3_stmt* loadOperationToSQL(const DBLoadOperation &dbop) const;

    /**
      Select incidences from Components table.

      @param stmt1 prepared sqlite statement for components table
      @param notebook notebook of incidence
      @return the queried incidence (the ownership is transfered to the caller).
    */
    KCalendarCore::Incidence* selectComponents(sqlite3_stmt *stmt1, QString &notebook);

    /**
      Select contacts and order them by appearances.

      @return ordered list of contacts.
    */
    KCalendarCore::Person::List selectContacts();

    bool selectMetadata(int *id);
    bool incrementTransactionId(int *id);

    // Helper Functions //

    /**
      Convert datetime to seconds relative to the origin.

      @param dt datetime
      @return seconds relative to origin
    */
    sqlite3_int64 toOriginTime(const QDateTime &dt) const;

    /**
      Convert local datetime to seconds relative to the origin.

      @param dt datetime
      @return seconds relative to origin
    */
    sqlite3_int64 toLocalOriginTime(const QDateTime &dt) const;

    /**
      Convert seconds from the origin to clock time.
      @param seconds relative to origin.
      @return clocktime datetime.
    */
    QDateTime fromLocalOriginTime(sqlite3_int64 seconds) const ;

    /**
      Convert seconds from the origin to UTC datetime.
      @param seconds relative to origin.
      @return UTC datetime.
    */
    QDateTime fromOriginTime(sqlite3_int64 seconds) const;

    /**
      Convert seconds from the origin to datetime in given timezone.
      @param seconds relative to origin.
      @param zonename timezone name.
      @return datetime in timezone.
    */
    QDateTime fromOriginTime(sqlite3_int64 seconds, const QByteArray &zonename) const;

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteFormat)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond
};

}

#define SL3_exec( db )                                        \
{                                                             \
 /* kDebug() << "SQL query:" << query;    */                  \
  rv = sqlite3_exec( (db), query, NULL, 0, &errmsg );         \
  if ( rv ) {                                                 \
    if ( rv != SQLITE_CONSTRAINT ) {                          \
      qCWarning(lcMkcal) << "sqlite3_exec error code:" << rv;           \
    }                                                         \
    if ( errmsg ) {                                           \
      if ( rv != SQLITE_CONSTRAINT ) {                        \
        qCWarning(lcMkcal) << errmsg;                                   \
      }                                                       \
      sqlite3_free( errmsg );                                 \
      errmsg = NULL;                                          \
    }                                                         \
    if ( rv != SQLITE_CONSTRAINT ) {                          \
      goto error;                                             \
    }                                                         \
  }                                                           \
}

#define SL3_prepare_v2( db, query, qsize, stmt, tail )                \
{                                                                     \
 /* kDebug() << "SQL query:" << query;     */                         \
  rv = sqlite3_prepare_v2( (db), (query), (qsize), (stmt), (tail) );  \
  if ( rv ) {                                                         \
    qCWarning(lcMkcal) << "sqlite3_prepare error code:" << rv;                  \
    qCWarning(lcMkcal) << sqlite3_errmsg( (db) );                               \
    goto error;                                                       \
  }                                                                   \
}

#define SL3_bind_text( stmt, index, value, size, desc )               \
{                                                                     \
  rv = sqlite3_bind_text( (stmt), (index), (value), (size), (desc) ); \
  if ( rv ) {                                                         \
    qCWarning(lcMkcal) << "sqlite3_bind_text error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define SL3_bind_blob( stmt, index, value, size, desc )               \
{                                                                     \
  rv = sqlite3_bind_blob( (stmt), (index), (value), (size), (desc) ); \
  if ( rv ) {                                                         \
    qCWarning(lcMkcal) << "sqlite3_bind_blob error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define SL3_bind_int( stmt, index, value )                            \
{                                                                     \
  rv = sqlite3_bind_int( (stmt), (index), (value) );                  \
  if ( rv ) {                                                         \
    qCWarning(lcMkcal) << "sqlite3_bind_int error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define SL3_bind_int64( stmt, index, value )                          \
{                                                                     \
  rv = sqlite3_bind_int64( (stmt), (index), (value) );                \
  if ( rv ) {                                                         \
    qCWarning(lcMkcal) << "sqlite3_bind_int64 error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define SL3_bind_double( stmt, index, value )                         \
{                                                                     \
  rv = sqlite3_bind_double( (stmt), (index), (value) );               \
  if ( rv ) {                                                         \
    qCWarning(lcMkcal) << "sqlite3_bind_int error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define SL3_step( stmt )                                \
{                                                       \
  rv = sqlite3_step( (stmt) );                          \
  if ( rv && rv != SQLITE_DONE && rv != SQLITE_ROW ) {  \
    if ( rv != SQLITE_CONSTRAINT ) {                    \
      qCWarning(lcMkcal) << "sqlite3_step error:" << rv;          \
    }                                                   \
    goto error;                                         \
  }                                                     \
}

#define SL3_reset( stmt )                               \
{                                                       \
  rv = sqlite3_reset( (stmt) );                         \
  if ( rv && rv != SQLITE_OK ) {                        \
    qCWarning(lcMkcal) << "sqlite3_reset error:" << rv; \
    goto error;                                         \
  }                                                     \
}

#define CREATE_METADATA \
  "CREATE TABLE IF NOT EXISTS Metadata(transactionId INTEGER)"
#define CREATE_TIMEZONES \
  "CREATE TABLE IF NOT EXISTS Timezones(TzId INTEGER PRIMARY KEY, ICalData TEXT)"
#define CREATE_CALENDARS \
  "CREATE TABLE IF NOT EXISTS Calendars(CalendarId TEXT PRIMARY KEY, Name TEXT, Description TEXT, Color INTEGER, Flags INTEGER, syncDate INTEGER, pluginName TEXT, account TEXT, attachmentSize INTEGER, modifiedDate INTEGER, sharedWith TEXT, syncProfile TEXT, createdDate INTEGER, extra1 STRING, extra2 STRING)"

//Extra fields added for future use in case they are needed. They will be documented here
//So we can add something without breaking the schema and not adding tables
//extra1: used to store the color of a single component.

#define CREATE_COMPONENTS \
  "CREATE TABLE IF NOT EXISTS Components(ComponentId INTEGER PRIMARY KEY AUTOINCREMENT, Notebook TEXT, Type TEXT, Summary TEXT, Category TEXT, DateStart INTEGER, DateStartLocal INTEGER, StartTimeZone TEXT, HasDueDate INTEGER, DateEndDue INTEGER, DateEndDueLocal INTEGER, EndDueTimeZone TEXT, Duration INTEGER, Classification INTEGER, Location TEXT, Description TEXT, Status INTEGER, GeoLatitude REAL, GeoLongitude REAL, Priority INTEGER, Resources TEXT, DateCreated INTEGER, DateStamp INTEGER, DateLastModified INTEGER, Sequence INTEGER, Comments TEXT, Attachments TEXT, Contact TEXT, InvitationStatus INTEGER, RecurId INTEGER, RecurIdLocal INTEGER, RecurIdTimeZone TEXT, RelatedTo TEXT, URL TEXT, UID TEXT, Transparency INTEGER, LocalOnly INTEGER, Percent INTEGER, DateCompleted INTEGER, DateCompletedLocal INTEGER, CompletedTimeZone TEXT, DateDeleted INTEGER, extra1 STRING, extra2 STRING, extra3 INTEGER)"

//Extra fields added for future use in case they are needed. They will be documented here
//So we can add something without breaking the schema and not adding tables

#define CREATE_RDATES \
  "CREATE TABLE IF NOT EXISTS Rdates(ComponentId INTEGER, Type INTEGER, Date INTEGER, DateLocal INTEGER, TimeZone TEXT)"
#define CREATE_CUSTOMPROPERTIES \
  "CREATE TABLE IF NOT EXISTS Customproperties(ComponentId INTEGER, Name TEXT, Value TEXT, Parameters TEXT)"
#define CREATE_RECURSIVE \
  "CREATE TABLE IF NOT EXISTS Recursive(ComponentId INTEGER, RuleType INTEGER, Frequency INTEGER, Until INTEGER, UntilLocal INTEGER, untilTimeZone TEXT, Count INTEGER, Interval INTEGER, BySecond TEXT, ByMinute TEXT, ByHour TEXT, ByDay TEXT, ByDayPos Text, ByMonthDay TEXT, ByYearDay TEXT, ByWeekNum TEXT, ByMonth TEXT, BySetPos TEXT, WeekStart INTEGER)"
#define CREATE_ALARM \
  "CREATE TABLE IF NOT EXISTS Alarm(ComponentId INTEGER, Action INTEGER, Repeat INTEGER, Duration INTEGER, Offset INTEGER, Relation TEXT, DateTrigger INTEGER, DateTriggerLocal INTEGER, triggerTimeZone TEXT, Description TEXT, Attachment TEXT, Summary TEXT, Address TEXT, CustomProperties TEXT, isEnabled INTEGER)"
#define CREATE_ATTENDEE \
"CREATE TABLE IF NOT EXISTS Attendee(ComponentId INTEGER, Email TEXT, Name TEXT, IsOrganizer INTEGER, Role INTEGER, PartStat INTEGER, Rsvp INTEGER, DelegatedTo TEXT, DelegatedFrom TEXT)"
#define CREATE_ATTACHMENTS \
"CREATE TABLE IF NOT EXISTS Attachments(ComponentId INTEGER, Data BLOB, Uri TEXT, MimeType TEXT, ShowInLine INTEGER, Label TEXT, Local INTEGER)"
#define CREATE_CALENDARPROPERTIES \
  "CREATE TABLE IF NOT EXISTS Calendarproperties(CalendarId REFERENCES Calendars(CalendarId) ON DELETE CASCADE, Name TEXT NOT NULL, Value TEXT, UNIQUE (CalendarId, Name))"

#define INDEX_CALENDAR \
"CREATE INDEX IF NOT EXISTS IDX_CALENDAR on Calendars(CalendarId)"
#define INDEX_COMPONENT \
"CREATE INDEX IF NOT EXISTS IDX_COMPONENT on Components(ComponentId, Notebook, DateStart, DateEndDue, DateDeleted)"
#define INDEX_COMPONENT_UID \
"CREATE UNIQUE INDEX IF NOT EXISTS IDX_COMPONENT_UID on Components(UID, RecurId, DateDeleted)"
#define INDEX_COMPONENT_NOTEBOOK \
"CREATE INDEX IF NOT EXISTS IDX_COMPONENT_NOTEBOOK on Components(Notebook)"
#define INDEX_RDATES \
"CREATE INDEX IF NOT EXISTS IDX_RDATES on Rdates(ComponentId)"
#define INDEX_CUSTOMPROPERTIES \
"CREATE INDEX IF NOT EXISTS IDX_CUSTOMPROPERTIES on Customproperties(ComponentId)"
#define INDEX_RECURSIVE \
"CREATE INDEX IF NOT EXISTS IDX_RECURSIVE on Recursive(ComponentId)"
#define INDEX_ALARM \
"CREATE INDEX IF NOT EXISTS IDX_ALARM on Alarm(ComponentId)"
#define INDEX_ATTENDEE \
"CREATE UNIQUE INDEX IF NOT EXISTS IDX_ATTENDEE on Attendee(ComponentId, Email)"
#define INDEX_ATTACHMENTS \
"CREATE INDEX IF NOT EXISTS IDX_ATTACHMENTS on Attachments(ComponentId)"
#define INDEX_CALENDARPROPERTIES \
"CREATE INDEX IF NOT EXISTS IDX_CALENDARPROPERTIES on Calendarproperties(CalendarId)"

#define INSERT_TIMEZONES \
"insert into Timezones values (1, '')"
#define INSERT_CALENDARS \
"insert into Calendars values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '', '')"
#define INSERT_COMPONENTS \
"insert into Components values (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, '', 0)"
#define INSERT_CUSTOMPROPERTIES \
"insert into Customproperties values (?, ?, ?, ?)"
#define INSERT_CALENDARPROPERTIES \
"insert into Calendarproperties values (?, ?, ?)"
#define INSERT_RDATES \
"insert into Rdates values (?, ?, ?, ?, ?)"
#define INSERT_RECURSIVE \
"insert into Recursive values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define INSERT_ALARM \
"insert into Alarm values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define INSERT_ATTENDEE \
"insert into Attendee values (?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define INSERT_ATTACHMENTS \
"insert into Attachments values (?, ?, ?, ?, ?, ?, ?)"

#define UPDATE_METADATA \
"replace into Metadata (rowid, transactionId) values (1, ?)"
#define UPDATE_TIMEZONES \
"update Timezones set ICalData=? where TzId=1"
#define UPDATE_CALENDARS \
"update Calendars set Name=?, Description=?, Color=?, Flags=?, syncDate=?, pluginName=?, account=?, attachmentSize=?, modifiedDate=?, sharedWith=?, syncProfile=?, createdDate=? where CalendarId=?"
#define UPDATE_COMPONENTS \
"update Components set Notebook=?, Type=?, Summary=?, Category=?, DateStart=?, DateStartLocal=?, StartTimeZone=?, HasDueDate=?, DateEndDue=?, DateEndDueLocal=?, EndDueTimeZone=?, Duration=?, Classification=?, Location=?, Description=?, Status=?, GeoLatitude=?, GeoLongitude=?, Priority=?, Resources=?, DateCreated=?, DateStamp=?, DateLastModified=?, Sequence=?, Comments=?, Attachments=?, Contact=?, RecurId=?, RecurIdLocal=?, RecurIdTimeZone=?, RelatedTo=?, URL=?, UID=?, Transparency=?, LocalOnly=?, Percent=?, DateCompleted=?, DateCompletedLocal=?, CompletedTimeZone=?, extra1=? where ComponentId=?"
#define UPDATE_COMPONENTS_AS_DELETED \
"update Components set DateDeleted=? where ComponentId=?"
//"update Components set DateDeleted=strftime('%s','now') where ComponentId=?"

#define DELETE_CALENDARS \
"delete from Calendars where CalendarId=?"
#define DELETE_COMPONENTS \
"delete from Components where ComponentId=?"
#define DELETE_RDATES \
"delete from Rdates where ComponentId=?"
#define DELETE_CUSTOMPROPERTIES \
"delete from Customproperties where ComponentId=?"
#define DELETE_CALENDARPROPERTIES \
"delete from Calendarproperties where CalendarId=?"
#define DELETE_RECURSIVE \
"delete from Recursive where ComponentId=?"
#define DELETE_ALARM \
"delete from Alarm where ComponentId=?"
#define DELETE_ATTENDEE \
"delete from Attendee where ComponentId=?"
#define DELETE_ATTACHMENTS \
"delete from Attachments where ComponentId=?"

#define SELECT_METADATA \
"select * from Metadata where rowid=1"
#define SELECT_TIMEZONES \
"select * from Timezones where TzId=1"
#define SELECT_CALENDARS_ALL \
"select * from Calendars order by Name"
#define SELECT_CALENDARS_BY_UID \
"select * from Calendars where calendarId=?"
#define SELECT_COMPONENTS_ALL \
"select * from Components where DateDeleted=0"
#define SELECT_COMPONENTS_BY_NOTEBOOK \
"select * from Components where Notebook=? and DateDeleted=0"
#define SELECT_COMPONENTS_ALL_DELETED \
"select * from Components where DateDeleted<>0"
#define SELECT_COMPONENTS_ALL_DELETED_BY_NOTEBOOK \
"select * from Components where Notebook=? and DateDeleted<>0"
#define SELECT_COMPONENTS_BY_GEO \
"select * from Components where GeoLatitude!=255.0 and GeoLongitude!=255.0 and DateDeleted=0"
#define SELECT_COMPONENTS_BY_GEO_AREA \
"select * from Components where GeoLatitude>=? and GeoLongitude>=? and GeoLatitude<=? and GeoLongitude<=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_JOURNAL \
"select * from Components where Type='Journal' and DateDeleted=0"
#define SELECT_COMPONENTS_BY_JOURNAL_DATE \
"select * from Components where Type='Journal' and DateDeleted=0 and datestart<=? order by DateStart desc, DateCreated desc"
#define SELECT_COMPONENTS_BY_PLAIN \
"select * from Components where DateStart=0 and DateEndDue=0 and DateDeleted=0"
#define SELECT_COMPONENTS_BY_RECURSIVE \
"select * from Components where ((ComponentId in (select DISTINCT ComponentId from Recursive)) or (ComponentId in (select DISTINCT ComponentId from Rdates)) or (RecurId!=0)) and DateDeleted=0"
#define SELECT_COMPONENTS_BY_ATTENDEE \
"select * from Components where ComponentId in (select DISTINCT ComponentId from Attendee) and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DATE_BOTH \
"select * from Components where DateStart<? and (DateEndDue>=? or (DateEndDue=0 and DateStart>=?)) and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DATE_START \
"select * from Components where (DateEndDue>=? or (DateEndDue=0 and DateStart>=?)) and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DATE_END \
"select * from Components where DateStart<? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_UID_AND_RECURID \
"select * from Components where UID=? and RecurId=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_UID \
"select * from Components where UID=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_NOTEBOOKUID \
"select * from Components where Notebook=? and DateDeleted=0"
#define SELECT_ROWID_FROM_COMPONENTS_BY_UID_AND_RECURID \
"select ComponentId from Components where UID=? and RecurId=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_UNCOMPLETED_TODOS \
"select * from Components where Type='Todo' and DateCompleted=0 and DateDeleted=0"
#define SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_DATE \
"select * from Components where Type='Todo' and DateCompleted<>0 and DateEndDue<>0 and DateEndDue<=? and DateDeleted=0 order by DateEndDue desc, DateCreated desc"
#define SELECT_COMPONENTS_BY_COMPLETED_TODOS_AND_CREATED \
"select * from Components where Type='Todo' and DateCompleted<>0 and DateEndDue=0 and DateCreated<=? and DateDeleted=0 order by DateCreated desc"
#define SELECT_COMPONENTS_BY_DATE_SMART \
"select * from Components where DateEndDue<>0 and DateEndDue<=? and DateDeleted=0 order by DateEndDue desc, DateCreated desc"

#define FUTURE_DATE_SMART_FIELD                                 \
" (case type when 'Todo' then DateEndDue else DateStart end) "
#define SELECT_COMPONENTS_BY_FUTURE_DATE_SMART                  \
    "select * from Components where "                           \
    FUTURE_DATE_SMART_FIELD ">=? and DateDeleted=0 order by "   \
    FUTURE_DATE_SMART_FIELD " asc, DateCreated asc"

#define SELECT_COMPONENTS_BY_CREATED_SMART                              \
"select * from Components where DateEndDue=0 and DateCreated<=? and DateDeleted=0 order by DateCreated desc"
#define SELECT_COMPONENTS_BY_GEO_AND_DATE \
"select * from Components where GeoLatitude!=255.0 and GeoLongitude!=255.0 and DateEndDue<>0 and DateEndDue<=? and DateDeleted=0 order by DateEndDue desc, DateCreated desc"
#define SELECT_COMPONENTS_BY_GEO_AND_CREATED \
"select * from Components where GeoLatitude!=255.0 and GeoLongitude!=255.0 and DateEndDue=0 and DateCreated<=? and DateDeleted=0 order by DateCreated desc"
#define SELECT_COMPONENTS_BY_ATTENDEE_EMAIL_AND_CREATED \
"select * from Components where ComponentId in (select distinct ComponentId from Attendee where email=?) and DateCreated<=? and DateDeleted=0 order by DateCreated desc"
#define SELECT_COMPONENTS_BY_ATTENDEE_AND_CREATED \
"select * from Components where ComponentId in (select distinct ComponentId from Attendee) and DateCreated<=? and DateDeleted=0 order by DateCreated desc"
#define SELECT_RDATES_BY_ID \
"select * from Rdates where ComponentId=?"
#define SELECT_CUSTOMPROPERTIES_BY_ID \
"select * from Customproperties where ComponentId=?"
#define SELECT_RECURSIVE_BY_ID \
"select * from Recursive where ComponentId=?"
#define SELECT_ALARM_BY_ID \
"select * from Alarm where ComponentId=?"
#define SELECT_ATTENDEE_BY_ID \
"select * from Attendee where ComponentId=?"
#define SELECT_ATTACHMENTS_BY_ID \
"select * from Attachments where ComponentId=?"
#define SELECT_CALENDARPROPERTIES_BY_ID \
"select * from Calendarproperties where CalendarId=?"
#define SELECT_COMPONENTS_BY_DUPLICATE \
"select * from Components where DateStart=? and Summary=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DUPLICATE_AND_NOTEBOOK \
"select * from Components where DateStart=? and Summary=? and Notebook=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_CREATED \
"select * from Components where DateCreated>=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_CREATED_AND_NOTEBOOK \
"select * from Components where DateCreated>=? and Notebook=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_LAST_MODIFIED \
"select * from Components where DateLastModified>=? and DateCreated<? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_LAST_MODIFIED_AND_NOTEBOOK \
"select * from Components where DateLastModified>=? and DateCreated<? and Notebook=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DELETED \
"select * from Components where DateDeleted>=? and DateCreated<?"
#define SELECT_COMPONENTS_BY_DELETED_AND_NOTEBOOK \
"select * from Components where DateDeleted>=? and DateCreated<? and Notebook=?"
#define SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED \
"select ComponentId, DateDeleted from Components where UID=? and RecurId=? and DateDeleted<>0"
#define SELECT_ATTENDEE_AND_COUNT \
"select Email, Name, count(Email) from Attendee where Email<>0 group by Email"
#define SELECT_EVENT_COUNT \
"select count(*) from Components where Type='Event' and DateDeleted=0"
#define SELECT_TODO_COUNT \
"select count(*) from Components where Type='Todo' and DateDeleted=0"
#define SELECT_JOURNAL_COUNT \
"select count(*) from Components where Type='Journal' and DateDeleted=0"

#define UNSET_FLAG_FROM_CALENDAR \
"update Calendars set Flags=(Flags & (~?))"

#define BEGIN_TRANSACTION \
"BEGIN IMMEDIATE;"
#define COMMIT_TRANSACTION \
"END;"

#endif
