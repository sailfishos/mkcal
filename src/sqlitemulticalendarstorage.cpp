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

#include "sqlitemulticalendarstorage.h"
#include "singlesqlitebackend_p.h"
#include "logging_p.h"

using namespace KCalendarCore;

using namespace mKCal;

//@cond PRIVATE
class SqliteMultiCalendarStorage::Private
{
public:
    Private(const QString &databaseName)
        : mBackend(databaseName)
    {
    }

    SingleSqliteBackend mBackend;
    Notebook::List mDbNotebooks;
    Notebook::Ptr mDefaultNotebook;

    bool loadNotebooks();
};
//@endcond

SqliteMultiCalendarStorage::SqliteMultiCalendarStorage(const QTimeZone &timezone,
                                                       const QString &databaseName)
    : MultiCalendarStorage(timezone)
    , d(new Private(databaseName))
{
}

SqliteMultiCalendarStorage::~SqliteMultiCalendarStorage()
{
    delete d;
}

bool SqliteMultiCalendarStorage::open()
{
    if (!d->mBackend.open() || !d->loadNotebooks()) {
        return false;
    }
    connect(&d->mBackend, &SingleSqliteBackend::modified,
            this, &SqliteMultiCalendarStorage::onModified);
    connect(&d->mBackend, &SingleSqliteBackend::updated,
            this, &SqliteMultiCalendarStorage::onUpdated);
    return MultiCalendarStorage::open();
}

bool SqliteMultiCalendarStorage::close()
{
    if (!d->mBackend.close()) {
        return false;
    }
    d->mDbNotebooks.clear();
    d->mDefaultNotebook.clear();
    disconnect(&d->mBackend, &SingleSqliteBackend::modified,
               this, &SqliteMultiCalendarStorage::onModified);
    disconnect(&d->mBackend, &SingleSqliteBackend::updated,
               this, &SqliteMultiCalendarStorage::onUpdated);
    return  MultiCalendarStorage::close();
}

bool SqliteMultiCalendarStorage::Private::loadNotebooks()
{
    mDbNotebooks.clear();
    mDefaultNotebook.clear();

    return mBackend.notebooks(&mDbNotebooks, &mDefaultNotebook);
}

void SqliteMultiCalendarStorage::onModified()
{
    d->loadNotebooks();
    emitStorageModified();
}

void SqliteMultiCalendarStorage::onUpdated(const QHash<QString, QStringList> &added,
                                           const QHash<QString, QStringList> &modified,
                                           const QHash<QString, QStringList> &deleted)
{
    emitStorageUpdated(added, modified, deleted);
}

Notebook::List SqliteMultiCalendarStorage::loadedNotebooks(QString *defaultUid) const
{
    if (defaultUid) {
        *defaultUid = d->mDefaultNotebook ? d->mDefaultNotebook->uid() : QString();
    }
    Notebook::List list;
    for (const Notebook::Ptr &nb : d->mDbNotebooks) {
        list.append(Notebook::Ptr(new Notebook(*nb)));
    }
    return list;
}

bool SqliteMultiCalendarStorage::load(const QDate &start, const QDate &end)
{
    bool success = true;
    QDateTime loadStart, loadEnd;
    if (getLoadDates(start, end, &loadStart, &loadEnd)) {
        bool loadAllRecurringIncidences = !isRecurrenceLoaded();
        QHash<QString, Incidence::List> hash;
        success = d->mBackend.incidences(&hash, loadStart, loadEnd, loadAllRecurringIncidences) && addIncidences(hash);

        if (success) {
            addLoadedRange(loadStart.date(), loadEnd.date());
        }
        if (loadAllRecurringIncidences) {
           setIsRecurrenceLoaded(success);
        }
    }

    return success;
}

bool SqliteMultiCalendarStorage::search(const QString &key, QStringList *identifiers, int limit)
{
    QHash<QString, Incidence::List> hash;
    QHash<QString, QStringList> ids;
    if (d->mBackend.search(&hash, &ids, key, limit) && addIncidences(hash)) {
        if (identifiers) {
            for (QHash<QString, QStringList>::ConstIterator it = ids.constBegin();
                 it != ids.constEnd(); it++) {
                for (const QString &id : it.value()) {
                    identifiers->append(multiCalendarIdentifier(it.key(), id));
                }
            }
        }
        return true;
    } else {
        return false;
    }
}

Incidence::List SqliteMultiCalendarStorage::incidences(const QString &notebookUid,
                                                       const QString &uid)
{
    MemoryCalendar::Ptr cal = calendar(notebookUid);
    Incidence::List list;
    if (uid.isEmpty() || !cal->incidence(uid)) {
        d->mBackend.incidences(&list, notebookUid, uid);
    } else {
        list.append(cal->incidence(uid));
        list += cal->instances(list[0]);
    }
    return list;
}

bool SqliteMultiCalendarStorage::save(const QString &notebookUid,
                                      const QHash<QString, Incidence::List> &added,
                                      const QHash<QString, Incidence::List> &modified,
                                      const QHash<QString, Incidence::List> &deleted,
                                      DeleteAction deleteAction)
{
    Notebook::List::Iterator it = d->mDbNotebooks.begin();
    while (it != d->mDbNotebooks.end()) {
        if (!notebook((*it)->uid())
            && (notebookUid.isEmpty() || (*it)->uid() == notebookUid)) {
            if (d->mBackend.deleteNotebook(**it)) {
                it = d->mDbNotebooks.erase(it);
            } else {
                qCWarning(lcMkcal) << "cannot delete notebook from storage." << (*it)->uid();
                return false;
            }
        } else {
            it++;
        }
    }

    const QString defaultUid = defaultNotebook() ? defaultNotebook()->uid() : QString();
    const QString dbDefaultUid = d->mDefaultNotebook ? d->mDefaultNotebook->uid() : QString();
    for (const Notebook::Ptr &nb : notebooks()) {
        Notebook::Ptr dbNotebook;
        for (Notebook::List::ConstIterator dbNb = d->mDbNotebooks.begin();
             dbNb != d->mDbNotebooks.end(); dbNb++) {
            if ((*dbNb)->uid() == nb->uid()) {
                dbNotebook = *dbNb;
                break;
            }
        }
        // Ensure notebooks exist in DB and are up-to-date.
        if (!dbNotebook
            && (notebookUid.isEmpty() || nb->uid() == notebookUid)) {
            if (d->mBackend.addNotebook(*nb, false)) {
                d->mDbNotebooks.append(Notebook::Ptr(new Notebook(*nb)));
            } else {
                qCWarning(lcMkcal) << "cannot add notebook" << nb->uid();
                return false;
            }
        } else if ((!(*dbNotebook == *nb)
                    && (notebookUid.isEmpty() || nb->uid() == notebookUid))
                   || (defaultUid != dbDefaultUid
                       && (notebookUid.isEmpty() || defaultUid == notebookUid)
                       && nb->uid() == defaultUid))  {
            if (d->mBackend.updateNotebook(*nb, defaultUid == nb->uid())) {
                Notebook old(*dbNotebook);
                *dbNotebook = *nb;
                emitNotebookUpdated(old);
            } else {
                qCWarning(lcMkcal) << "cannot update notebook" << nb->uid();
                return false;
            }
        }
    }

    // Now save incidence changes.
    if (!d->mBackend.deferSaving()) {
        return false;
    }

    bool success = true;
    for (QHash<QString, Incidence::List>::ConstIterator it = added.constBegin();
         it != added.constEnd(); it++) {
        for (const Incidence::Ptr &incidence : it.value()) {
            success = d->mBackend.addIncidence(it.key(), *incidence) && success;
        }
    }
    for (QHash<QString, Incidence::List>::ConstIterator it = modified.constBegin();
         it != modified.constEnd(); it++) {
        for (const Incidence::Ptr &incidence : it.value()) {
            success = d->mBackend.modifyIncidence(it.key(), *incidence) && success;
        }
    }
    if (deleteAction == MarkDeleted) {
        for (QHash<QString, Incidence::List>::ConstIterator it = deleted.constBegin();
             it != deleted.constEnd(); it++) {
            for (const Incidence::Ptr &incidence : it.value()) {
                success = d->mBackend.deleteIncidence(it.key(), *incidence) && success;
            }
        }
    } else if (deleteAction == PurgeDeleted) {
        for (QHash<QString, Incidence::List>::ConstIterator it = deleted.constBegin();
             it != deleted.constEnd(); it++) {
            for (const Incidence::Ptr &incidence : it.value()) {
                success = d->mBackend.purgeIncidence(it.key(), *incidence) && success;
            }
        }
    } else if (deleteAction == PurgeOnLocal) {
        for (QHash<QString, Incidence::List>::ConstIterator it = deleted.constBegin();
             it != deleted.constEnd(); it++) {
            Notebook::Ptr nb = notebook(it.key());
            if (nb->isMaster() && !nb->isShared() && nb->pluginName().isEmpty()) {
                for (const Incidence::Ptr &incidence : it.value()) {
                    success = d->mBackend.purgeIncidence(it.key(), *incidence) && success;
                }
            } else {
                for (const Incidence::Ptr &incidence : it.value()) {
                    success = d->mBackend.deleteIncidence(it.key(), *incidence) && success;
                }
            }
        }        
    }

    success = d->mBackend.commit() && success;

    return success;
}
