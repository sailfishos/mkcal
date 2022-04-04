/*
  This file is part of the mkcal library.

  Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies). All rights reserved.
  Copyright (c) 2014-2019 Jolla Ltd.
  Copyright (c) 2019 Open Mobile Platform LLC.

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

  @brief
  Sqlite format implementation.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
*/
#include "sqliteformat.h"
#include "logging_p.h"

#include <KCalendarCore/Alarm>
#include <KCalendarCore/Attendee>
#include <KCalendarCore/Person>
#include <KCalendarCore/Sorting>

using namespace KCalendarCore;

#define FLOATING_DATE "FloatingDate"

using namespace mKCal;
class mKCal::SqliteFormat::Private
{
public:
    Private(SqliteFormat *format, sqlite3 *database, const QTimeZone &timeZone)
        : mFormat(format), mDatabase(database)
        , mTimeZone(timeZone)
    {
    }
    ~Private()
    {
        sqlite3_finalize(mSelectMetadata);
        sqlite3_finalize(mUpdateMetadata);
        sqlite3_finalize(mInsertCalendar);
        sqlite3_finalize(mUpdateCalendar);
        sqlite3_finalize(mDeleteCalendar);
        sqlite3_finalize(mSelectCalProps);
        sqlite3_finalize(mInsertCalProps);
        sqlite3_finalize(mSelectIncProperties);
        sqlite3_finalize(mSelectIncAttendees);
        sqlite3_finalize(mSelectIncAlarms);
        sqlite3_finalize(mSelectIncRecursives);
        sqlite3_finalize(mSelectIncRDates);
        sqlite3_finalize(mSelectIncAttachments);
        sqlite3_finalize(mSelectDeletedIncidences);
        sqlite3_finalize(mDeleteIncComponents);
        sqlite3_finalize(mDeleteIncProperties);
        sqlite3_finalize(mDeleteIncAttendees);
        sqlite3_finalize(mDeleteIncAlarms);
        sqlite3_finalize(mDeleteIncRecursives);
        sqlite3_finalize(mDeleteIncRDates);
        sqlite3_finalize(mDeleteIncAttachments);
        sqlite3_finalize(mInsertIncComponents);
        sqlite3_finalize(mInsertIncProperties);
        sqlite3_finalize(mInsertIncAttendees);
        sqlite3_finalize(mInsertIncAlarms);
        sqlite3_finalize(mInsertIncRecursives);
        sqlite3_finalize(mInsertIncRDates);
        sqlite3_finalize(mInsertIncAttachments);
        sqlite3_finalize(mUpdateIncComponents);
        sqlite3_finalize(mMarkDeletedIncidences);
    }
    SqliteFormat *mFormat;
    sqlite3 *mDatabase;
    QTimeZone mTimeZone;

    // Cache for various queries.
    sqlite3_stmt *mSelectMetadata = nullptr;
    sqlite3_stmt *mUpdateMetadata = nullptr;

    sqlite3_stmt *mInsertCalendar = nullptr;
    sqlite3_stmt *mUpdateCalendar = nullptr;
    sqlite3_stmt *mDeleteCalendar = nullptr;
    sqlite3_stmt *mSelectCalProps = nullptr;
    sqlite3_stmt *mInsertCalProps = nullptr;

    sqlite3_stmt *mSelectIncProperties = nullptr;
    sqlite3_stmt *mSelectIncAttendees = nullptr;
    sqlite3_stmt *mSelectIncAlarms = nullptr;
    sqlite3_stmt *mSelectIncRecursives = nullptr;
    sqlite3_stmt *mSelectIncRDates = nullptr;
    sqlite3_stmt *mSelectIncAttachments = nullptr;

    sqlite3_stmt *mSelectDeletedIncidences = nullptr;

    sqlite3_stmt *mDeleteIncComponents = nullptr;
    sqlite3_stmt *mDeleteIncProperties = nullptr;
    sqlite3_stmt *mDeleteIncAttendees = nullptr;
    sqlite3_stmt *mDeleteIncAlarms = nullptr;
    sqlite3_stmt *mDeleteIncRecursives = nullptr;
    sqlite3_stmt *mDeleteIncRDates = nullptr;
    sqlite3_stmt *mDeleteIncAttachments = nullptr;

    sqlite3_stmt *mInsertIncComponents = nullptr;
    sqlite3_stmt *mInsertIncProperties = nullptr;
    sqlite3_stmt *mInsertIncAttendees = nullptr;
    sqlite3_stmt *mInsertIncAlarms = nullptr;
    sqlite3_stmt *mInsertIncRecursives = nullptr;
    sqlite3_stmt *mInsertIncRDates = nullptr;
    sqlite3_stmt *mInsertIncAttachments = nullptr;

    sqlite3_stmt *mUpdateIncComponents = nullptr;

    sqlite3_stmt *mMarkDeletedIncidences = nullptr;

    bool updateMetadata(int transactionId);
    bool selectCustomproperties(Incidence *incidence, int rowid);
    int selectRowId(const Incidence &incidence);
    bool selectRecursives(Incidence *incidence, int rowid);
    bool selectAlarms(Incidence *incidence, int rowid);
    bool selectAttendees(Incidence *incidence, int rowid);
    bool selectRdates(Incidence *incidence, int rowid);
    bool selectAttachments(Incidence *incidence, int rowid);
    bool selectCalendarProperties(Notebook *notebook);
    bool insertCustomproperties(const Incidence &incidence, int rowid);
    bool insertCustomproperty(int rowid, const QByteArray &key, const QString &value, const QString &parameters);
    bool insertAttendees(const Incidence &incidence, int rowid);
    bool insertAttendee(int rowid, const Attendee &attendee, bool isOrganizer);
    bool insertAttachments(const Incidence &incidence, int rowid);
    bool insertAlarms(const Incidence &incidence, int rowid);
    bool insertAlarm(int rowid, Alarm::Ptr alarm);
    bool insertRecursives(const Incidence &incidence, int rowid);
    bool insertRecursive(int rowid, RecurrenceRule *rule, int type);
    bool insertRdates(const Incidence &incidence, int rowid);
    bool insertRdate(int rowid, int type, const QDateTime &rdate, bool allDay);
    bool deleteListsForIncidence(int rowid);
    bool modifyCalendarProperties(const Notebook &notebook, DBOperation dbop);
    bool deleteCalendarProperties(const QByteArray &id);
    bool insertCalendarProperty(const QByteArray &id, const QByteArray &key,
                                const QByteArray &value);
};
//@endcond

SqliteFormat::SqliteFormat(sqlite3 *database, const QTimeZone &timeZone)
    : d(new Private(this, database, timeZone))
{
}

SqliteFormat::~SqliteFormat()
{
    delete d;
}

bool SqliteFormat::selectMetadata(int *id)
{
    int rv = 0;

    if (!id)
        return false;
    if (!d->mSelectMetadata) {
        const char *query = SELECT_METADATA;
        int qsize = sizeof(SELECT_METADATA);
        SL3_prepare_v2(d->mDatabase, query, qsize, &d->mSelectMetadata, NULL);
    }
    SL3_step(d->mSelectMetadata);
    *id = (rv == SQLITE_ROW) ? sqlite3_column_int(d->mSelectMetadata, 0) : -1;
    SL3_reset(d->mSelectMetadata);

    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(d->mDatabase);
    return false;
}

bool SqliteFormat::incrementTransactionId(int *id)
{
    int savedId;

    if (id)
        *id = -1;
    if (!selectMetadata(&savedId))
        return false;
    savedId += 1;

    if (!d->updateMetadata(savedId))
        return false;
    if (id)
        *id = savedId;
    return true;
}

bool SqliteFormat::Private::updateMetadata(int transactionId)
{
    int rv = 0;
    int index = 1;

    if (!mUpdateMetadata) {
        const char *qupdate = UPDATE_METADATA;
        int qsize = sizeof(UPDATE_METADATA);
        SL3_prepare_v2(mDatabase, qupdate, qsize, &mUpdateMetadata, NULL);
    }
    SL3_reset(mUpdateMetadata);
    SL3_bind_int64(mUpdateMetadata, index, transactionId);
    SL3_step(mUpdateMetadata);

    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::modifyCalendars(const Notebook &notebook,
                                   DBOperation dbop, bool isDefault)
{
    int rv = 0;
    int index = 1;
    sqlite3_stmt *stmt;
    QByteArray uid = notebook.uid().toUtf8();
    QByteArray name = notebook.name().toUtf8();
    QByteArray description = notebook.description().toUtf8();
    QByteArray color = notebook.color().toUtf8();
    QByteArray plugin = notebook.pluginName().toUtf8();
    QByteArray account = notebook.account().toUtf8();
    QByteArray sharedWith = notebook.sharedWithStr().toUtf8();
    QByteArray syncProfile = notebook.syncProfile().toUtf8();

    sqlite3_int64  secs;
    const char *operation = (dbop == DBInsert) ? "inserting" :
                            (dbop == DBUpdate) ? "updating" : "deleting";

    switch (dbop) {
    case DBDelete:
        if (!d->mDeleteCalendar) {
            const char *query = DELETE_CALENDARS;
            int qsize = sizeof(DELETE_CALENDARS);
            SL3_prepare_v2(d->mDatabase, query, qsize, &d->mDeleteCalendar, nullptr);
        }
        SL3_reset(d->mDeleteCalendar);
        SL3_bind_text(d->mDeleteCalendar, index, uid, uid.length(), SQLITE_STATIC);
        stmt = d->mDeleteCalendar;
        break;
    case DBInsert:
        if (!d->mInsertCalendar) {
            const char *query = INSERT_CALENDARS;
            int qsize = sizeof(INSERT_CALENDARS);
            SL3_prepare_v2(d->mDatabase, query, qsize, &d->mInsertCalendar, nullptr);
        }
        SL3_reset(d->mInsertCalendar);
        SL3_bind_text(d->mInsertCalendar, index, uid, uid.length(), SQLITE_STATIC);
        stmt = d->mInsertCalendar;
        break;
    case DBUpdate:
        if (!d->mUpdateCalendar) {
            const char *query = UPDATE_CALENDARS;
            int qsize = sizeof(UPDATE_CALENDARS);
            SL3_prepare_v2(d->mDatabase, query, qsize, &d->mUpdateCalendar, nullptr);
        }
        SL3_reset(d->mUpdateCalendar);
        stmt = d->mUpdateCalendar;
        break;
    default:
        qCWarning(lcMkcal) << "unknown notebook DB operation" << dbop;
        return false;
    }

    qCDebug(lcMkcal) << operation << "notebook" << uid << name << "in database";

    if (dbop == DBInsert || dbop == DBUpdate) {
        int flags = 0;
        if (isDefault) {
            const char *query = UNSET_FLAG_FROM_CALENDAR;
            int qsize = sizeof(UNSET_FLAG_FROM_CALENDAR);
            sqlite3_stmt *unset;
            int idx = 1;
            SL3_prepare_v2(d->mDatabase, query, qsize, &unset, nullptr);
            SL3_bind_int(unset, idx, SqliteFormat::Default);
            SL3_step(unset);
            sqlite3_finalize(unset);
            flags |= SqliteFormat::Default;
        }
        flags |= notebook.eventsAllowed() ? SqliteFormat::AllowEvents : 0;
        flags |= notebook.todosAllowed() ? SqliteFormat::AllowTodos : 0;
        flags |= notebook.journalsAllowed() ? SqliteFormat::AllowJournals : 0;
        flags |= notebook.isShared() ? SqliteFormat::Shared : 0;
        flags |= notebook.isMaster() ? SqliteFormat::Master : 0;
        flags |= notebook.isSynchronized() ? SqliteFormat::Synchronized : 0;
        flags |= notebook.isReadOnly() ? SqliteFormat::ReadOnly : 0;
        flags |= notebook.isVisible() ? SqliteFormat::Visible : 0;
        flags |= notebook.isRunTimeOnly() ? SqliteFormat::RunTimeOnly : 0;
        flags |= notebook.isShareable() ? SqliteFormat::Shareable : 0;
        SL3_bind_text(stmt, index, name, name.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, description, description.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, color, color.length(), SQLITE_STATIC);
        SL3_bind_int(stmt, index, flags);
        secs = toOriginTime(notebook.syncDate().toUTC());
        SL3_bind_int64(stmt, index, secs);
        SL3_bind_text(stmt, index, plugin, plugin.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, account, account.length(), SQLITE_STATIC);
        SL3_bind_int64(stmt, index, notebook.attachmentSize());
        secs = toOriginTime(notebook.modifiedDate().toUTC());
        SL3_bind_int64(stmt, index, secs);
        SL3_bind_text(stmt, index, sharedWith, sharedWith.length(), SQLITE_STATIC);
        SL3_bind_text(stmt, index, syncProfile, syncProfile.length(), SQLITE_STATIC);
        secs = toOriginTime(notebook.creationDate().isValid()
                            ? notebook.creationDate().toUTC()
                            : QDateTime::currentDateTimeUtc());
        SL3_bind_int64(stmt, index, secs);

        if (dbop == DBUpdate)
            SL3_bind_text(stmt, index, uid, uid.length(), SQLITE_STATIC);
    }

    SL3_step(stmt);

    if (!d->modifyCalendarProperties(notebook, dbop)) {
        qCWarning(lcMkcal) << "failed to modify calendarproperties for notebook" << uid;
    }

    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(d->mDatabase);
    return false;
}

bool SqliteFormat::purgeDeletedComponents(const KCalendarCore::Incidence &incidence)
{
    int rv;
    int index = 1;
    const QByteArray u(incidence.uid().toUtf8());
    qint64 secsRecurId = 0;

    if (incidence.hasRecurrenceId() && incidence.recurrenceId().timeSpec() == Qt::LocalTime) {
        secsRecurId = toLocalOriginTime(incidence.recurrenceId());
    } else if (incidence.hasRecurrenceId()) {
        secsRecurId = toOriginTime(incidence.recurrenceId());
    }

    if (!d->mDeleteIncComponents) {
        const char *query = DELETE_COMPONENTS;
        int qsize = sizeof(DELETE_COMPONENTS);
        SL3_prepare_v2(d->mDatabase, query, qsize, &d->mDeleteIncComponents, nullptr);
    }

    if (!d->mSelectDeletedIncidences) {
        const char *query = SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED;
        int qsize = sizeof(SELECT_COMPONENTS_BY_UID_RECID_AND_DELETED);
        SL3_prepare_v2(d->mDatabase, query, qsize, &d->mSelectDeletedIncidences, nullptr);
    }
    SL3_reset(d->mSelectDeletedIncidences);
    SL3_bind_text(d->mSelectDeletedIncidences, index, u.constData(), u.length(), SQLITE_STATIC);
    SL3_bind_int64(d->mSelectDeletedIncidences, index, secsRecurId);

    SL3_step(d->mSelectDeletedIncidences);
    while (rv == SQLITE_ROW) {
        int rowid = sqlite3_column_int(d->mSelectDeletedIncidences, 0);

        int index2 = 1;
        SL3_reset(d->mDeleteIncComponents);
        SL3_bind_int(d->mDeleteIncComponents, index2, rowid);
        SL3_step(d->mDeleteIncComponents);

        if (!d->deleteListsForIncidence(rowid)) {
            qCWarning(lcMkcal) << "failed to delete lists for incidence" << u;
        }

        SL3_step(d->mSelectDeletedIncidences);
    }

    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(d->mDatabase);
    return false;
}

static bool setDateTime(SqliteFormat *format, sqlite3_stmt *stmt, int &index, const QDateTime &dateTime, bool allDay)
{
    int rv = 0;
    sqlite3_int64 secs;
    QByteArray tz;

    if (dateTime.isValid()) {
        secs = (dateTime.timeSpec() == Qt::LocalTime || allDay)
            ? format->toLocalOriginTime(dateTime) : format->toOriginTime(dateTime);
        SL3_bind_int64(stmt, index, secs);
        secs = format->toLocalOriginTime(dateTime);
        SL3_bind_int64(stmt, index, secs);
        if (allDay) {
            tz = FLOATING_DATE;
        } else if (dateTime.timeSpec() != Qt::LocalTime) {
            tz = dateTime.timeZone().id();
        }
        SL3_bind_text(stmt, index, tz.constData(), tz.length(), SQLITE_TRANSIENT);
    } else {
        SL3_bind_int(stmt, index, 0);
        SL3_bind_int(stmt, index, 0);
        SL3_bind_text(stmt, index, "", 0, SQLITE_STATIC);
    }
    return true;
 error:
    return false;
}

#define SL3_bind_date_time( format, stmt, index, dt, allDay)           \
    {                                                                  \
        if (!setDateTime(format, stmt, index, dt, allDay))             \
            goto error;                                                \
    }

bool SqliteFormat::modifyComponents(const Incidence &incidence, const QString &nbook,
                                    DBOperation dbop)
{
    int rv = 0;
    int index = 1;
    QByteArray uid;
    QByteArray notebook;
    QByteArray type;
    QByteArray summary;
    QByteArray category;
    QByteArray location;
    QByteArray description;
    QByteArray url;
    QByteArray contact;
    QByteArray relatedtouid;
    QByteArray colorstr;
    QByteArray comments;
    QByteArray resources;
    QDateTime dt;
    sqlite3_int64 secs;
    int rowid = 0;
    sqlite3_stmt *stmt1;

    if (dbop == DBDelete || dbop == DBMarkDeleted || dbop == DBUpdate) {
        rowid = d->selectRowId(incidence);
        if (!rowid && dbop == DBDelete) {
            // Already deleted.
            return true;
        } else if (!rowid) {
            qCWarning(lcMkcal) << "failed to select rowid of incidence" << incidence.uid() << incidence.recurrenceId();
            goto error;
        }
    }

    switch (dbop) {
    case DBDelete:
        if (!d->mDeleteIncComponents) {
            const char *query = DELETE_COMPONENTS;
            int qsize = sizeof(DELETE_COMPONENTS);
            SL3_prepare_v2(d->mDatabase, query, qsize, &d->mDeleteIncComponents, nullptr);
        }
        SL3_reset(d->mDeleteIncComponents);
        SL3_bind_int(d->mDeleteIncComponents, index, rowid);
        stmt1 = d->mDeleteIncComponents;
        break;
    case DBMarkDeleted:
        if (!d->mMarkDeletedIncidences) {
            const char *query = UPDATE_COMPONENTS_AS_DELETED;
            int qsize = sizeof(UPDATE_COMPONENTS_AS_DELETED);
            SL3_prepare_v2(d->mDatabase, query, qsize, &d->mMarkDeletedIncidences, nullptr);
        }
        SL3_reset(d->mMarkDeletedIncidences);
        secs = toOriginTime(QDateTime::currentDateTimeUtc());
        SL3_bind_int64(d->mMarkDeletedIncidences, index, secs);
        SL3_bind_int(d->mMarkDeletedIncidences, index, rowid);
        stmt1 = d->mMarkDeletedIncidences;
        break;
    case DBInsert:
        if (!d->mInsertIncComponents) {
            const char *query = INSERT_COMPONENTS;
            int qsize = sizeof(INSERT_COMPONENTS);
            SL3_prepare_v2(d->mDatabase, query, qsize, &d->mInsertIncComponents, nullptr);
        }
        SL3_reset(d->mInsertIncComponents);
        stmt1 = d->mInsertIncComponents;
        break;
    case DBUpdate:
        if (!d->mUpdateIncComponents) {
            const char *query = UPDATE_COMPONENTS;
            int qsize = sizeof(UPDATE_COMPONENTS);
            SL3_prepare_v2(d->mDatabase, query, qsize, &d->mUpdateIncComponents, nullptr);
        }
        SL3_reset(d->mUpdateIncComponents);
        stmt1 = d->mUpdateIncComponents;
        break;
    default:
        qCWarning(lcMkcal) << "unknown DB operation" << dbop;
        goto error;
    }

    if (dbop == DBInsert || dbop == DBUpdate) {
        notebook = nbook.toUtf8();
        SL3_bind_text(stmt1, index, notebook.constData(), notebook.length(), SQLITE_STATIC);

        switch (incidence.type()) {
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
            goto error;
        }
        SL3_bind_text(stmt1, index, type.constData(), type.length(), SQLITE_STATIC);   // NOTE

        summary = incidence.summary().toUtf8();
        SL3_bind_text(stmt1, index, summary.constData(), summary.length(), SQLITE_STATIC);

        category = incidence.categoriesStr().toUtf8();
        SL3_bind_text(stmt1, index, category.constData(), category.length(), SQLITE_STATIC);

        if ((incidence.type() == Incidence::TypeEvent) ||
                (incidence.type() == Incidence::TypeJournal)) {
            SL3_bind_date_time(this, stmt1, index, incidence.dtStart(), incidence.allDay());

            // set HasDueDate to false
            SL3_bind_int(stmt1, index, 0);

            QDateTime effectiveDtEnd;
            if (incidence.type() == Incidence::TypeEvent) {
                const Event &event = static_cast<const Event&>(incidence);
                if (event.hasEndDate()) {
                    // Keep this one day addition for backward compatibility reasons
                    // with existing events in database.
                    if (incidence.allDay()) {
                        effectiveDtEnd = event.dtEnd().addDays(1);
                    } else {
                        effectiveDtEnd = event.dtEnd();
                    }
                }
            }
            SL3_bind_date_time(this, stmt1, index, effectiveDtEnd, incidence.allDay());
        } else if (incidence.type() == Incidence::TypeTodo) {
            const Todo &todo = static_cast<const Todo&>(incidence);
            SL3_bind_date_time(this, stmt1, index,
                               todo.hasStartDate() ? todo.dtStart(true) : QDateTime(), todo.allDay());

            SL3_bind_int(stmt1, index, (int) todo.hasDueDate());

            SL3_bind_date_time(this, stmt1, index, todo.hasDueDate() ? todo.dtDue(true) : QDateTime(), todo.allDay());
        }

        if (incidence.type() != Incidence::TypeJournal) {
            SL3_bind_int(stmt1, index, incidence.duration().asSeconds()); // NOTE
        } else {
            SL3_bind_int(stmt1, index, 0);
        }

        SL3_bind_int(stmt1, index, incidence.secrecy()); // NOTE

        if (incidence.type() != Incidence::TypeJournal) {
            location = incidence.location().toUtf8();
            SL3_bind_text(stmt1, index, location.constData(), location.length(), SQLITE_STATIC);
        } else {
            SL3_bind_text(stmt1, index, "", 0, SQLITE_STATIC);
        }

        description = incidence.description().toUtf8();
        SL3_bind_text(stmt1, index, description.constData(), description.length(), SQLITE_STATIC);

        SL3_bind_int(stmt1, index, incidence.status()); // NOTE

        if (incidence.type() != Incidence::TypeJournal) {
            if (incidence.hasGeo()) {
                SL3_bind_double(stmt1, index, incidence.geoLatitude());
                SL3_bind_double(stmt1, index, incidence.geoLongitude());
            } else {
                SL3_bind_double(stmt1, index, INVALID_LATLON);
                SL3_bind_double(stmt1, index, INVALID_LATLON);
            }

            SL3_bind_int(stmt1, index, incidence.priority());

            resources = incidence.resources().join(" ").toUtf8();
            SL3_bind_text(stmt1, index, resources.constData(), resources.length(), SQLITE_STATIC);
        } else {
            SL3_bind_double(stmt1, index, INVALID_LATLON);
            SL3_bind_double(stmt1, index, INVALID_LATLON);
            SL3_bind_int(stmt1, index, 0);
            SL3_bind_text(stmt1, index, "", 0, SQLITE_STATIC);
        }

        secs = toOriginTime(incidence.created());
        SL3_bind_int64(stmt1, index, secs);

        secs = toOriginTime(QDateTime::currentDateTimeUtc());
        SL3_bind_int64(stmt1, index, secs);   // datestamp

        secs = toOriginTime(incidence.lastModified());
        SL3_bind_int64(stmt1, index, secs);

        SL3_bind_int(stmt1, index, incidence.revision());

        comments = incidence.comments().join(" ").toUtf8();
        SL3_bind_text(stmt1, index, comments.constData(), comments.length(), SQLITE_STATIC);

        // Attachments are now stored in a dedicated table.
        SL3_bind_text(stmt1, index, nullptr, 0, SQLITE_STATIC);

        contact = incidence.contacts().join(" ").toUtf8();
        SL3_bind_text(stmt1, index, contact.constData(), contact.length(), SQLITE_STATIC);

        // Never save recurrenceId as FLOATING_DATE, because the time of a
        // floating date is not guaranteed on read and recurrenceId is used
        // for date-time comparisons.
        SL3_bind_date_time(this, stmt1, index, incidence.recurrenceId(), false);

        relatedtouid = incidence.relatedTo().toUtf8();
        SL3_bind_text(stmt1, index, relatedtouid.constData(), relatedtouid.length(), SQLITE_STATIC);

        url = incidence.url().toString().toUtf8();
        SL3_bind_text(stmt1, index, url.constData(), url.length(), SQLITE_STATIC);

        uid = incidence.uid().toUtf8();
        SL3_bind_text(stmt1, index, uid.constData(), uid.length(), SQLITE_STATIC);

        if (incidence.type() == Incidence::TypeEvent) {
            const Event &event = static_cast<const Event&>(incidence);
            SL3_bind_int(stmt1, index, (int)event.transparency());
        } else {
            SL3_bind_int(stmt1, index, 0);
        }

        SL3_bind_int(stmt1, index, (int) incidence.localOnly());

        int percentComplete = 0;
        QDateTime effectiveDtCompleted;
        if (incidence.type() == Incidence::TypeTodo) {
            const Todo &todo = static_cast<const Todo&>(incidence);
            percentComplete = todo.percentComplete();
            if (todo.isCompleted()) {
                if (!todo.hasCompletedDate()) {
                    effectiveDtCompleted = QDateTime::currentDateTimeUtc();
                } else {
                    effectiveDtCompleted = todo.completed();
                }
            }
        }
        SL3_bind_int(stmt1, index, percentComplete);
        SL3_bind_date_time(this, stmt1, index, effectiveDtCompleted, incidence.allDay());

        colorstr = incidence.color().toUtf8();
        SL3_bind_text(stmt1, index, colorstr.constData(), colorstr.length(), SQLITE_STATIC);

        if (dbop == DBUpdate)
            SL3_bind_int(stmt1, index, rowid);
    }

    SL3_step(stmt1);

    if ((dbop == DBDelete || dbop == DBUpdate) && !d->deleteListsForIncidence(rowid)) {
        qCWarning(lcMkcal) << "failed to delete lists for incidence" << incidence.uid();
    } else if (dbop == DBInsert || dbop == DBUpdate) {
        if (dbop == DBInsert)
            rowid = sqlite3_last_insert_rowid(d->mDatabase);

        if (!d->insertCustomproperties(incidence, rowid))
            qCWarning(lcMkcal) << "failed to modify customproperties for incidence" << incidence.uid();

        if (!d->insertAttendees(incidence, rowid))
            qCWarning(lcMkcal) << "failed to modify attendees for incidence" << incidence.uid();

        if (!d->insertAlarms(incidence, rowid))
            qCWarning(lcMkcal) << "failed to modify alarms for incidence" << incidence.uid();

        if (!d->insertRecursives(incidence, rowid))
            qCWarning(lcMkcal) << "failed to modify recursives for incidence" << incidence.uid();

        if (!d->insertRdates(incidence, rowid))
            qCWarning(lcMkcal) << "failed to modify rdates for incidence" << incidence.uid();

        if (!d->insertAttachments(incidence, rowid))
            qCWarning(lcMkcal) << "failed to modify attachments for incidence" << incidence.uid();
    }

    return true;

error:
    return false;
}

//@cond PRIVATE
bool SqliteFormat::Private::deleteListsForIncidence(int rowid)
{
    int rv = 0;
    int index = 1;

    if (!mDeleteIncProperties) {
        const char *query = DELETE_CUSTOMPROPERTIES;
        int qsize = sizeof(DELETE_CUSTOMPROPERTIES);
        SL3_prepare_v2(mDatabase, query, qsize, &mDeleteIncProperties, nullptr);
    }
    SL3_reset(mDeleteIncProperties);
    index = 1;
    SL3_bind_int(mDeleteIncProperties, index, rowid);
    SL3_step(mDeleteIncProperties);

    if (!mDeleteIncAlarms) {
        const char *query = DELETE_ALARM;
        int qsize = sizeof(DELETE_ALARM);
        SL3_prepare_v2(mDatabase, query, qsize, &mDeleteIncAlarms, nullptr);
    }
    SL3_reset(mDeleteIncAlarms);
    index = 1;
    SL3_bind_int(mDeleteIncAlarms, index, rowid);
    SL3_step(mDeleteIncAlarms);

    if (!mDeleteIncAttendees) {
        const char *query = DELETE_ATTENDEE;
        int qsize = sizeof(DELETE_ATTENDEE);
        SL3_prepare_v2(mDatabase, query, qsize, &mDeleteIncAttendees, nullptr);
    }
    SL3_reset(mDeleteIncAttendees);
    index = 1;
    SL3_bind_int(mDeleteIncAttendees, index, rowid);
    SL3_step(mDeleteIncAttendees);

    if (!mDeleteIncRecursives) {
        const char *query = DELETE_RECURSIVE;
        int qsize = sizeof(DELETE_RECURSIVE);
        SL3_prepare_v2(mDatabase, query, qsize, &mDeleteIncRecursives, nullptr);
    }
    SL3_reset(mDeleteIncRecursives);
    index = 1;
    SL3_bind_int(mDeleteIncRecursives, index, rowid);
    SL3_step(mDeleteIncRecursives);

    if (!mDeleteIncRDates) {
        const char *query = DELETE_RDATES;
        int qsize = sizeof(DELETE_RDATES);
        SL3_prepare_v2(mDatabase, query, qsize, &mDeleteIncRDates, nullptr);
    }
    SL3_reset(mDeleteIncRDates);
    index = 1;
    SL3_bind_int(mDeleteIncRDates, index, rowid);
    SL3_step(mDeleteIncRDates);

    if (!mDeleteIncAttachments) {
        const char *query = DELETE_ATTACHMENTS;
        int qsize = sizeof(DELETE_ATTACHMENTS);
        SL3_prepare_v2(mDatabase, query, qsize, &mDeleteIncAttachments, nullptr);
    }
    SL3_reset(mDeleteIncAttachments);
    index = 1;
    SL3_bind_int(mDeleteIncAttachments, index, rowid);
    SL3_step(mDeleteIncAttachments);

    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::Private::insertCustomproperties(const Incidence &incidence, int rowid)
{
    bool success = true;

    QMap<QByteArray, QString> mProperties = incidence.customProperties();
    for (QMap<QByteArray, QString>::ConstIterator it = mProperties.begin(); it != mProperties.end(); ++it) {
        if (!insertCustomproperty(rowid, it.key(), it.value(),
                                  incidence.nonKDECustomPropertyParameters(it.key()))) {
            qCWarning(lcMkcal) << "failed to modify customproperty for incidence" << incidence.uid();
            success = false;
        }
    }

    return success;
}

bool SqliteFormat::Private::insertCustomproperty(int rowid, const QByteArray &key,
                                                 const QString &value, const QString &parameters)
{
    int rv = 0;
    int index = 1;
    QByteArray valueba;
    QByteArray parametersba;

    if (!mInsertIncProperties) {
        const char *query = INSERT_CUSTOMPROPERTIES;
        int qsize = sizeof(INSERT_CUSTOMPROPERTIES);
        SL3_prepare_v2(mDatabase, query, qsize, &mInsertIncProperties, nullptr);
    }
    SL3_reset(mInsertIncProperties);
    SL3_bind_int(mInsertIncProperties, index, rowid);
    SL3_bind_text(mInsertIncProperties, index, key.constData(), key.length(), SQLITE_STATIC);
    valueba = value.toUtf8();
    SL3_bind_text(mInsertIncProperties, index, valueba.constData(), valueba.length(), SQLITE_STATIC);
    parametersba = parameters.toUtf8();
    SL3_bind_text(mInsertIncProperties, index, parametersba.constData(), parametersba.length(), SQLITE_STATIC);

    SL3_step(mInsertIncProperties);
    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::Private::insertRdates(const Incidence &incidence, int rowid)
{
    bool success = true;

    int type = SqliteFormat::RDate;
    DateList dateList = incidence.recurrence()->rDates();
    DateList::ConstIterator dt;
    for (dt = dateList.constBegin(); dt != dateList.constEnd(); ++dt) {
        if (!insertRdate(rowid, type, QDateTime((*dt)), true)) {
            qCWarning(lcMkcal) << "failed to modify rdates for incidence" << incidence.uid();
            success = false;
        }
    }

    type = SqliteFormat::XDate;
    dateList = incidence.recurrence()->exDates();
    for (dt = dateList.constBegin(); dt != dateList.constEnd(); ++dt) {
        if (!insertRdate(rowid, type, QDateTime((*dt)), true)) {
            qCWarning(lcMkcal) << "failed to modify xdates for incidence" << incidence.uid();
            success = false;
        }
    }

    // Both for rDateTimes and exDateTimes, there are possible issues
    // with all day events. KCalendarCore::Recurrence::timesInInterval()
    // is returning repeating events in clock time for all day events,
    // Thus being yyyy-mm-ddT00:00:00 and then "converted" to local
    // zone, for display (meaning being after yyyy-mm-ddT00:00:00+xxxx).
    // When saving, we don't want to store this local zone info, otherwise,
    // the saved date-time won't match when read in another time zone.
    type = SqliteFormat::RDateTime;
    DateTimeList dateTimeList = incidence.recurrence()->rDateTimes();
    DateTimeList::ConstIterator it;
    for (it = dateTimeList.constBegin(); it != dateTimeList.constEnd(); ++it) {
        bool allDay(incidence.allDay() && it->timeSpec() == Qt::LocalTime && it->time() == QTime(0,0));
        if (!insertRdate(rowid, type, *it, allDay)) {
            qCWarning(lcMkcal) << "failed to modify rdatetimes for incidence" << incidence.uid();
            success = false;
        }
    }

    type = SqliteFormat::XDateTime;
    dateTimeList = incidence.recurrence()->exDateTimes();
    for (it = dateTimeList.constBegin(); it != dateTimeList.constEnd(); ++it) {
        bool allDay(incidence.allDay() && it->timeSpec() == Qt::LocalTime && it->time() == QTime(0,0));
        if (!insertRdate(rowid, type, *it, allDay)) {
            qCWarning(lcMkcal) << "failed to modify xdatetimes for incidence" << incidence.uid();
            success = false;
        }
    }

    return success;
}

bool SqliteFormat::Private::insertRdate(int rowid, int type, const QDateTime &date, bool allDay)
{
    int rv = 0;
    int index = 1;

    if (!mInsertIncRDates) {
        const char *query = INSERT_RDATES;
        int qsize = sizeof(INSERT_RDATES);
        SL3_prepare_v2(mDatabase, query, qsize, &mInsertIncRDates, nullptr);
    }
    SL3_reset(mInsertIncRDates);
    SL3_bind_int(mInsertIncRDates, index, rowid);
    SL3_bind_int(mInsertIncRDates, index, type);
    SL3_bind_date_time(mFormat, mInsertIncRDates, index, date, allDay);

    SL3_step(mInsertIncRDates);
    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::Private::insertAlarms(const Incidence &incidence, int rowid)
{
    bool success = true;

    const Alarm::List &list = incidence.alarms();
    Alarm::List::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        if (!insertAlarm(rowid, *it)) {
            qCWarning(lcMkcal) << "failed to modify alarm for incidence" << incidence.uid();
            success = false;
        }
    }

    return success;
}

bool SqliteFormat::Private::insertAlarm(int rowid, Alarm::Ptr alarm)
{
    int rv = 0;
    int index = 1;
    QByteArray description;
    QByteArray relation;
    QByteArray attachment;
    QByteArray addresses;
    QByteArray summary;
    QByteArray properties;
    QStringList list;
    const QMap<QByteArray, QString> custom = alarm->customProperties();
    int action = 0; // default Alarm::Invalid
    Alarm::Type type = alarm->type();

    if (!mInsertIncAlarms) {
        const char *query = INSERT_ALARM;
        int qsize = sizeof(INSERT_ALARM);
        SL3_prepare_v2(mDatabase, query, qsize, &mInsertIncAlarms, nullptr);
    }
    SL3_reset(mInsertIncAlarms);
    SL3_bind_int(mInsertIncAlarms, index, rowid);

    switch (type) {
    case Alarm::Display:
        action = 1;
        description = alarm->text().toUtf8();
        break;
    case Alarm::Procedure:
        action = 2;
        attachment = alarm->programFile().toUtf8();
        if (!alarm->programArguments().isEmpty())
            description = alarm->programArguments().toUtf8();
        break;
    case Alarm::Email:
        action = 3;
        summary = alarm->mailSubject().toUtf8();
        description = alarm->mailText().toUtf8();
        if (alarm->mailAttachments().size() > 0)
            attachment = alarm->mailAttachments().join(" ").toUtf8();
        if (alarm->mailAddresses().size() > 0) {
            QStringList mailaddresses;
            for (int i = 0; i < alarm->mailAddresses().size(); i++) {
                mailaddresses << alarm->mailAddresses().at(i).email();
            }
            addresses = mailaddresses.join(" ").toUtf8();
        }
        break;
    case Alarm::Audio:
        action = 4;
        if (!alarm->audioFile().isEmpty())
            attachment = alarm->audioFile().toUtf8();
        break;
    default:
        break;
    }

    SL3_bind_int(mInsertIncAlarms, index, action);

    if (alarm->repeatCount()) {
        SL3_bind_int(mInsertIncAlarms, index, alarm->repeatCount());
        SL3_bind_int(mInsertIncAlarms, index, alarm->snoozeTime().asSeconds());
    } else {
        SL3_bind_int(mInsertIncAlarms, index, 0);
        SL3_bind_int(mInsertIncAlarms, index, 0);
    }

    if (alarm->hasStartOffset()) {
        SL3_bind_int(mInsertIncAlarms, index, alarm->startOffset().asSeconds());
        relation = QString("startTriggerRelation").toUtf8();
        SL3_bind_text(mInsertIncAlarms, index, relation.constData(), relation.length(), SQLITE_STATIC);
        SL3_bind_int(mInsertIncAlarms, index, 0); // time
        SL3_bind_int(mInsertIncAlarms, index, 0); // localtime
        SL3_bind_text(mInsertIncAlarms, index, "", 0, SQLITE_STATIC);
    } else if (alarm->hasEndOffset()) {
        SL3_bind_int(mInsertIncAlarms, index, alarm->endOffset().asSeconds());
        relation = QString("endTriggerRelation").toUtf8();
        SL3_bind_text(mInsertIncAlarms, index, relation.constData(), relation.length(), SQLITE_STATIC);
        SL3_bind_int(mInsertIncAlarms, index, 0); // time
        SL3_bind_int(mInsertIncAlarms, index, 0); // localtime
        SL3_bind_text(mInsertIncAlarms, index, "", 0, SQLITE_STATIC);
    } else {
        SL3_bind_int(mInsertIncAlarms, index, 0); // offset
        SL3_bind_text(mInsertIncAlarms, index, "", 0, SQLITE_STATIC); // relation
        SL3_bind_date_time(mFormat, mInsertIncAlarms, index, alarm->time(), false);
    }

    SL3_bind_text(mInsertIncAlarms, index, description.constData(), description.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertIncAlarms, index, attachment.constData(), attachment.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertIncAlarms, index, summary.constData(), summary.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertIncAlarms, index, addresses.constData(), addresses.length(), SQLITE_STATIC);

    for (QMap<QByteArray, QString>::ConstIterator c = custom.begin(); c != custom.end();  ++c) {
        list.append(c.key());
        list.append(c.value());
    }
    if (!list.isEmpty())
        properties = list.join("\r\n").toUtf8();

    SL3_bind_text(mInsertIncAlarms, index, properties.constData(), properties.length(), SQLITE_STATIC);
    SL3_bind_int(mInsertIncAlarms, index, (int)alarm->enabled());

    SL3_step(mInsertIncAlarms);
    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::Private::insertRecursives(const Incidence &incidence, int rowid)
{
    bool success = true;

    const RecurrenceRule::List &listRR = incidence.recurrence()->rRules();
    RecurrenceRule::List::ConstIterator it;
    for (it = listRR.begin(); it != listRR.end(); ++it) {
        if (!insertRecursive(rowid, *it, 1)) {
            qCWarning(lcMkcal) << "failed to modify recursive for incidence" << incidence.uid();
            success = false;
        }
    }
    const RecurrenceRule::List &listER = incidence.recurrence()->exRules();
    for (it = listER.begin(); it != listER.end(); ++it) {
        if (!insertRecursive(rowid, *it, 2)) {
            qCWarning(lcMkcal) << "failed to modify recursive for incidence" << incidence.uid();
            success = false;
        }
    }

    return success;
}

bool SqliteFormat::Private::insertRecursive(int rowid, RecurrenceRule *rule, int type)
{
    int rv = 0;
    int index = 1;

    QByteArray bySeconds;
    QByteArray byMinutes;
    QByteArray byHours;
    QByteArray byDays;
    QByteArray byDayPoss;
    QByteArray byMonthDays;
    QByteArray byYearDays;
    QByteArray byWeekNumbers;
    QByteArray byMonths;
    QByteArray bySetPos;

    if (!mInsertIncRecursives) {
        const char *query = INSERT_RECURSIVE;
        int qsize = sizeof(INSERT_RECURSIVE);
        SL3_prepare_v2(mDatabase, query, qsize, &mInsertIncRecursives, nullptr);
    }
    SL3_reset(mInsertIncRecursives);
    SL3_bind_int(mInsertIncRecursives, index, rowid);

    SL3_bind_int(mInsertIncRecursives, index, type);

    SL3_bind_int(mInsertIncRecursives, index, (int)rule->recurrenceType()); // frequency

    SL3_bind_date_time(mFormat, mInsertIncRecursives, index, rule->endDt(), rule->allDay());

    SL3_bind_int(mInsertIncRecursives, index, rule->duration());  // count

    SL3_bind_int(mInsertIncRecursives, index, (int)rule->frequency()); // interval

#define writeSetByList( listname )                                      \
    {                                                                   \
        QString number;                                                 \
        QStringList byL;                                                \
        QList<int>::iterator i;                                         \
        QList<int> byList;                                              \
        byList = rule->listname();                                      \
        byL.clear();                                                    \
        for (i = byList.begin(); i != byList.end(); ++i) {              \
            number.setNum(*i);                                          \
            byL << number;                                              \
        }                                                               \
        listname = byL.join(" ").toUtf8();                              \
        SL3_bind_text(mInsertIncRecursives, index, listname.constData(), listname.length(), SQLITE_STATIC); \
    }

    // BYSECOND, MINUTE and HOUR, MONTHDAY, YEARDAY, WEEKNUMBER, MONTH
    // and SETPOS are standard int lists, so we can treat them with the
    // same macro
    writeSetByList(bySeconds);
    writeSetByList(byMinutes);
    writeSetByList(byHours);

    // BYDAY is a special case, since it's not an int list
    {
        QString number;
        QStringList byL;
        QList<RecurrenceRule::WDayPos>::iterator j;
        QList<RecurrenceRule::WDayPos> wdList = rule->byDays();
        byL.clear();
        for (j = wdList.begin(); j != wdList.end(); ++j) {
            number.setNum((*j).day());
            byL << number;
        }
        byDays =  byL.join(" ").toUtf8();
        byL.clear();
        for (j = wdList.begin(); j != wdList.end(); ++j) {
            number.setNum((*j).pos());
            byL << number;
        }
        byDayPoss =  byL.join(" ").toUtf8();
    }
    SL3_bind_text(mInsertIncRecursives, index, byDays.constData(), byDays.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertIncRecursives, index, byDayPoss.constData(), byDayPoss.length(), SQLITE_STATIC);

    writeSetByList(byMonthDays);
    writeSetByList(byYearDays);
    writeSetByList(byWeekNumbers);
    writeSetByList(byMonths);
    writeSetByList(bySetPos);

#undef writeSetByList

    SL3_bind_int(mInsertIncRecursives, index, rule->weekStart());

    SL3_step(mInsertIncRecursives);
    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::Private::insertAttendees(const Incidence &incidence, int rowid)
{
    bool success = true;

    // FIXME: this doesn't fully save and restore attendees as they were set.
    // e.g. has constraints that every attendee must have email and they need to be unique among the attendees.
    // also this forces attendee list to include the organizer.
    QString organizerEmail;
    if (!incidence.organizer().isEmpty()) {
        organizerEmail = incidence.organizer().email();
        Attendee organizer = incidence.attendeeByMail(organizerEmail);
        if (organizer.isNull())
            organizer = Attendee(incidence.organizer().name(), organizerEmail);
        if (!insertAttendee(rowid, organizer, true)) {
            qCWarning(lcMkcal) << "failed to modify organizer for incidence" << incidence.uid();
            success = false;
        }
    }
    const Attendee::List &list = incidence.attendees();
    Attendee::List::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        if (it->email().isEmpty()) {
            qCWarning(lcMkcal) << "Attendee doesn't have an email address";
            continue;
        } else if (it->email() == organizerEmail) {
            continue; // already added above
        }
        if (!insertAttendee(rowid, *it, false)) {
            qCWarning(lcMkcal) << "failed to modify attendee for incidence" << incidence.uid();
            success = false;
        }
    }

    return success;
}

bool SqliteFormat::Private::insertAttendee(int rowid, const Attendee &attendee, bool isOrganizer)
{
    int rv = 0;
    int index = 1;
    QByteArray email;
    QByteArray name;
    QByteArray delegate;
    QByteArray delegator;

    if (!mInsertIncAttendees) {
        const char *query = INSERT_ATTENDEE;
        int qsize = sizeof(INSERT_ATTENDEE);
        SL3_prepare_v2(mDatabase, query, qsize, &mInsertIncAttendees, nullptr);
    }
    SL3_reset(mInsertIncAttendees);
    SL3_bind_int(mInsertIncAttendees, index, rowid);

    email = attendee.email().toUtf8();
    SL3_bind_text(mInsertIncAttendees, index, email.constData(), email.length(), SQLITE_STATIC);

    name = attendee.name().toUtf8();
    SL3_bind_text(mInsertIncAttendees, index, name.constData(), name.length(), SQLITE_STATIC);

    SL3_bind_int(mInsertIncAttendees, index, (int)isOrganizer);

    SL3_bind_int(mInsertIncAttendees, index, (int)attendee.role());

    SL3_bind_int(mInsertIncAttendees, index, (int)attendee.status());

    SL3_bind_int(mInsertIncAttendees, index, (int)attendee.RSVP());

    delegate = attendee.delegate().toUtf8();
    SL3_bind_text(mInsertIncAttendees, index, delegate.constData(), delegate.length(), SQLITE_STATIC);

    delegator = attendee.delegator().toUtf8();
    SL3_bind_text(mInsertIncAttendees, index, delegator.constData(), delegator.length(), SQLITE_STATIC);

    SL3_step(mInsertIncAttendees);
    return true;

error:
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::Private::insertAttachments(const Incidence &incidence, int rowid)
{
    const Attachment::List &list = incidence.attachments();
    Attachment::List::ConstIterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        int rv = 0;
        int index = 1;

        if (!mInsertIncAttachments) {
            const char *query = INSERT_ATTACHMENTS;
            int qsize = sizeof(INSERT_ATTACHMENTS);
            SL3_prepare_v2(mDatabase, query, qsize, &mInsertIncAttachments, nullptr);
        }
        SL3_reset(mInsertIncAttachments);
        SL3_bind_int(mInsertIncAttachments, index, rowid);
        QByteArray uri; // must remain valid instance until end of the scope
        if (it->isBinary()) {
            SL3_bind_blob(mInsertIncAttachments, index, it->decodedData().constData(), it->size(), SQLITE_STATIC);
            SL3_bind_text(mInsertIncAttachments, index, nullptr, 0, SQLITE_STATIC);
        } else if (it->isUri()) {
            uri = it->uri().toUtf8();
            SL3_bind_blob(mInsertIncAttachments, index, nullptr, 0, SQLITE_STATIC);
            SL3_bind_text(mInsertIncAttachments, index, uri.constData(), uri.length(), SQLITE_STATIC);
        } else {
            continue;
        }
        const QByteArray mime = it->mimeType().toUtf8();
        SL3_bind_text(mInsertIncAttachments, index, mime.constData(), mime.length(), SQLITE_STATIC);
        SL3_bind_int(mInsertIncAttachments, index, (it->showInline() ? 1 : 0));
        const QByteArray label = it->label().toUtf8();
        SL3_bind_text(mInsertIncAttachments, index, label.constData(), label.length(), SQLITE_STATIC);
        SL3_bind_int(mInsertIncAttachments, index, (it->isLocal() ? 1 : 0));
        SL3_step(mInsertIncAttachments);
    }

    return true;

error:
    qCWarning(lcMkcal) << "cannot modify attachment for incidence" << incidence.instanceIdentifier();
    qCWarning(lcMkcal) << "Sqlite error:" << sqlite3_errmsg(mDatabase);
    return false;
}

bool SqliteFormat::Private::modifyCalendarProperties(const Notebook &notebook,
                                                     DBOperation dbop)
{
    QByteArray id(notebook.uid().toUtf8());
    // In Update always delete all first then insert all
    if (dbop == DBUpdate && !deleteCalendarProperties(id)) {
        qCWarning(lcMkcal) << "failed to delete calendarproperties for notebook" << id;
        return false;
    }

    bool success = true;
    if (dbop == DBInsert || dbop == DBUpdate) {
        QList<QByteArray> properties = notebook.customPropertyKeys();
        for (QList<QByteArray>::ConstIterator it = properties.constBegin();
             it != properties.constEnd(); ++it) {
            if (!insertCalendarProperty(id, *it, notebook.customProperty(*it).toUtf8())) {
                qCWarning(lcMkcal) << "failed to insert calendarproperty" << *it << "in notebook" << id;
                success = false;
            }
        }
    }
    return success;
}

bool SqliteFormat::Private::deleteCalendarProperties(const QByteArray &id)
{
    int rv = 0;
    int index = 1;
    bool success = false;

    const char *query = DELETE_CALENDARPROPERTIES;
    int qsize = sizeof(DELETE_CALENDARPROPERTIES);
    sqlite3_stmt *stmt = NULL;

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, NULL);
    SL3_bind_text(stmt, index, id.constData(), id.length(), SQLITE_STATIC);
    SL3_step(stmt);
    success = true;

error:
    sqlite3_finalize(stmt);

    return success;
}

bool SqliteFormat::Private::insertCalendarProperty(const QByteArray &id,
                                                   const QByteArray &key,
                                                   const QByteArray &value)
{
    int rv = 0;
    int index = 1;
    bool success = false;

    if (!mInsertCalProps) {
        const char *query = INSERT_CALENDARPROPERTIES;
        int qsize = sizeof(INSERT_CALENDARPROPERTIES);
        SL3_prepare_v2(mDatabase, query, qsize, &mInsertCalProps, NULL);
    }

    SL3_bind_text(mInsertCalProps, index, id.constData(), id.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertCalProps, index, key.constData(), key.length(), SQLITE_STATIC);
    SL3_bind_text(mInsertCalProps, index, value.constData(), value.length(), SQLITE_STATIC);
    SL3_step(mInsertCalProps);
    success = true;

error:
    sqlite3_reset(mInsertCalProps);

    return success;
}
//@endcond

Notebook* SqliteFormat::selectCalendars(sqlite3_stmt *stmt, bool *isDefault)
{
    int rv = 0;
    Notebook* notebook = nullptr;
    sqlite3_int64 date;
    QDateTime syncDate = QDateTime();
    QDateTime modifiedDate = QDateTime();
    QDateTime creationDate = QDateTime();

    SL3_step(stmt);

    *isDefault = false;
    if (rv == SQLITE_ROW) {
        QString id = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 0));
        QString name = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 1));
        QString description = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 2));
        QString color = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 3));
        int flags = (int)sqlite3_column_int(stmt, 4);
        date = sqlite3_column_int64(stmt, 5);
        QString plugin = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 6));
        QString account = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 7));
        int attachmentSize = sqlite3_column_int(stmt, 8);
        syncDate = fromOriginTime(date);
        date = sqlite3_column_int64(stmt, 9);
        modifiedDate = fromOriginTime(date);
        QString sharedWith = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 10));
        QString syncProfile = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 11));
        date = sqlite3_column_int64(stmt, 12);
        creationDate = fromOriginTime(date);

        notebook = new Notebook(name, description);
        notebook->setUid(id);
        notebook->setColor(color);
        notebook->setEventsAllowed(flags & SqliteFormat::AllowEvents);
        notebook->setTodosAllowed(flags & SqliteFormat::AllowTodos);
        notebook->setJournalsAllowed(flags & SqliteFormat::AllowJournals);
        notebook->setIsShared(flags & SqliteFormat::Shared);
        notebook->setIsMaster(flags & SqliteFormat::Master);
        notebook->setIsSynchronized(flags & SqliteFormat::Synchronized);
        notebook->setIsReadOnly(flags & SqliteFormat::ReadOnly);
        notebook->setIsVisible(flags & SqliteFormat::Visible);
        notebook->setRunTimeOnly(flags & SqliteFormat::RunTimeOnly);
        notebook->setIsShareable(flags & SqliteFormat::Shareable);
        notebook->setPluginName(plugin);
        notebook->setAccount(account);
        notebook->setAttachmentSize(attachmentSize);
        notebook->setSyncDate(syncDate);
        notebook->setSharedWithStr(sharedWith);
        notebook->setSyncProfile(syncProfile);
        notebook->setCreationDate(creationDate);

        if (!d->selectCalendarProperties(notebook)) {
            qCWarning(lcMkcal) << "failed to get calendarproperties for notebook" << id;
        }

        // This has to be called last! Otherwise the last modified date
        // will be roughly now, and not whenever notebook was really last
        // modified.
        notebook->setModifiedDate(modifiedDate);

        *isDefault = flags & SqliteFormat::Default;
    }

error:
    return notebook;
}

static QDateTime getDateTime(SqliteFormat *format, sqlite3_stmt *stmt, int index, bool *isDate = 0)
{
    sqlite3_int64 date;
    const QByteArray timezone((const char *)sqlite3_column_text(stmt, index + 2));
    QDateTime dateTime;

    if (timezone.isEmpty()) {
        // consider empty timezone as clock time
        date = sqlite3_column_int64(stmt, index + 1);
        if (date || sqlite3_column_int64(stmt, index)) {
            dateTime = format->fromOriginTime(date);
            dateTime.setTimeSpec(Qt::LocalTime);
        }
        if (isDate) {
            *isDate = false;
        }
    } else if (timezone == QStringLiteral(FLOATING_DATE)) {
        date = sqlite3_column_int64(stmt, index + 1);
        dateTime = format->fromOriginTime(date);
        dateTime.setTimeSpec(Qt::LocalTime);
        dateTime.setTime(QTime(0, 0, 0));
        if (isDate) {
            *isDate = dateTime.isValid();
        }
    } else {
        date = sqlite3_column_int64(stmt, index);
        dateTime = format->fromOriginTime(date, timezone);
        if (!dateTime.isValid()) {
            // timezone is specified but invalid?
            // fall back to local seconds from origin as clock time.
            date = sqlite3_column_int64(stmt, index + 1);
            dateTime = format->fromLocalOriginTime(date);
        }
        if (isDate) {
            *isDate = false;
        }
    }
    return dateTime;
}

Incidence::Ptr SqliteFormat::selectComponents(sqlite3_stmt *stmt1, QString &notebook)
{
    int rv = 0;
    int index = 0;
    Incidence::Ptr incidence;
    QString type;
    QString timezone;
    int rowid;

    SL3_step(stmt1);

    if (rv == SQLITE_ROW) {

        QByteArray type((const char *)sqlite3_column_text(stmt1, 2));
        if (type == "Event") {
            // Set Event specific data.
            Event::Ptr event(new Event());
            event->setAllDay(false);

            bool startIsDate;
            QDateTime start = getDateTime(this, stmt1, 5, &startIsDate);
            if (start.isValid()) {
                event->setDtStart(start);
            } else {
                // start date time is mandatory in RFC5545 for VEVENTS.
                event->setDtStart(fromOriginTime(0));
            }

            bool endIsDate;
            QDateTime end = getDateTime(this, stmt1, 9, &endIsDate);
            if (startIsDate && (!end.isValid() || endIsDate)) {
                event->setAllDay(true);
                // Keep backward compatibility with already saved events with end + 1.
                if (end.isValid()) {
                    end = end.addDays(-1);
                    if (end == start) {
                        end = QDateTime();
                    }
                }
            }
            if (end.isValid()) {
                event->setDtEnd(end);
            }
            incidence = event;
        } else if (type == "Todo") {
            // Set Todo specific data.
            Todo::Ptr todo(new Todo());
            todo->setAllDay(false);

            bool startIsDate;
            QDateTime start = getDateTime(this, stmt1, 5, &startIsDate);
            if (start.isValid()) {
                todo->setDtStart(start);
            }

            bool hasDueDate(sqlite3_column_int(stmt1, 8));
            bool dueIsDate;
            QDateTime due = getDateTime(this, stmt1, 9, &dueIsDate);
            if (due.isValid()) {
                if (start.isValid() && due == start && !hasDueDate) {
                    due = QDateTime();
                } else {
                    todo->setDtDue(due, true);
                }
            }

            if (startIsDate && (!due.isValid() || (dueIsDate && due > start))) {
                todo->setAllDay(true);
            }
            incidence = todo;
        } else if (type == "Journal") {
            // Set Journal specific data.
            Journal::Ptr journal(new Journal());

            bool startIsDate;
            QDateTime start = getDateTime(this, stmt1, 5, &startIsDate);
            journal->setDtStart(start);
            journal->setAllDay(startIsDate);
            incidence = journal;
        }

        if (!incidence) {
            return Incidence::Ptr();
        }

        // Set common Incidence data.
        rowid = sqlite3_column_int(stmt1, index++);

        notebook = QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++));

        index++;

        incidence->setSummary(QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        incidence->setCategories(QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        index++;
        index++;
        index++;
        index++;
        index++;
        index++;
        index++;

        int duration = sqlite3_column_int(stmt1, index++);
        if (duration != 0) {
            incidence->setDuration(Duration(duration, Duration::Seconds));
        }
        incidence->setSecrecy(
            (Incidence::Secrecy)sqlite3_column_int(stmt1, index++));

        incidence->setLocation(
            QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        incidence->setDescription(
            QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));

        incidence->setStatus(
            (Incidence::Status)sqlite3_column_int(stmt1, index++));

        incidence->setGeoLatitude(sqlite3_column_double(stmt1, index++));
        incidence->setGeoLongitude(sqlite3_column_double(stmt1, index++));
        if (incidence->geoLatitude() != INVALID_LATLON) {
            incidence->setHasGeo(true);
        }

        incidence->setPriority(sqlite3_column_int(stmt1, index++));

        QString Resources = QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++));
        incidence->setResources(Resources.split(' '));

        incidence->setCreated(fromOriginTime(
                                  sqlite3_column_int64(stmt1, index++)));

        QDateTime dtstamp = fromOriginTime(sqlite3_column_int64(stmt1, index++));

        incidence->setLastModified(
            fromOriginTime(sqlite3_column_int64(stmt1, index++)));

        incidence->setRevision(sqlite3_column_int(stmt1, index++));

        QString Comment = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));
        if (!Comment.isEmpty()) {
            QStringList CommL = Comment.split(' ');
            for (QStringList::Iterator it = CommL.begin(); it != CommL.end(); ++it) {
                incidence->addComment(*it);
            }
        }

        // Old way to store attachment, deprecated.
        QString Att = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));

        incidence->addContact(
            QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++)));

        //Invitation status (removed but still on DB)
        ++index;

        QDateTime rid = getDateTime(this, stmt1, index);
        if (rid.isValid()) {
            incidence->setRecurrenceId(rid);
        } else {
            incidence->setRecurrenceId(QDateTime());
        }
        index += 3;

        QString relatedtouid = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));
        incidence->setRelatedTo(relatedtouid);

        QUrl url(QString::fromUtf8((const char *)sqlite3_column_text(stmt1, index++)));
        if (url.isValid()) {
            incidence->setUrl(url);
        }

        // set the real uid to uid
        incidence->setUid(QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++)));

        if (incidence->type() == Incidence::TypeEvent) {
            Event::Ptr event = incidence.staticCast<Event>();
            int transparency = sqlite3_column_int(stmt1, index);
            event->setTransparency((Event::Transparency) transparency);
        }

        index++;

        incidence->setLocalOnly(sqlite3_column_int(stmt1, index++)); //LocalOnly

        if (incidence->type() == Incidence::TypeTodo) {
            Todo::Ptr todo = incidence.staticCast<Todo>();
            todo->setPercentComplete(sqlite3_column_int(stmt1, index++));
            QDateTime completed = getDateTime(this, stmt1, index);
            if (completed.isValid())
                todo->setCompleted(completed);
            index += 3;
        } else {
            index += 4;
        }

        index++; //DateDeleted

        QString colorstr = QString::fromUtf8((const char *) sqlite3_column_text(stmt1, index++));
        if (!colorstr.isEmpty()) {
            incidence->setColor(colorstr);
        }
//    kDebug() << "loaded component for incidence" << incidence->uid() << "notebook" << notebook;

        if (!d->selectCustomproperties(incidence.data(), rowid)) {
            qCWarning(lcMkcal) << "failed to get customproperties for incidence" << incidence->uid();
        }
        if (!d->selectAttendees(incidence.data(), rowid)) {
            qCWarning(lcMkcal) << "failed to get attendees for incidence" << incidence->uid();
        }
        if (!d->selectAlarms(incidence.data(), rowid)) {
            qCWarning(lcMkcal) << "failed to get alarms for incidence" << incidence->uid();
        }
        if (!d->selectRecursives(incidence.data(), rowid)) {
            qCWarning(lcMkcal) << "failed to get recursive for incidence" << incidence->uid();
        }
        if (!d->selectRdates(incidence.data(), rowid)) {
            qCWarning(lcMkcal) << "failed to get rdates for incidence" << incidence->uid();
        }
        if (!d->selectAttachments(incidence.data(), rowid)) {
            qCWarning(lcMkcal) << "failed to get attachments for incidence" << incidence->uid();
        }

        // Backward compatibility with the old attachment storage.
        if (!Att.isEmpty() && incidence->attachments().isEmpty()) {
            QStringList AttL = Att.split(' ');
            for (QStringList::Iterator it = AttL.begin(); it != AttL.end(); ++it) {
                incidence->addAttachment(Attachment(*it));
            }
        }
    }

error:
    return incidence;
}

//@cond PRIVATE
int SqliteFormat::Private::selectRowId(const Incidence &incidence)
{
    int rv = 0;
    int index = 1;
    const char *query = NULL;
    int qsize = 0;
    sqlite3_stmt *stmt = NULL;

    QByteArray u;
    qint64 secsRecurId;
    int rowid = 0;

    query = SELECT_ROWID_FROM_COMPONENTS_BY_UID_AND_RECURID;
    qsize = sizeof(SELECT_ROWID_FROM_COMPONENTS_BY_UID_AND_RECURID);

    SL3_prepare_v2(mDatabase, query, qsize, &stmt, NULL);
    u = incidence.uid().toUtf8();
    SL3_bind_text(stmt, index, u.constData(), u.length(), SQLITE_STATIC);
    if (incidence.recurrenceId().isValid()) {
        if (incidence.recurrenceId().timeSpec() == Qt::LocalTime) {
            secsRecurId = mFormat->toLocalOriginTime(incidence.recurrenceId());
        } else {
            secsRecurId = mFormat->toOriginTime(incidence.recurrenceId());
        }
        SL3_bind_int64(stmt, index, secsRecurId);
    } else {
        SL3_bind_int64(stmt, index, 0);
    }

    SL3_step(stmt);

    if (rv == SQLITE_ROW) {
        rowid = sqlite3_column_int(stmt, 0);
    }

error:
    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    return rowid;
}

bool SqliteFormat::Private::selectCustomproperties(Incidence *incidence, int rowid)
{
    int rv = 0;
    int index = 1;

    if (!mSelectIncProperties) {
        const char *query = SELECT_CUSTOMPROPERTIES_BY_ID;
        int qsize = sizeof(SELECT_CUSTOMPROPERTIES_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectIncProperties, nullptr);
    }

    SL3_reset(mSelectIncProperties);
    SL3_bind_int(mSelectIncProperties, index, rowid);
    do {
        SL3_step(mSelectIncProperties);

        if (rv == SQLITE_ROW) {
            // Set Incidence data customproperties
            const QByteArray &name = (const char *)sqlite3_column_text(mSelectIncProperties, 1);
            const QString &value = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncProperties, 2));
            const QString &parameters = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncProperties, 3));
            incidence->setNonKDECustomProperty(name, value, parameters);
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectRdates(Incidence *incidence, int rowid)
{
    int rv = 0;
    int index = 1;
    QString   timezone;
    QDateTime kdt;

    if (!mSelectIncRDates) {
        const char *query = SELECT_RDATES_BY_ID;
        int qsize = sizeof(SELECT_RDATES_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectIncRDates, nullptr);
    }

    SL3_reset(mSelectIncRDates);
    SL3_bind_int(mSelectIncRDates, index, rowid);
    do {
        SL3_step(mSelectIncRDates);

        if (rv == SQLITE_ROW) {
            // Set Incidence data rdates
            int type = sqlite3_column_int(mSelectIncRDates, 1);
            kdt = getDateTime(mFormat, mSelectIncRDates, 2);
            if (kdt.isValid()) {
                if (type == SqliteFormat::RDate || type == SqliteFormat::XDate) {
                    if (type == SqliteFormat::RDate)
                        incidence->recurrence()->addRDate(kdt.date());
                    else
                        incidence->recurrence()->addExDate(kdt.date());
                } else {
                    if (type == SqliteFormat::RDateTime)
                        incidence->recurrence()->addRDateTime(kdt);
                    else
                        incidence->recurrence()->addExDateTime(kdt);
                }
            }
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectRecursives(Incidence *incidence, int rowid)
{
    int  rv = 0;
    int  index = 1;
    QString   timezone;
    QDateTime kdt;
    QDateTime dt;

    if (!mSelectIncRecursives) {
        const char *query = SELECT_RECURSIVE_BY_ID;
        int qsize = sizeof(SELECT_RECURSIVE_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectIncRecursives, nullptr);
    }

    SL3_reset(mSelectIncRecursives);
    SL3_bind_int(mSelectIncRecursives, index, rowid);
    do {
        SL3_step(mSelectIncRecursives);

        if (rv == SQLITE_ROW) {
            // Set Incidence data from recursive

            // all BY*
            QList<int> byList;
            QList<int> byList2;
            QStringList byL;
            QStringList byL2;
            QString by;
            QString by2;
            RecurrenceRule *recurrule = new RecurrenceRule();

            if (incidence->dtStart().isValid())
                recurrule->setStartDt(incidence->dtStart());
            else {
                if (incidence->type() == Incidence::TypeTodo) {
                    Todo *todo = static_cast<Todo*>(incidence);
                    recurrule->setStartDt(todo->dtDue(true));
                }
            }

            // Generate the RRULE string
            if (sqlite3_column_int(mSelectIncRecursives, 1) == 1)   // ruletype
                recurrule->setRRule(QString("RRULE"));
            else
                recurrule->setRRule(QString("EXRULE"));

            switch (sqlite3_column_int(mSelectIncRecursives, 2)) {    // frequency
            case 1:
                recurrule->setRecurrenceType(RecurrenceRule::rSecondly);
                break;
            case 2:
                recurrule->setRecurrenceType(RecurrenceRule::rMinutely);
                break;
            case 3:
                recurrule->setRecurrenceType(RecurrenceRule::rHourly);
                break;
            case 4:
                recurrule->setRecurrenceType(RecurrenceRule::rDaily);
                break;
            case 5:
                recurrule->setRecurrenceType(RecurrenceRule::rWeekly);
                break;
            case 6:
                recurrule->setRecurrenceType(RecurrenceRule::rMonthly);
                break;
            case 7:
                recurrule->setRecurrenceType(RecurrenceRule::rYearly);
                break;
            default:
                recurrule->setRecurrenceType(RecurrenceRule::rNone);
            }

            // Duration & End Date
            bool isAllDay;
            QDateTime until = getDateTime(mFormat, mSelectIncRecursives, 3, &isAllDay);
            recurrule->setEndDt(until);
            incidence->recurrence()->setAllDay(until.isValid() ? isAllDay : incidence->allDay());

            int duration = sqlite3_column_int(mSelectIncRecursives, 6);  // count
            if (duration == 0 && !recurrule->endDt().isValid()) {
                duration = -1; // work around invalid recurrence state: recurring infinitely but having invalid end date
            } else if (duration > 0) {
                // Ensure that no endDt is saved if duration is provided.
                // This guarantees that the operator== returns true for
                // rRule(withDuration) == savedRRule(withDuration)
                recurrule->setEndDt(QDateTime());
            }
            recurrule->setDuration(duration);
            // Frequency
            recurrule->setFrequency(sqlite3_column_int(mSelectIncRecursives, 7)); // interval-field


#define readSetByList( field, setfunc )                 \
      by = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncRecursives, field)); \
      if (!by.isEmpty()) {                      \
        byList.clear();                         \
        byL = by.split(' ');                        \
        for ( QStringList::Iterator it = byL.begin(); it != byL.end(); ++it ) \
          byList.append((*it).toInt());                 \
        if ( !byList.isEmpty() )                    \
          recurrule->setfunc(byList);                   \
      }

            // BYSECOND, MINUTE and HOUR, MONTHDAY, YEARDAY, WEEKNUMBER, MONTH
            // and SETPOS are standard int lists, so we can treat them with the
            // same macro
            readSetByList(8, setBySeconds);
            readSetByList(9, setByMinutes);
            readSetByList(10, setByHours);
            readSetByList(13, setByMonthDays);
            readSetByList(14, setByYearDays);
            readSetByList(15, setByWeekNumbers);
            readSetByList(16, setByMonths);
            readSetByList(17, setBySetPos);

#undef readSetByList

            // BYDAY is a special case, since it's not an int list
            QList<RecurrenceRule::WDayPos> wdList;
            RecurrenceRule::WDayPos pos;
            wdList.clear();
            byList.clear();
            by = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncRecursives, 11));
            by2 = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncRecursives, 12));
            if (!by.isEmpty()) {
                byL = by.split(' ');
                if (!by2.isEmpty())
                    byL2 = by2.split(' ');
                for (int i = 0; i < byL.size(); ++i) {
                    if (!by2.isEmpty()) {
                        pos.setDay(byL.at(i).toInt());
                        pos.setPos(byL2.at(i).toInt());
                        wdList.append(pos);
                    } else {
                        pos.setDay(byL.at(i).toInt());
                        wdList.append(pos);
                    }
                }
                if (!wdList.isEmpty())
                    recurrule->setByDays(wdList);
            }

            // Week start setting
            recurrule->setWeekStart(sqlite3_column_int(mSelectIncRecursives, 18));

            if (recurrule->rrule() == "RRULE")
                incidence->recurrence()->addRRule(recurrule);
            else
                incidence->recurrence()->addExRule(recurrule);
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectAlarms(Incidence *incidence, int rowid)
{
    int rv = 0;
    int index = 1;
    int offset;
    QString   timezone;
    QDateTime kdt;
    QDateTime dt;

    if (!mSelectIncAlarms) {
        const char *query = SELECT_ALARM_BY_ID;
        int qsize = sizeof(SELECT_ALARM_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectIncAlarms, nullptr);
    }

    SL3_reset(mSelectIncAlarms);
    SL3_bind_int(mSelectIncAlarms, index, rowid);
    do {
        SL3_step(mSelectIncAlarms);

        if (rv == SQLITE_ROW) {
            // Set Incidence data from alarm

            Alarm::Ptr ialarm = incidence->newAlarm();

            // Determine the alarm's action type
            int action = sqlite3_column_int(mSelectIncAlarms, 1);
            Alarm::Type type = Alarm::Invalid;

            switch (action) {
            case 1: //ICAL_ACTION_DISPLAY
                type = Alarm::Display;
                break;
            case 2: //ICAL_ACTION_PROCEDURE
                type = Alarm::Procedure;
                break;
            case 3: //ICAL_ACTION_EMAIL
                type = Alarm::Email;
                break;
            case 4: //ICAL_ACTION_AUDIO
                type = Alarm::Audio;
                break;
            default:
                break;
            }

            ialarm->setType(type);

            if (sqlite3_column_int(mSelectIncAlarms, 2) > 0)
                ialarm->setRepeatCount(sqlite3_column_int(mSelectIncAlarms, 2));
            if (sqlite3_column_int(mSelectIncAlarms, 3) > 0)
                ialarm->setSnoozeTime(Duration(sqlite3_column_int(mSelectIncAlarms, 3), Duration::Seconds));

            offset = sqlite3_column_int(mSelectIncAlarms, 4);
            QString relation = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAlarms, 5));

            kdt = getDateTime(mFormat, mSelectIncAlarms, 6);
            if (kdt.isValid())
                ialarm->setTime(kdt);

            if (!ialarm->hasTime()) {
                if (relation.contains("startTriggerRelation")) {
                    ialarm->setStartOffset(Duration(offset, Duration::Seconds));
                } else if (relation.contains("endTriggerRelation")) {
                    ialarm->setEndOffset(Duration(offset, Duration::Seconds));
                }
            }

            const QString &description =  QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAlarms, 9));
            const QString &attachments =  QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAlarms, 10));
            const QString &summary = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAlarms, 11));
            const QString &addresses = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAlarms, 12));

            switch (ialarm->type()) {
            case Alarm::Display:
                ialarm->setText(description);
                break;
            case Alarm::Procedure:
                ialarm->setProgramFile(attachments);
                ialarm->setProgramArguments(description);
                break;
            case Alarm::Email:
                ialarm->setMailSubject(summary);
                ialarm->setMailText(description);
                if (!attachments.isEmpty())
                    ialarm->setMailAttachments(attachments.split(','));
                if (!addresses.isEmpty()) {
                    Person::List persons;
                    QStringList emails = addresses.split(',');
                    for (int i = 0; i < emails.size(); i++) {
                        persons.append(Person(QString(), emails.at(i)));
                    }
                    ialarm->setMailAddresses(persons);
                }
                break;
            case Alarm::Audio:
                ialarm->setAudioFile(attachments);
                break;
            default:
                break;
            }

            const QString &properties = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAlarms, 13));
            if (!properties.isEmpty()) {
                QMap<QByteArray, QString> customProperties;
                QStringList list = properties.split("\r\n");
                for (int i = 0; i < list.size(); i += 2) {
                    QByteArray key;
                    QString value;
                    key = list.at(i).toUtf8();
                    if ((i + 1) < list.size()) {
                        value = list.at(i + 1);
                        customProperties[key] = value;
                    }
                }
                ialarm->setCustomProperties(customProperties);
                QString locationRadius = ialarm->nonKDECustomProperty("X-LOCATION-RADIUS");
                if (!locationRadius.isEmpty()) {
                    ialarm->setLocationRadius(locationRadius.toInt());
                    ialarm->setHasLocationRadius(true);
                }
            }

            ialarm->setEnabled((bool)sqlite3_column_int(mSelectIncAlarms, 14));
        }

    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectAttendees(Incidence *incidence, int rowid)
{
    int rv = 0;
    int index = 1;

    if (!mSelectIncAttendees) {
        const char *query = SELECT_ATTENDEE_BY_ID;
        int qsize = sizeof(SELECT_ATTENDEE_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectIncAttendees, nullptr);
    }

    SL3_reset(mSelectIncAttendees);
    SL3_bind_int(mSelectIncAttendees, index, rowid);
    do {
        SL3_step(mSelectIncAttendees);

        if (rv == SQLITE_ROW) {
            const QString &email = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAttendees, 1));
            const QString &name = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAttendees, 2));
            bool isOrganizer = (bool)sqlite3_column_int(mSelectIncAttendees, 3);
            Attendee::Role role = (Attendee::Role)sqlite3_column_int(mSelectIncAttendees, 4);
            Attendee::PartStat status = (Attendee::PartStat)sqlite3_column_int(mSelectIncAttendees, 5);
            bool rsvp = (bool)sqlite3_column_int(mSelectIncAttendees, 6);
            if (isOrganizer) {
                incidence->setOrganizer(Person(name, email));
            }
            Attendee attendee(name, email, rsvp, status, role);
            attendee.setDelegate(QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAttendees, 7)));
            attendee.setDelegator(QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAttendees, 8)));
            incidence->addAttendee(attendee, false);
        }
    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

bool SqliteFormat::Private::selectAttachments(Incidence *incidence, int rowid)
{
    int rv = 0;
    int index = 1;

    if (!mSelectIncAttachments) {
        const char *query = SELECT_ATTACHMENTS_BY_ID;
        int qsize = sizeof(SELECT_ATTACHMENTS_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectIncAttachments, nullptr);
    }

    SL3_reset(mSelectIncAttachments);
    SL3_bind_int(mSelectIncAttachments, index, rowid);
    do {
        SL3_step(mSelectIncAttachments);

        if (rv == SQLITE_ROW) {
            Attachment attach;

            QByteArray data = QByteArray((const char *)sqlite3_column_blob(mSelectIncAttachments, 1),
                                         sqlite3_column_bytes(mSelectIncAttachments, 1));
            if (!data.isEmpty()) {
                attach.setDecodedData(data);
            } else {
                QString uri = QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAttachments, 2));
                if (!uri.isEmpty()) {
                    attach.setUri(uri);
                }
            }
            if (!attach.isEmpty()) {
                attach.setMimeType(QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAttachments, 3)));
                attach.setShowInline(sqlite3_column_int(mSelectIncAttachments, 4) != 0);
                attach.setLabel(QString::fromUtf8((const char *)sqlite3_column_text(mSelectIncAttachments, 5)));
                attach.setLocal(sqlite3_column_int(mSelectIncAttachments, 6) != 0);
                incidence->addAttachment(attach);
            } else {
                qCWarning(lcMkcal) << "Empty attachment for incidence" << incidence->instanceIdentifier();
            }
        }
    } while (rv != SQLITE_DONE);

    return true;

error:
    return false;
}

Person::List SqliteFormat::selectContacts()
{
    int rv = 0;
    Person::List list;
    QHash<QString, Person> hash;

    const char *query1 = SELECT_ATTENDEE_AND_COUNT;
    int qsize1 = sizeof(SELECT_ATTENDEE_AND_COUNT);

    sqlite3_stmt *stmt = NULL;

    SL3_prepare_v2(d->mDatabase, query1, qsize1, &stmt, NULL);

    do {
        SL3_step(stmt);

        if (rv == SQLITE_ROW) {
            QString name = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 1));
            QString email = QString::fromUtf8((const char *)sqlite3_column_text(stmt, 0));

            hash.insert(email, Person(name, email));
        }
    } while (rv != SQLITE_DONE);

    sqlite3_finalize(stmt);

    list = hash.values().toVector();

error:
    return list;
}

bool SqliteFormat::Private::selectCalendarProperties(Notebook *notebook)
{
    int rv = 0;
    int index = 1;
    const QByteArray id(notebook->uid().toUtf8());
    bool success = false;

    if (!mSelectCalProps) {
        const char *query = SELECT_CALENDARPROPERTIES_BY_ID;
        int qsize = sizeof(SELECT_CALENDARPROPERTIES_BY_ID);
        SL3_prepare_v2(mDatabase, query, qsize, &mSelectCalProps, NULL);
    }

    SL3_bind_text(mSelectCalProps, index, id.constData(), id.length(), SQLITE_STATIC);
    do {
        SL3_step(mSelectCalProps);
        if (rv == SQLITE_ROW) {
            const QByteArray name = (const char *)sqlite3_column_text(mSelectCalProps, 1);
            const QString value = QString::fromUtf8((const char *)sqlite3_column_text(mSelectCalProps, 2));
            notebook->setCustomProperty(name, value);
        }
    } while (rv != SQLITE_DONE);
    success = true;

error:
    sqlite3_reset(mSelectCalProps);

    return success;
}

sqlite3_int64 SqliteFormat::toOriginTime(const QDateTime &dt)
{
    return dt.toMSecsSinceEpoch() / 1000;
}

sqlite3_int64 SqliteFormat::toLocalOriginTime(const QDateTime &dt)
{
    return toOriginTime(QDateTime(dt.date(), dt.time(), Qt::UTC));
}

QDateTime SqliteFormat::fromLocalOriginTime(sqlite3_int64 seconds)
{
    // Note: don't call toClockTime() as that implies a conversion first to the local time zone.
    const QDateTime local = fromOriginTime(seconds);
    return QDateTime(local.date(), local.time(), Qt::LocalTime);
}

QDateTime SqliteFormat::fromOriginTime(sqlite3_int64 seconds)
{
    //qCDebug(lcMkcal) << "fromOriginTime" << seconds << d->mOriginTime.addSecs( seconds ).toUtc();
    return QDateTime::fromMSecsSinceEpoch(seconds * 1000, Qt::UTC);
}

QDateTime SqliteFormat::fromOriginTime(sqlite3_int64 seconds, const QByteArray &zonename)
{
    QDateTime dt;

    if (zonename == "UTC") {
        dt = fromOriginTime(seconds);
    } else if (!zonename.isEmpty()) {
        // First try system zones.
        const QTimeZone timezone(zonename);
        if (timezone.isValid()) {
            dt = fromOriginTime(seconds).toTimeZone(timezone);
        } else if (d->mTimeZone.isValid() && d->mTimeZone.id() == zonename) {
            dt = fromOriginTime(seconds).toTimeZone(d->mTimeZone);
        }
    } else {
        // Empty zonename, use floating time.
        dt = fromOriginTime(seconds);
        dt.setTimeSpec(Qt::LocalTime);
    }
//  qCDebug(lcMkcal) << "fromOriginTime" << seconds << zonename << dt;
    return dt;
}
