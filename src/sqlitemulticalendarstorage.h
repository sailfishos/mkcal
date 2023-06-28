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
  This file implements MultiCalendarStorage on top of
  a SQLite database.

  @author Damien Caliste \<dcaliste@free.fr\>
*/

#ifndef MKCAL_SQLITEMULTICALENDARSTORAGE_H
#define MKCAL_SQLITEMULTICALENDARSTORAGE_H

#include "multicalendarstorage.h"

namespace mKCal {

/**
  @brief
  This class implements an SQLite storage and provides
  multi-notebook access to it.
*/
class Q_DECL_EXPORT SqliteMultiCalendarStorage
    : public MultiCalendarStorage
{
    Q_OBJECT

public:

    typedef QSharedPointer<SqliteMultiCalendarStorage> Ptr;

    /**
      Constructs a new SqliteMultiCalendarStorage object.

      @param timezone defines the time zone where to expand the incidences
             in local time to.
      @param database, a filepath to read or store the database into.
    */
    explicit SqliteMultiCalendarStorage(const QTimeZone &timezone = QTimeZone::systemTimeZone(),
                                        const QString &databaseName = QString());

    virtual ~SqliteMultiCalendarStorage();

    /**
      @copydoc
      MultiCalendarStorage::open()
    */
    virtual bool open() override;

    /**
      @copydoc
      MultiCalendarStorage::close()
    */
    virtual bool close() override;

    /**
      @copydoc
      MultiCalendarStorage::load(const QDate &, const QDate &)
    */
    virtual bool load(const QDate &start, const QDate &end) override;

    /**
      @copydoc
      MultiCalendarStorage::search(const QString &, QStringList*, int)
    */
    virtual bool search(const QString &key, QStringList *identifiers, int limit = 0) override;

    using MultiCalendarStorage::save;

protected:
    virtual Notebook::List loadedNotebooks(QString *defaultUid) const override;

    virtual bool save(const QString &notebookUid,
                      const QHash<QString, KCalendarCore::Incidence::List> &added,
                      const QHash<QString, KCalendarCore::Incidence::List> &modified,
                      const QHash<QString, KCalendarCore::Incidence::List> &deleted,
                      DeleteAction deleteAction) override;

    virtual KCalendarCore::Incidence::List incidences(const QString &notebookUid,
                                                      const QString &uid) override;

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteMultiCalendarStorage)
    class Private;
    Private *const d;
    //@endcond

public slots:
    void onModified();
    void onUpdated(const QHash<QString, QStringList> &added,
                   const QHash<QString, QStringList> &modified,
                   const QHash<QString, QStringList> &deleted);
};

}

#endif
