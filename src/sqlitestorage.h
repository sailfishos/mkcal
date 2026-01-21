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
#include "extendedstorage.h"

namespace mKCal {

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
      Constructs a new SqliteStorage object for Calendar @p calendar with
      storage to file @p databaseName.

      @param calendar is a pointer to a valid Calendar object.
      @param databaseName is the name of the database containing the Calendar data.
      @param validateNotebooks set to true for saving only those incidences
             that belong to an existing notebook of this storage
    */
    explicit SqliteStorage(const ExtendedCalendar::Ptr &cal,
                           const QString &databaseName,
                           bool validateNotebooks = true);

    /**
      Constructs a new SqliteStorage object for Calendar @p calendar. Location
      of the database is using default location, or is taken from SQLITESTORAGEDB
      enivronment variable.

      @param calendar is a pointer to a valid Calendar object.
      @param validateNotebooks set to true for saving only those incidences
             that belong to an existing notebook of this storage
    */
    explicit SqliteStorage(const ExtendedCalendar::Ptr &cal,
                           bool validateNotebooks = true);

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
      ExtendedStorage::load(const QString &)
    */
    bool load(const QString &uid);

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
      ExtendedStorage::purgeDeletedIncidences(const KCalCore::Incidence::List &, const QString &)
    */
    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list,
                                const QString &notebookUid = QString());

    /**
      @copydoc
      CalStorage::save()
    */
    bool save();

    /**
      @copydoc
      ExtendedStorage::save(ExtendedStorage::DeleteAction deleteAction)
    */
    bool save(ExtendedStorage::DeleteAction deleteAction);

    /**
      @copydoc
      CalStorage::close()
    */
    bool close();

    /**
      @copydoc
      Calendar::CalendarObserver::calendarModified()
    */
    void calendarModified(bool modified, KCalendarCore::Calendar *calendar);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceCreated()
    */
    void calendarIncidenceCreated(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdded()
    */
    void calendarIncidenceAdded(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceChanged()
    */
    void calendarIncidenceChanged(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceDeleted()
    */
    void calendarIncidenceDeleted(const KCalendarCore::Incidence::Ptr &incidence, const KCalendarCore::Calendar *calendar);

    /**
      @copydoc
      Calendar::CalendarObserver::calendarIncidenceAdditionCanceled()
    */
    void calendarIncidenceAdditionCanceled(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      ExtendedStorage::insertedIncidences()
    */
    bool insertedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::modifiedIncidences()
    */
    bool modifiedIncidences(KCalendarCore::Incidence::List *list, const QDateTime &after,
                            const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::deletedIncidences()
    */
    bool deletedIncidences(KCalendarCore::Incidence::List *list,
                           const QDateTime &after = QDateTime(),
                           const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::allIncidences()
    */
    bool allIncidences(KCalendarCore::Incidence::List *list, const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::search()
    */
    bool search(const QString &key, QStringList *identifiers, int limit = 0);

    /**
      @copydoc
      ExtendedStorage::incidenceDeletedDate()
    */
    QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      ExtendedStorage::virtual_hook()
    */
    virtual void virtual_hook(int id, void *data);

protected:
    bool loadNotebooks();
    bool insertNotebook(const Notebook::Ptr &nb);
    bool modifyNotebook(const Notebook::Ptr &nb);
    bool eraseNotebook(const Notebook::Ptr &nb);

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SqliteStorage)
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
