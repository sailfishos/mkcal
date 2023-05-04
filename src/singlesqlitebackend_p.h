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
  This file is part of the API for handling calendar data and
  defines the an interface to access a single file SQLite database.

  @author Damien Caliste \<dcaliste@free.fr\>
*/

#ifndef MKCAL_SINGLESQLITEBACKEND_H
#define MKCAL_SINGLESQLITEBACKEND_H

#include "mkcal_export.h"
#include "notebook.h"

#include <QObject>
#include <QHash>

#include <KCalendarCore/Incidence>

namespace mKCal {

class SqliteFormat;

/**
  @brief
  This class provides method to create, read and write
  KCalendarCore::Incidence into a single database. These incidences
  can come from various sources as long as (NotebookId, IncidenceId,
  RecurrenceId) are unique.
*/
class MKCAL_EXPORT SingleSqliteBackend
    : public QObject
{
    Q_OBJECT

public:
    /**
      Constructs a new SingleSqliteBackend object.

      @param databaseName is a path to a file. If not provided, the default
             path is built from Calendar/mkcal/db inside the user writable
             data directory. This value is overwritten by the SQLITESTORAGEDB
             environment variable.
    */
    SingleSqliteBackend(const QString &databaseName = QString());
    ~SingleSqliteBackend();

    /**
      Provides the path to the SQLite database managed by this object.

      @return a file path.
    */
    QString databaseName() const;

    /**
      Open the database to allow read / write operations. If the database
      file does not exist, it is created. It also creates the tables inside
      the database if they don't exist yet. If the database is based on an
      older format, it performs a migration to the latest format.

      @return true on success.
    */
    bool open();

    /**
      Close the database.

      @return true on success.
     */
    bool close();

    /**
      Read method, providing a list of incidences of a given notebook.

      @param list stores the list of returned incidences
      @param notebookUid identify a notebook. It returns an error
             to give an empty value.
      @param uid optionally provide a way to select specific incidences
             from notebookUid, based on their UID. In case of recurring
             incidences with exceptions, all incidences sharing the same
             UID are loaded.
      @return true on success.
     */
    bool incidences(KCalendarCore::Incidence::List *list,
                    const QString &notebookUid, const QString &uid = QString());

    /**
      Read method, providing a multi-notebook look-up based on incidence
      time.

      @param list stores the list of returned incidences, sorted by notebook
      @param start and end defined the window in which to list incidences.
             end parameter is exclusive. If start or end are invalid date times,
             the window is supposed to be open-ended. Both start and end
             cannot be invalid at the same time.
      @param loadAllRecurringIncidences true is listing all recurring incidences
             and their exceptions since there is no way at database level to
             know if a recurring event has occurrences within the start end
             window. When false, only non recurring event are listed (exceptions
             are not listed either to avoid ending up with orphaned exceptions).
      @return true on success.
     */
    bool incidences(QHash<QString, KCalendarCore::Incidence::List> *list,
                    const QDateTime &start, const QDateTime &end = QDateTime(),
                    bool loadAllRecurringIncidences = true);

    /**  
      Read method, providing a multi-notebook look-up based on incidence
      UID.

      @param list stores the list of returned incidences, sorted by notebook
      @param uid some incidence UID
      @return true on success.
     */
    bool incidences(QHash<QString, KCalendarCore::Incidence::List> *list,
                    const QString &uid = QString());

    /**
      Read method, providing a way to list incidences marked as deleted
      but not yet purged from the database.

      @param list stores the list of returned incidences
      @param notebookUid identify a notebook. It returns an error
             to give an empty value.
      @return true on success.
     */
    bool deletedIncidences(KCalendarCore::Incidence::List *list,
                           const QString &notebookUid);

    /**
      Read method, providing a way to list incidences based on a substring
      in the summary, description or location fields.

      More incidences than the listed ones in @param identifiers may be
      loaded into @param list to ensure consistency with respect to
      exceptions of recurring incidences.

      @param list stores the list of returned incidences, sorted by notebook
      @param identifiers stores the instance identifiers of
             matching incidences, sorted by notebook.
      @param key can be any substring from the summary, the description or the location.
      @param limit the maximum number of loaded incidences, unlimited by default
      @return true on success.
     */
    bool search(QHash<QString, KCalendarCore::Incidence::List> *list,
                QHash<QString, QStringList> *identifiers,
                const QString &key, int limit = 0);

    /**
      Write method, deferring any later call to addIncidence(), modifyIncidence(),
      deleteIncidence() or purgeIncidence() up to the moment commit() is called.
      This allows to write changes to the database by batch.

      @return true on success.
     */
    bool deferSaving();

    /**
      Write method, add a new incidence to the database. The triplet
      (notebookUid, incidence.uid(), incidence.recurrenceid()) should
      not already exist. All incidences already marked as deleted
      and sharing the same triplet are removed from the database before
      performing the insertion.

      On success, the updated() signal is emitted.

      @param notebookUid defines the notebook the incidence belongs to
      @param incidence describes the new incidence to insert
      @return true on success.
     */
    bool addIncidence(const QString &notebookUid,
                      const KCalendarCore::Incidence &incidence);

    /**
      Write method, modify an existing incidence in the database. The triplet
      (notebookUid, incidence.uid(), incidence.recurrenceid()) should
      already exist.

      On success, the updated() signal is emitted.

      @param notebookUid defines the notebook the incidence belongs to
      @param incidence describes the incidence to update
      @return true on success.
     */
    bool modifyIncidence(const QString &notebookUid,
                         const KCalendarCore::Incidence &incidence);

    /**
      Write method, mark an existing incidence as deleted in the database
      without removing it. The triplet (notebookUid, incidence.uid(),
      incidence.recurrenceid()) should already exist and be already
      marked as deleted.

      On success, the updated() signal is emitted.

      @param notebookUid defines the notebook the incidence belongs to
      @param incidence describes the incidence to be deleted
      @return true on success.
     */
    bool deleteIncidence(const QString &notebookUid,
                         const KCalendarCore::Incidence &incidence);

    /**
      Write method, remove an existing incidence from the database.
      The triplet (notebookUid, incidence.uid(), incidence.recurrenceid())
      may already exist, if not no error is returned. The incidence
      should not already be marked as deleted.

      On success, the updated() signal is emitted.

      @param notebookUid defines the notebook the incidence belongs to
      @param incidence describes the incidence to be removed
      @return true on success.
     */
    bool purgeIncidence(const QString &notebookUid,
                        const KCalendarCore::Incidence &incidence);

    /**
      Write method, commit deferred changes to the database.
      
      On success, the updated() signal is emitted.

      @return true on success.
     */
    bool commit();

    /**
      Write method, remove from the database the list of incidence marked
      as deleted.

      @param notebookUid defines the notebook the incidences belong to
      @param list defines the list of incidences marked as deleted to be
             permanently removed from the database.
      @return true on success.
     */
    bool purgeDeletedIncidences(const QString &notebookUid,
                                const KCalendarCore::Incidence::List &list);

    /**
      Read method, provides the list of notebooks as defined in the database.

      @param list stores the list of returned notebooks
      @param defaultNb points to the default notebook in the database, if any.
      @return true on success.
     */
    bool notebooks(Notebook::List *list, Notebook::Ptr *defaultNb = nullptr);

    /**
      Write method, add a notebook to the database. notebook.uid() should
      not already exists in the database.

      @param notebook describes the new notebook to insert
      @param isDefault defines if this new notebook will become the
             default one
      @return true on success.
     */
    bool addNotebook(const Notebook &notebook, bool isDefault);

    /**
      Write method, modify an existing notebook of the database.
      notebook.uid() should already exists in the database.

      @param notebook describes the notebook to modify
      @param isDefault defines if this notebook will become the
             default one
      @return true on success.
     */
    bool updateNotebook(const Notebook &notebook, bool isDefault);

    /**
      Write method, remove an existing notebook of the database.
      notebook.uid() should already exists in the database. All
      associated incidences to this notebook, marked as deleted
      or not, are also removed.

      @param notebook describes the notebook to be removed
      @return true on success.
     */
    bool deleteNotebook(const Notebook &notebook);

    // To be removed, kept for backward compatibility inside SqliteStorage.
    SqliteFormat* acquireDb();
    void releaseDb();

signals:
    /**
      This signal is emitted whenever the database has been modified
      by external means from this object. Extend of the modifications
      are unknown and any could be possible.
     */
    void modified();
    /**
      This signal is emitted when incidences have been sucessfully
      updated by this object.

      @param added lists the instance identifiers, sorted by notebook,
             of newly added incidences.
      @param modified lists the instance identifiers, sorted by notebook,
             of modified incidences.
      @param deleted lists the instance identifiers, sorted by notebook,
             of marked as deleted or purged incidences.
     */
    void updated(const QHash<QString, QStringList> &added,
                 const QHash<QString, QStringList> &modified,
                 const QHash<QString, QStringList> &deleted);

private:
    //@cond PRIVATE
    Q_DISABLE_COPY(SingleSqliteBackend)
    class Private;
    Private *const d;
    //@endcond

public Q_SLOTS:
    void fileChanged(const QString &path);
};

}
#endif
