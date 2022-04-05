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
  defines the SqliteStorage class.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
  @author Pertti Luukko \<ext-pertti.luukko@nokia.com\>
  @author Alvaro Manera \<alvaro.manera@nokia.com \>
*/

#ifndef MKCAL_SQLITESTORAGE_H
#define MKCAL_SQLITESTORAGE_H

#include "mkcal_export.h"
#include "storagebackend.h"

namespace mKCal {

/**
  @brief
  This class provides a calendar storage as an sqlite database.

  @warning When saving Attendees, the CustomProperties are not saved.
*/
class MKCAL_EXPORT SqliteStorage : public StorageBackend
{
    Q_OBJECT

public:

    /**
      A shared pointer to a SqliteStorage
    */
    typedef QSharedPointer<SqliteStorage> Ptr;

    /**
      Constructs a new SqliteStorage object with
      storage to file @p databaseName.

      @param timeZone is time zone definition.
      @param databaseName is the name of the database containing the Calendar data.
    */
    explicit SqliteStorage(const QTimeZone &timeZone,
                           const QString &databaseName);

    /**
      Constructs a new SqliteStorage object. Location
      of the database is using default location, or is taken from SQLITESTORAGEDB
      enivronment variable.

      @param timeZone is time zone definition.
    */
    explicit SqliteStorage(const QTimeZone &timeZone);

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
      StorageBackend::open()
    */
    bool open();

    /**
      @copydoc
      StorageBackend::load()
    */
    bool load();

    /**
      @copydoc
      StorageBackend::load(const QString &, const QDateTime &)
    */
    bool load(const QString &uid, const QDateTime &recurrenceId = QDateTime());

    /**
      @copydoc
      StorageBackend::load(const QDate &)
    */
    bool load(const QDate &date);

    /**
      @copydoc
      StorageBackend::load(const QDate &, const QDate &)
    */
    bool load(const QDate &start, const QDate &end);

    /**
      @copydoc
      StorageBackend::loadSeries(const QString &)
    */
    bool loadSeries(const QString &uid);

    /**
      @copydoc
      StorageBackend::loadIncidenceInstance(const QString &)
    */
    bool loadIncidenceInstance(const QString &instanceIdentifier);

    /**
      @copydoc
      StorageBackend::loadNotebookIncidences(const QString &)
    */
    bool loadNotebookIncidences(const QString &notebookUid);

    /**
      @copydoc
      StorageBackend::loadJournals()
    */
    bool loadJournals();

    /**
      @copydoc
      StorageBackend::loadPlainIncidences()
    */
    bool loadPlainIncidences();

    /**
      @copydoc
      StorageBackend::loadRecurringIncidences()
    */
    bool loadRecurringIncidences();

    /**
      @copydoc
      StorageBackend::loadGeoIncidences()
    */
    bool loadGeoIncidences();

    /**
      @copydoc
      StorageBackend::loadGeoIncidences(float, float, float, float)
    */
    bool loadGeoIncidences(float geoLatitude, float geoLongitude,
                           float diffLatitude, float diffLongitude);

    /**
      @copydoc
      StorageBackend::loadAttendeeIncidences()
    */
    bool loadAttendeeIncidences();

    /**
      @copydoc
      StorageBackend::loadUncompletedTodos()
    */
    int loadUncompletedTodos();

    /**
      @copydoc
      StorageBackend::loadCompletedTodos()
    */
    int loadCompletedTodos(bool hasDate, int limit, QDateTime *last);

    /**
      @copydoc
      StorageBackend::loadIncidences( bool, bool, int, QDateTime* );
    */
    int loadIncidences(bool hasDate, int limit, QDateTime *last);

    /**
      @copydoc
      StorageBackend::loadFutureIncidences( bool, int, QDateTime* );
    */
    int loadFutureIncidences(int limit, QDateTime *last);

    /**
      @copydoc
      StorageBackend::loadGeoIncidences( bool, bool, int, QDateTime* );
    */
    int loadGeoIncidences(bool hasDate, int limit, QDateTime *last);

    /**
      @copydoc
      StorageBackend::loadContacts()
    */
    KCalendarCore::Person::List loadContacts();

    /**
      @copydoc
      StorageBackend::loadContactIncidences( const KCalendarCore::Person & )
    */
    int loadContactIncidences(const KCalendarCore::Person &person, int limit, QDateTime *last);

    /**
      @copydoc
      StorageBackend::loadJournals()
    */
    int loadJournals(int limit, QDateTime *last);

    /**
      @copydoc
      StorageBackend::purgeDeletedIncidences(const KCalCore::Incidence::List &)
    */
    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list);

    /**
      @copydoc
      StorageBackend::close()
    */
    bool close();

    /**
      @copydoc
      StorageBackend::insertedIncidences()
    */
    bool insertedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      StorageBackend::modifiedIncidences()
    */
    bool modifiedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      StorageBackend::deletedIncidences()
    */
    bool deletedIncidences(KCalendarCore::Incidence::List *list,
                           const QDateTime &after = QDateTime(),
                           const QString &notebookUid = QString());

    /**
      @copydoc
      StorageBackend::allIncidences()
    */
    bool allIncidences(KCalendarCore::Incidence::List *list, const QString &notebookUid = QString());

    /**
      @copydoc
      StorageBackend::duplicateIncidences()
    */
    bool duplicateIncidences(KCalendarCore::Incidence::List *list,
                             const KCalendarCore::Incidence::Ptr &incidence,
                             const QString &notebookUid = QString());

    /**
      @copydoc
      StorageBackend::incidenceDeletedDate()
    */
    QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      StorageBackend::eventCount()
    */
    int eventCount();

    /**
      @copydoc
      StorageBackend::todoCount()
    */
    int todoCount();

    /**
      @copydoc
      StorageBackend::journalCount()
    */
    int journalCount();

    /**
      @copydoc
      StorageBackend::virtual_hook()
    */
    virtual void virtual_hook(int id, void *data);

protected:
    bool modifyNotebook(const Notebook &nb, StorageBackend::DBOperation dbop, bool isDefault) override;
    bool modifyIncidences(const StorageBackend::Collection &additions,
                          const StorageBackend::Collection &modifications,
                          const StorageBackend::Collection &deletions,
                          StorageBackend::DeleteAction deleteAction) override;

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

public Q_SLOTS:
    void fileChanged(const QString &path);
};

}

#endif
