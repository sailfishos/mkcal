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
  defines the SqliteStorage class.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
  @author Alvaro Manera \<alvaro.manera@nokia.com \>
*/

#ifndef MKCAL_SQLITESTORAGE_H
#define MKCAL_SQLITESTORAGE_H

#include "mkcal_export.h"
#include "extendedstorage.h"

#include <sqlite3.h>

namespace mKCal {

const int VersionMajor = 11; // Major version, if different than stored in database, open fails
const int VersionMinor = 0; // Minor version, if different than stored in database, open warning

/**
  @brief
  This class provides a calendar storage as an sqlite database.

  @warning When saving Attendees, the CustomProperties are not saved.
*/
class MKCAL_EXPORT SqliteStorage : public ExtendedStorage
{
    Q_OBJECT

public:

    /**
      A shared pointer to a SqliteStorage
    */
    typedef QSharedPointer<SqliteStorage> Ptr;

    /**
      Constructs a new SqliteStorage object for Calendar @p calendar with format
      @p format, and storage to file @p fileName.

      @param calendar is a pointer to a valid Calendar object.
      @param databaseName is the name of the database containing the Calendar data.
      @param validateNotebooks set to true for saving only those incidences
             that belong to an existing notebook of this storage
    */
    explicit SqliteStorage(const ExtendedCalendar::Ptr &cal,
                           const QString &databaseName,
                           bool validateNotebooks = false);

    /**
      Destructor.
    */
    virtual ~SqliteStorage();

    /**
      Returns a string containing the name of the calendar database.
    */
    QString databaseName() const;

    /**
      @copydoc
      CalStorage::open()
    */
    bool open();

    /**
      @copydoc
      CalStorage::load()
    */
    bool load();

    /**
      @copydoc
      ExtendedStorage::load(const QString &, const KDateTime &)
    */
    bool load(const QString &uid, const KDateTime &recurrenceId = KDateTime());

    /**
      @copydoc
      ExtendedStorage::load(const QDate &)
    */
    bool load(const QDate &date);

    /**
      @copydoc
      ExtendedStorage::load(const QDate &, const QDate &)
    */
    bool load(const QDate &start, const QDate &end);

    /**
      @copydoc
      ExtendedStorage::loadNotebookIncidences(const QString &)
    */
    bool loadNotebookIncidences(const QString &notebookUid);

    /**
      @copydoc
      ExtendedStorage::loadJournals()
    */
    bool loadJournals();

    /**
      @copydoc
      ExtendedStorage::loadPlainIncidences()
    */
    bool loadPlainIncidences();

    /**
      @copydoc
      ExtendedStorage::loadRecurringIncidences()
    */
    bool loadRecurringIncidences();

    /**
      @copydoc
      ExtendedStorage::loadGeoIncidences()
    */
    bool loadGeoIncidences();

    /**
      @copydoc
      ExtendedStorage::loadGeoIncidences(float, float, float, float)
    */
    bool loadGeoIncidences(float geoLatitude, float geoLongitude,
                           float diffLatitude, float diffLongitude);

    /**
      @copydoc
      ExtendedStorage::loadAttendeeIncidences()
    */
    bool loadAttendeeIncidences();

    /**
      @copydoc
      ExtendedStorage::loadUncompletedTodos()
    */
    int loadUncompletedTodos();

    /**
      @copydoc
      ExtendedStorage::loadCompletedTodos()
    */
    int loadCompletedTodos(bool hasDate, int limit, KDateTime *last);

    /**
      @copydoc
      ExtendedStorage::loadIncidences( bool, bool, int, KDateTime* );
    */
    int loadIncidences(bool hasDate, int limit, KDateTime *last);

    /**
      @copydoc
      ExtendedStorage::loadFutureIncidences( bool, int, KDateTime* );
    */
    int loadFutureIncidences(int limit, KDateTime *last);

    /**
      @copydoc
      ExtendedStorage::loadGeoIncidences( bool, bool, int, KDateTime* );
    */
    int loadGeoIncidences(bool hasDate, int limit, KDateTime *last);

    /**
      @copydoc
      ExtendedStorage::loadUnreadInvitationIncidences()
    */
    int loadUnreadInvitationIncidences();

    /**
      @copydoc
      ExtendedStorage::loadInvitationIncidences()
    */
    int loadOldInvitationIncidences(int limit, KDateTime *last);

    /**
      @copydoc
      ExtendedStorage::loadContacts()
    */
    KCalCore::Person::List loadContacts();

    /**
      @copydoc
      ExtendedStorage::loadContactIncidences( const KCalCore::Person::Ptr & )
    */
    int loadContactIncidences(const KCalCore::Person::Ptr &person, int limit, KDateTime *last);

    /**
      @copydoc
      ExtendedStorage::loadJournals()
    */
    int loadJournals(int limit, KDateTime *last);

    /**
      @copydoc
      ExtendedStorage::notifyOpened( const KCalCore::Incidence::Ptr & )
    */
    bool notifyOpened(const KCalCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      CalStorage::save()
    */
    bool save();

    /**
      @copydoc
      ExtendedStorage::cancel()
    */
    bool cancel();

    /**
      @copydoc
      CalStorage::close()
    */
    bool close();

    /**
      @copydoc
      Calendar::CalendarObserver::calendarModified()
    */
    void calendarModified(bool modified, KCalCore::Calendar *calendar);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceCreated()
    */
    void calendarIncidenceCreated(const KCalCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdded()
    */
    void calendarIncidenceAdded(const KCalCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceChanged()
    */
    void calendarIncidenceChanged(const KCalCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceDeleted()
    */
    void calendarIncidenceDeleted(const KCalCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdditionCanceled()
    */
    void calendarIncidenceAdditionCanceled(const KCalCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      ExtendedStorage::insertedIncidences()
    */
    bool insertedIncidences(KCalCore::Incidence::List *list, const KDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::modifiedIncidences()
    */
    bool modifiedIncidences(KCalCore::Incidence::List *list, const KDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::deletedIncidences()
    */
    bool deletedIncidences(KCalCore::Incidence::List *list, const KDateTime &after,
                           const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::allIncidences()
    */
    bool allIncidences(KCalCore::Incidence::List *list, const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::duplicateIncidences()
    */
    bool duplicateIncidences(KCalCore::Incidence::List *list,
                             const KCalCore::Incidence::Ptr &incidence,
                             const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::incidenceDeletedDate()
    */
    KDateTime incidenceDeletedDate(const KCalCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      ExtendedStorage::eventCount()
    */
    int eventCount();

    /**
      @copydoc
      ExtendedStorage::todoCount()
    */
    int todoCount();

    /**
      @copydoc
      ExtendedStorage::journalCount()
    */
    int journalCount();

    /**
      @copydoc
      ExtendedStorage::virtual_hook()
    */
    virtual void virtual_hook(int id, void *data);


    // Helper Functions //

    /**
      Convert datetime to seconds relative to the origin.

      @param dt datetime
      @return seconds relative to origin
    */
    sqlite3_int64 toOriginTime(KDateTime dt);

    /**
      Convert local datetime to seconds relative to the origin.

      @param dt datetime
      @return seconds relative to origin
    */
    sqlite3_int64 toLocalOriginTime(KDateTime dt);

    /**
      Convert seconds from the origin to UTC datetime.
      @param seconds relative to origin.
      @return UTC datetime.
    */
    KDateTime fromOriginTime(sqlite3_int64 seconds);

    /**
      Convert seconds from the origin to datetime in given timezone.
      @param seconds relative to origin.
      @param zonename timezone name.
      @return datetime in timezone.
    */
    KDateTime fromOriginTime(sqlite3_int64 seconds, QString zonename);

private:
    /**
      Initialized the database with the predefined contents

      @return True if ok, false if there was an error
    */
    bool initializeDatabase();

protected:
    bool loadNotebooks();
    bool reloadNotebooks();
    bool modifyNotebook(const Notebook::Ptr &nb, DBOperation dbop, bool signal = true);

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

public Q_SLOTS:
    void fileChanged(const QString &path);

    void queryFinished();

};

#define sqlite3_exec( db )                                    \
{                                                             \
 /* kDebug() << "SQL query:" << query;    */                  \
  rv = sqlite3_exec( (db), query, NULL, 0, &errmsg );         \
  if ( rv ) {                                                 \
    if ( rv != SQLITE_CONSTRAINT ) {                          \
      kError() << "sqlite3_exec error code:" << rv;           \
    }                                                         \
    if ( errmsg ) {                                           \
      if ( rv != SQLITE_CONSTRAINT ) {                        \
        kError() << errmsg;                                   \
      }                                                       \
      sqlite3_free( errmsg );                                 \
      errmsg = NULL;                                          \
    }                                                         \
    if ( rv != SQLITE_CONSTRAINT ) {                          \
      goto error;                                             \
    }                                                         \
  }                                                           \
}

#define sqlite3_prepare_v2( db, query, qsize, stmt, tail )            \
{                                                                     \
 /* kDebug() << "SQL query:" << query;     */                         \
  rv = sqlite3_prepare_v2( (db), (query), (qsize), (stmt), (tail) );  \
  if ( rv ) {                                                         \
    kError() << "sqlite3_prepare error code:" << rv;                  \
    kError() << sqlite3_errmsg( (db) );                               \
    goto error;                                                       \
  }                                                                   \
}

#define sqlite3_bind_text( stmt, index, value, size, desc )           \
{                                                                     \
  rv = sqlite3_bind_text( (stmt), (index), (value), (size), (desc) ); \
  if ( rv ) {                                                         \
    kError() << "sqlite3_bind_text error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define sqlite3_bind_int( stmt, index, value )                        \
{                                                                     \
  rv = sqlite3_bind_int( (stmt), (index), (value) );                  \
  if ( rv ) {                                                         \
    kError() << "sqlite3_bind_int error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define sqlite3_bind_int64( stmt, index, value )                      \
{                                                                     \
  rv = sqlite3_bind_int64( (stmt), (index), (value) );                \
  if ( rv ) {                                                         \
    kError() << "sqlite3_bind_int64 error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define sqlite3_bind_double( stmt, index, value )                     \
{                                                                     \
  rv = sqlite3_bind_double( (stmt), (index), (value) );               \
  if ( rv ) {                                                         \
    kError() << "sqlite3_bind_int error:" << rv << "on index and value:" << index << value; \
    goto error;                                                       \
  }                                                                   \
  index++;                                                            \
}

#define sqlite3_step( stmt )                            \
{                                                       \
  rv = sqlite3_step( (stmt) );                          \
  if ( rv && rv != SQLITE_DONE && rv != SQLITE_ROW ) {  \
    if ( rv != SQLITE_CONSTRAINT ) {                    \
      kError() << "sqlite3_step error:" << rv;          \
    }                                                   \
    goto error;                                         \
  }                                                     \
}

#define CREATE_VERSION \
  "CREATE TABLE IF NOT EXISTS Version(Major INTEGER, Minor INTEGER)"
#define CREATE_TIMEZONES \
  "CREATE TABLE IF NOT EXISTS Timezones(TzId INTEGER PRIMARY KEY, ICalData TEXT)"
#define CREATE_CALENDARS \
  "CREATE TABLE IF NOT EXISTS Calendars(CalendarId TEXT PRIMARY KEY, Name TEXT, Description TEXT, Color INTEGER, Flags INTEGER, syncDate INTEGER, pluginName TEXT, account TEXT, attachmentSize INTEGER, modifiedDate INTEGER, sharedWith TEXT, syncProfile TEXT, createdDate INTEGER, extra1 STRING, extra2 STRING)"

//Extra fields added for future use in case they are needed. They will be documented here
//So we can add somthing without breaking the schema and not adding tables

#define CREATE_COMPONENTS \
  "CREATE TABLE IF NOT EXISTS Components(ComponentId INTEGER PRIMARY KEY AUTOINCREMENT, Notebook TEXT, Type TEXT, Summary TEXT, Category TEXT, DateStart INTEGER, DateStartLocal INTEGER, StartTimeZone TEXT, HasDueDate INTEGER, DateEndDue INTEGER, DateEndDueLocal INTEGER, EndDueTimeZone TEXT, Duration INTEGER, Classification INTEGER, Location TEXT, Description TEXT, Status INTEGER, GeoLatitude REAL, GeoLongitude REAL, Priority INTEGER, Resources TEXT, DateCreated INTEGER, DateStamp INTEGER, DateLastModified INTEGER, Sequence INTEGER, Comments TEXT, Attachments TEXT, Contact TEXT, InvitationStatus INTEGER, RecurId INTEGER, RecurIdLocal INTEGER, RecurIdTimeZone TEXT, RelatedTo TEXT, URL TEXT, UID TEXT, Transparency INTEGER, LocalOnly INTEGER, Percent INTEGER, DateCompleted INTEGER, DateCompletedLocal INTEGER, CompletedTimeZone TEXT, DateDeleted INTEGER, extra1 STRING, extra2 STRING, extra3 INTEGER)"

//Extra fields added for future use in case they are needed. They will be documented here
//So we can add somthing without breaking the schema and not adding tables

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

#define INDEX_CALENDAR \
"CREATE INDEX IF NOT EXISTS IDX_CALENDAR on Calendars(CalendarId)"
#define INDEX_INVITATION \
"CREATE INDEX IF NOT EXISTS IDX_INVITATION on Invitations(InvitationId)"
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

#define INSERT_VERSION \
"insert into Version values (?, ?)"
#define INSERT_TIMEZONES \
"insert into Timezones values (1, '')"
#define INSERT_CALENDARS \
"insert into Calendars values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '', '')"
#define INSERT_INVITATIONS \
"insert into Invitations values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define INSERT_COMPONENTS \
"insert into Components values (NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, '', '', 0)"
#define INSERT_CUSTOMPROPERTIES \
"insert into Customproperties values (?, ?, ?, ?)"
#define INSERT_RDATES \
"insert into Rdates values (?, ?, ?, ?, ?)"
#define INSERT_RECURSIVE \
"insert into Recursive values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define INSERT_ALARM \
"insert into Alarm values (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
#define INSERT_ATTENDEE \
"insert into Attendee values (?, ?, ?, ?, ?, ?, ?, ?, ?)"

#define UPDATE_TIMEZONES \
"update Timezones set ICalData=? where TzId=1"
#define UPDATE_CALENDARS \
"update Calendars set Name=?, Description=?, Color=?, Flags=?, syncDate=?, pluginName=?, account=?, attachmentSize=?, modifiedDate=?, sharedWith=?, syncProfile=?, createdDate=? where CalendarId=?"
#define UPDATE_COMPONENTS \
"update Components set Notebook=?, Type=?, Summary=?, Category=?, DateStart=?, DateStartLocal=?, StartTimeZone=?, HasDueDate=?, DateEndDue=?, DateEndDueLocal=?, EndDueTimeZone=?, Duration=?, Classification=?, Location=?, Description=?, Status=?, GeoLatitude=?, GeoLongitude=?, Priority=?, Resources=?, DateCreated=?, DateStamp=?, DateLastModified=?, Sequence=?, Comments=?, Attachments=?, Contact=?, InvitationStatus=?, RecurId=?, RecurIdLocal=?, RecurIdTimeZone=?, RelatedTo=?, URL=?, UID=?, Transparency=?, LocalOnly=?, DateCompleted=?, DateCompletedLocal=?, CompletedTimeZone=?, Percent=? where ComponentId=?"

#define DELETE_TIMEZONES \
"delete from Timezones where TzId=1"
#define DELETE_CALENDARS \
"delete from Calendars where CalendarId=?"
#define DELETE_INVITATIONS \
"delete from Invitations where InvitationId=?"
#define DELETE_COMPONENTS \
"update Components set DateDeleted=? where ComponentId=?"
//"update Components set DateDeleted=strftime('%s','now') where ComponentId=?"
#define DELETE_RDATES \
"delete from Rdates where ComponentId=?"
#define DELETE_CUSTOMPROPERTIES \
"delete from Customproperties where ComponentId=?"
#define DELETE_RECURSIVE \
"delete from Recursive where ComponentId=?"
#define DELETE_ALARM \
"delete from Alarm where ComponentId=?"
#define DELETE_ATTENDEE \
"delete from Attendee where ComponentId=?"

#define SELECT_VERSION \
"select * from Version"
#define SELECT_TIMEZONES \
"select * from Timezones where TzId=1"
#define SELECT_CALENDARS_ALL \
"select * from Calendars order by Name"
#define SELECT_INVITATIONS_ALL \
"select * from Invitations"
#define SELECT_COMPONENTS_ALL \
"select * from Components where DateDeleted=0"
#define SELECT_COMPONENTS_BY_NOTEBOOK \
"select * from Components where Notebook=? and DateDeleted=0"
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
"select * from components where ((ComponentId in (select DISTINCT ComponentId from recursive)) or (RecurId!=0)) and DateDeleted=0"
#define SELECT_COMPONENTS_BY_ATTENDEE \
"select * from components where ComponentId in (select DISTINCT ComponentId from attendee) and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DATE_BOTH \
"select * from Components where DateStart<=? and DateEndDue>=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DATE_START \
"select * from Components where DateEndDue>=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_DATE_END \
"select * from Components where DateStart<=? and DateDeleted=0"
#define SELECT_COMPONENTS_BY_UID_AND_RECURID \
"select * from Components where UID=? and RecurId=? and DateDeleted=0"
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
#define SELECT_COMPONENTS_BY_INVITATION_UNREAD \
"select * from Components where InvitationStatus=1 and DateDeleted=0"
#define SELECT_COMPONENTS_BY_INVITATION_AND_CREATED \
"select * from Components where InvitationStatus>1 and DateCreated<=? and DateDeleted=0 order by DateCreated desc"
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
#define SELECT_COMPONENTS_BY_UID_AND_DELETED \
"select DateDeleted from Components where UID=? and DateDeleted<>0"
#define SELECT_ATTENDEE_AND_COUNT \
"select Email, Name, count(Email) from Attendee where Email<>0 group by Email"
#define SELECT_EVENT_COUNT \
"select count(*) from Components where Type='Event' and DateDeleted=0"
#define SELECT_TODO_COUNT \
"select count(*) from Components where Type='Todo' and DateDeleted=0"
#define SELECT_JOURNAL_COUNT \
"select count(*) from Components where Type='Journal' and DateDeleted=0"

#define BEGIN_TRANSACTION \
"BEGIN IMMEDIATE;"
#define COMMIT_TRANSACTION \
"END;"

#define FLOATING_DATE "FloatingDate"
}

#endif
