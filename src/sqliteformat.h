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

namespace mKCal {

class SqliteStorage;

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
      Constructor a new Sqlite Format object.
    */
    SqliteFormat(SqliteStorage *storage, sqlite3 *database);

    /**
      Destructor.
    */
    virtual ~SqliteFormat();

    /**
      Update notebook data in Calendars table.

      @param notebook notebook to update
      @param dbop database operation
      @param stmt prepared sqlite statement for calendars table
      @return true if the operation was successful; false otherwise.
    */
    bool modifyCalendars(const Notebook::Ptr &notebook, DBOperation dbop, sqlite3_stmt *stmt);

    /**
      Select notebooks from Calendars table.

      @param stmt prepared sqlite statement for calendars table
      @return the queried notebook.
    */
    Notebook::Ptr selectCalendars(sqlite3_stmt *stmt);

    /**
      Update incidence data in Components table.

      @param incidence incidence to update
      @param notebook notebook of incidence
      @param dbop database operation
      @param stmt1 prepared sqlite statement for components table
      @param stmt2 prepared sqlite statement for customproperties table
      @param stmt3 prepared sqlite statement for attendee table
      @param stmt4 prepared sqlite statement for alarm table
      @return true if the operation was successful; false otherwise.
    */
    bool modifyComponents(const KCalendarCore::Incidence::Ptr &incidence, const QString &notebook,
                          DBOperation dbop, sqlite3_stmt *stmt1, sqlite3_stmt *stmt2,
                          sqlite3_stmt *stmt3, sqlite3_stmt *stmt4, sqlite3_stmt *stmt5,
                          sqlite3_stmt *stmt6, sqlite3_stmt *stmt7, sqlite3_stmt *stmt8,
                          sqlite3_stmt *stmt9, sqlite3_stmt *stmt10, sqlite3_stmt *stmt11);

    bool purgeDeletedComponents(const KCalendarCore::Incidence::Ptr &incidence,
                                sqlite3_stmt *stmt1, sqlite3_stmt *stmt2,
                                sqlite3_stmt *stmt3, sqlite3_stmt *stmt4,
                                sqlite3_stmt *stmt5, sqlite3_stmt *stmt6,
                                sqlite3_stmt *stmt7);

    /**
      Select incidences from Components  table.

      @param stmt1 prepared sqlite statement for components table
      @param stmt2 prepared sqlite statement for customproperties table
      @param stmt3 prepared sqlite statement for attendee table
      @param stmt4 prepared sqlite statement for alarm table
      @param stmt5 prepared sqlite statement for recursive table
      @param notebook notebook of incidence
      @return the queried incidence.
    */
    KCalendarCore::Incidence::Ptr selectComponents(sqlite3_stmt *stmt1, sqlite3_stmt *stmt2,
                                              sqlite3_stmt *stmt3, sqlite3_stmt *stmt4,
                                              sqlite3_stmt *stmt5, sqlite3_stmt *stmt6,
                                              QString &notebook);

    /**
      Select contacts and order them by appearances.

      @param stmt prepared sqlite statement for Attendees table
      @return ordered list of contacts.
    */
    KCalendarCore::Person::List selectContacts(sqlite3_stmt *stmt);

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteFormat)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond
};

}

#endif
