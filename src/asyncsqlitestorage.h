/*
  This file is part of the mkcal library.

  Copyright (c) 2022 Damien Caliste <dcaliste@free.fr>

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
  This file is part of the API for storing and accessing calendar data
  in an asynchromous way from an SQlite database.

  @author Damien Caliste \<dcaliste@free.fr \>
*/

#ifndef MKCAL_ASYNCSQLITESTORAGE_H
#define MKCAL_ASYNCSQLITESTORAGE_H

#include "mkcal_export.h"
#include "extendedstorage.h"

namespace mKCal {

/**
  @brief
  This class provides a calendar storage as an sqlite database.

  @warning When saving Attendees, the CustomProperties are not saved.
*/
class MKCAL_EXPORT AsyncSqliteStorage : public ExtendedStorage
{
    Q_OBJECT

public:

    /**
      A shared pointer to a AsyncSqliteStorage
    */
    typedef QSharedPointer<AsyncSqliteStorage> Ptr;

    /**
      Constructs a new AsyncSqliteStorage object for Calendar @p calendar with
      storage to file @p databaseName.

      @param calendar is a pointer to a valid Calendar object.
      @param databaseName is the name of the database containing the Calendar data.
      @param validateNotebooks set to true for saving only those incidences
             that belong to an existing notebook of this storage
    */
    AsyncSqliteStorage(const ExtendedCalendar::Ptr &cal,
                       const QString &databaseName = QString(),
                       bool validateNotebooks = true);

    /**
      Destructor.
    */
    virtual ~AsyncSqliteStorage();

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
      ExtendedStorage::loadContacts()
    */
    KCalendarCore::Person::List loadContacts();

    /**
      @copydoc
      ExtendedStorage::notifyOpened( const KCalendarCore::Incidence::Ptr & )
    */
    bool notifyOpened(const KCalendarCore::Incidence::Ptr &incidence);

    /**
      @copydoc
      ExtendedStorage::purgeDeletedIncidences(const KCalCore::Incidence::List &)
    */
    bool purgeDeletedIncidences(const KCalendarCore::Incidence::List &list);

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
      ExtendedStorage::duplicateIncidences()
    */
    bool duplicateIncidences(KCalendarCore::Incidence::List *list,
                             const KCalendarCore::Incidence::Ptr &incidence,
                             const QString &notebookUid = QString());

    /**
      @copydoc
      ExtendedStorage::incidenceDeletedDate()
    */
    QDateTime incidenceDeletedDate(const KCalendarCore::Incidence::Ptr &incidence);

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

    Notebook loadNotebook(const QString &uid) override;

    void registerDirectObserver(DirectStorageInterface::Observer *observer) override;
    void unregisterDirectObserver(DirectStorageInterface::Observer *observer) override;

protected:
    bool loadNotebooks() override;
    bool modifyNotebook(const Notebook::Ptr &nb, DBOperation dbop) override;
    bool loadBatch(const QList<DBLoadOperationWrapper> &dbops) override;
    bool loadIncidences(const DBLoadOperation &dbop) override;
    int loadIncidences(const DBLoadDateLimited &dbop, QDateTime *last,
                       int limit = -1, bool useDate = false, bool ignoreEnd = false) override;
    bool storeIncidences(const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &additions,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &modifications,
                         const QMultiHash<QString, KCalendarCore::Incidence::Ptr> &deletions,
                         ExtendedStorage::DeleteAction deleteAction) override;

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(AsyncSqliteStorage)
    class MKCAL_HIDE Private;
    Private *const d;
    //@endcond

    void notebookSaved(const QString &notebookUid, DBOperation dbop);
    void incidenceSaved(const KCalendarCore::MemoryCalendar *calendar,
                        const QStringList &added, const QStringList &modified, const QStringList &deleted);
};

}

#endif
