/*
  This file is part of the mkcal library.

  Copyright (c) 2023 Damien Caliste <dcaliste@free.fr>

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
  This file is providing an implementation of the CalendarStorage
  API, based on an SQLite database.

  @author Damien Caliste \<dcaliste@free.fr\>
*/

#ifndef MKCAL_SQLITECALENDARSTORAGE_H
#define MKCAL_SQLITECALENDARSTORAGE_H

#include "calendarstorage.h"

namespace mKCal {

/**
  @brief
  This class provides an SQLite implementation of CalendarStorage API.
*/
class Q_DECL_EXPORT SqliteCalendarStorage
    : public CalendarStorage
{
    Q_OBJECT

public:

    typedef QSharedPointer<SqliteCalendarStorage> Ptr;

    /**
      Constructs a new SqliteCalendarStorage object.

      @param cal is a pointer to a valid Calendar object.
      @param database, a filepath to read or store the database into.
    */
    explicit SqliteCalendarStorage(const KCalendarCore::MemoryCalendar::Ptr &cal,
                                   const QString &databaseName = QString());

    /**
      Constructs a new SqliteCalendarStorage object with a default
      KCalendarCore::MemoryCalendar.

      @param uid defines a identifier for this calendar.
      @param database, a filepath to read or store the database into.
    */
    explicit SqliteCalendarStorage(const QString &uid,
                                   const QString &databaseName = QString());

    virtual ~SqliteCalendarStorage();

    /**
      @copydoc
      CalStorage::open()
    */
    virtual bool open() override;

    /**
      @copydoc
      CalStorage::close()
    */
    virtual bool close() override;

    /**
      @copydoc
      CalStorage::load()
    */
    virtual bool load() override;

    /**
      @copydoc
      CalendarStorage::load(const QString &)
    */
    virtual bool load(const QString &uid) override;

    using CalendarStorage::save;

    /**
      @copydoc
      CalendarStorage::deletedIncidences(KCalendarCore::Incidence::List *, const QDateTime &)
    */
    virtual bool deletedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after = QDateTime()) override;

    /**
      @copydoc
      CalendarStorage::purgeDeletedIncidences(KCalendarCore::Incidence::List &)
    */
    virtual bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list) override;

    /**
      @copydoc
      CalendarStorage::insertedIncidences(KCalendarCore::Incidence::List *, const QDateTime &)
    */
    virtual bool insertedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after) override;

    /**
      @copydoc
      CalendarStorage::modifiedIncidences(KCalendarCore::Incidence::List *, const QDateTime &)
    */
    virtual bool modifiedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after) override;

protected:
    virtual Notebook::Ptr loadedNotebook() const override;

    virtual bool save(const KCalendarCore::Incidence::List &added,
                      const KCalendarCore::Incidence::List &modified,
                      const KCalendarCore::Incidence::List &deleted,
                      DeleteAction deleteAction) override;

    virtual KCalendarCore::Incidence::List incidences(const QString &uid = QString()) override;

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteCalendarStorage)
    class Private;
    Private *const d;
    //@endcond

public Q_SLOTS:
    void onModified();
    void onUpdated(const QHash<QString, QStringList> &added,
                   const QHash<QString, QStringList> &modified,
                   const QHash<QString, QStringList> &deleted);
};

}

#endif
