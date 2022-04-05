/*
  This file is part of the mkcal library.

  Copyright (c) 2002,2003 Cornelius Schumacher <schumacher@kde.org>
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
  defines the ExtendedStorage abstract base class.

  @brief
  An abstract base class that provides a calendar storage interface.

  @author Cornelius Schumacher \<schumacher@kde.org\>
*/
#include "extendedstorage.h"
#include "logging_p.h"

#include <KCalendarCore/Exceptions>
#include <KCalendarCore/Calendar>
using namespace KCalendarCore;

#include <QtCore/QUuid>

using namespace mKCal;

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::ExtendedStorage::Private
    : public StorageBackend::Observer
    , public StorageBackend::Manager
    , public KCalendarCore::Calendar::CalendarObserver
{
public:
    Private(ExtendedCalendar::Ptr calendar, bool validateNotebooks)
        : mCalendar(calendar)
        , mValidateNotebooks(validateNotebooks)
    {}

    ExtendedCalendar::Ptr mCalendar;
    bool mValidateNotebooks;
    QHash<QString, Notebook::Ptr> mNotebooks; // uid to notebook
    Notebook::Ptr mDefaultNotebook;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToInsert;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToUpdate;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToDelete;

    void newNotebooks(StorageBackend *storage,
                      const StorageBackend::Library &notebooks, const QString &defaultNotebookId) override;
    void newIncidences(StorageBackend *storage,
                       const StorageBackend::Collection &incidences) override;

    void storageOpened(StorageBackend *storage) override;
    void storageClosed(StorageBackend *storage) override;
    void storageModified(StorageBackend *storage) override;

    void calendarModified(bool modified, KCalendarCore::Calendar *calendar);
    void calendarIncidenceAdded(const KCalendarCore::Incidence::Ptr &incidence);
    void calendarIncidenceChanged(const KCalendarCore::Incidence::Ptr &incidence);
    void calendarIncidenceDeleted(const KCalendarCore::Incidence::Ptr &incidence,
                                  const KCalendarCore::Calendar *calendar);
    void calendarIncidenceAdditionCanceled(const KCalendarCore::Incidence::Ptr &incidence);
};
//@endcond

ExtendedStorage::ExtendedStorage(const ExtendedCalendar::Ptr &cal, const QString &dbFile, bool validateNotebooks)
    : SqliteStorage(cal->timeZone(), dbFile),
      d(new ExtendedStorage::Private(cal, validateNotebooks))
{
    cal->registerObserver(d);
    registerManager(d);
    registerObserver(d);
}

ExtendedStorage::~ExtendedStorage()
{
    d->mCalendar->unregisterObserver(d);
    unregisterObserver(d);
    unregisterManager(d);
    delete d;
}

ExtendedCalendar::Ptr ExtendedStorage::calendar()
{
    return d->mCalendar;
}

bool ExtendedStorage::cancel()
{
    return true;
}

bool ExtendedStorage::save()
{
    return save(ExtendedStorage::MarkDeleted);
}

bool ExtendedStorage::save(ExtendedStorage::DeleteAction deleteAction)
{
    StorageBackend::SharedCollection added;
    StorageBackend::SharedCollection modified;
    StorageBackend::SharedCollection deleted;

    QMultiHash<QString, Incidence::Ptr>::ConstIterator it;
    for (it = d->mIncidencesToInsert.constBegin();
         it != d->mIncidencesToInsert.constEnd(); it++) {
        const QString notebookUid = calendar()->notebook(*it);
        if (isValidNotebook(notebookUid)) {
            added.insert(notebookUid, *it);
        } else {
            qCWarning(lcMkcal) << "invalid notebook - not saving incidence" << (*it)->uid();
        }
    }
    for (it = d->mIncidencesToUpdate.constBegin();
         it != d->mIncidencesToUpdate.constEnd(); it++) {
        const QString notebookUid = calendar()->notebook(*it);
        if (isValidNotebook(notebookUid)) {
            modified.insert(notebookUid, *it);
        } else {
            qCWarning(lcMkcal) << "invalid notebook - not updating incidence" << (*it)->uid();
        }
    }
    for (it = d->mIncidencesToDelete.constBegin();
         it != d->mIncidencesToDelete.constEnd(); it++) {
        deleted.insert(calendar()->notebook(*it), *it);
    }
    d->mIncidencesToInsert.clear();
    d->mIncidencesToUpdate.clear();
    d->mIncidencesToDelete.clear();

    return storeIncidences(added, modified, deleted, deleteAction);
}

bool ExtendedStorage::notifyOpened(const Incidence::Ptr &incidence)
{
    Q_UNUSED(incidence);
    return false;
}

void ExtendedStorage::Private::calendarModified(bool modified, Calendar *calendar)
{
    Q_UNUSED(calendar);
    qCDebug(lcMkcal) << "calendarModified called:" << modified;
}

void ExtendedStorage::Private::calendarIncidenceAdded(const Incidence::Ptr &incidence)
{
    QMultiHash<QString, Incidence::Ptr>::Iterator deleted =
        mIncidencesToDelete.find(incidence->uid());
    if (deleted != mIncidencesToDelete.end()) {
        qCDebug(lcMkcal) << "removing incidence from deleted" << incidence->uid();
        while (deleted != mIncidencesToDelete.end()) {
            if ((*deleted)->recurrenceId() == incidence->recurrenceId()) {
                deleted = mIncidencesToDelete.erase(deleted);
                calendarIncidenceChanged(incidence);
            } else {
                ++deleted;
            }
        }
    } else if (!mIncidencesToInsert.contains(incidence->uid(), incidence)) {

        QString uid = incidence->uid();

        if (uid.length() < 7) {   // We force a minimum length of uid to grant uniqness
            QByteArray suuid(QUuid::createUuid().toByteArray());
            qCDebug(lcMkcal) << "changing" << uid << "to" << suuid;
            incidence->setUid(suuid.mid(1, suuid.length() - 2));
        }

        qCDebug(lcMkcal) << "appending incidence" << incidence->uid() << "for database insert";
        mIncidencesToInsert.insert(incidence->uid(), incidence);
    }
}

void ExtendedStorage::Private::calendarIncidenceChanged(const Incidence::Ptr &incidence)
{
    if (!mIncidencesToUpdate.contains(incidence->uid(), incidence) &&
            !mIncidencesToInsert.contains(incidence->uid(), incidence)) {
        qCDebug(lcMkcal) << "appending incidence" << incidence->uid() << "for database update";
        mIncidencesToUpdate.insert(incidence->uid(), incidence);
    }
}

void ExtendedStorage::Private::calendarIncidenceDeleted(const Incidence::Ptr &incidence,
                                                        const KCalendarCore::Calendar *calendar)
{
    Q_UNUSED(calendar);

    if (mIncidencesToInsert.contains(incidence->uid(), incidence)) {
        qCDebug(lcMkcal) << "removing incidence from inserted" << incidence->uid();
        mIncidencesToInsert.remove(incidence->uid(), incidence);
    } else if (!mIncidencesToDelete.contains(incidence->uid(), incidence)) {
        qCDebug(lcMkcal) << "appending incidence" << incidence->uid() << "for database delete";
        mIncidencesToDelete.insert(incidence->uid(), incidence);
    }
}

void ExtendedStorage::Private::calendarIncidenceAdditionCanceled(const Incidence::Ptr &incidence)
{
    if (mIncidencesToInsert.contains(incidence->uid())) {
        qCDebug(lcMkcal) << "duplicate - removing incidence from inserted" << incidence->uid();
        mIncidencesToInsert.remove(incidence->uid(), incidence);
    }
}

void ExtendedStorage::Private::newNotebooks(StorageBackend *storage,
                                            const StorageBackend::Library &notebooks,
                                            const QString &defaultNotebookId)
{
    for (Notebook *notebook : notebooks) {
        const Notebook::Ptr nb(notebook);
        mNotebooks.insert(nb->uid(), nb);
        if (!mCalendar->addNotebook(nb->uid(), nb->isVisible())
            && !mCalendar->updateNotebook(nb->uid(), nb->isVisible())) {
            qCWarning(lcMkcal) << "notebook" << nb->uid() << "already in calendar";
        }
        if (notebook->uid() == defaultNotebookId) {
            mDefaultNotebook = nb;
            if (!mCalendar->setDefaultNotebook(defaultNotebookId)) {
                qCWarning(lcMkcal) << "cannot set notebook" << defaultNotebookId << "as default in calendar";
            }
        }
    }
}

static bool isContaining(const QMultiHash<QString, Incidence::Ptr> &list, const Incidence::Ptr &incidence)
{
    QMultiHash<QString, Incidence::Ptr>::ConstIterator it = list.find(incidence->uid());
    for (; it != list.constEnd(); ++it) {
        if ((*it)->recurrenceId() == incidence->recurrenceId()) {
            return true;
        }
    }
    return false;
}

void ExtendedStorage::Private::newIncidences(StorageBackend *storage,
                                             const StorageBackend::Collection &incidences)
{
    mCalendar->unregisterObserver(this);
    for(StorageBackend::Collection::ConstIterator it = incidences.constBegin();
        it != incidences.constEnd(); it++) {
        const Incidence::Ptr incidence(it.value());
        bool added = true;
        bool hasNotebook = mCalendar->hasValidNotebook(it.key());
        // Cannot use .contains(incidence->uid(), incidence) here, like
        // in the rest of the file, since incidence here is a new one
        // returned by the selectComponents() that cannot by design be already
        // in the multihash tables.
        if (isContaining(mIncidencesToInsert, incidence) ||
            isContaining(mIncidencesToUpdate, incidence) ||
            isContaining(mIncidencesToDelete, incidence) ||
            (mValidateNotebooks && !hasNotebook)) {
            qCWarning(lcMkcal) << "not loading" << incidence->uid() << it.key()
                               << (!hasNotebook ? "(invalidated notebook)" : "(local changes)");
            added = false;
        } else {
            Incidence::Ptr old(mCalendar->incidence(incidence->uid(),
                                                    incidence->recurrenceId()));
            if (old) {
                if (incidence->revision() > old->revision()) {
                    mCalendar->deleteIncidence(old);   // move old to deleted
                    // and replace it with the new one.
                } else {
                    added = false;
                }
            }
        }
        if (added && !mCalendar->addIncidence(incidence, it.key())) {
            qCWarning(lcMkcal) << "cannot add incidence" << incidence->uid()
                               << "to notebook" << it.key();
        }
    }
    mCalendar->registerObserver(this);
}

void ExtendedStorage::Private::storageOpened(StorageBackend *storage)
{
    if (storage->timeZone().isValid()) {
        mCalendar->setTimeZone(storage->timeZone());
    }
}

void ExtendedStorage::Private::storageClosed(StorageBackend *storage)
{
    mIncidencesToInsert.clear();
    mIncidencesToUpdate.clear();
    mIncidencesToDelete.clear();

    mNotebooks.clear();
    mDefaultNotebook.clear();
}

void ExtendedStorage::Private::storageModified(StorageBackend *storage)
{
    for (const Notebook::Ptr &nb : mNotebooks.values()) {
        if (!mCalendar->deleteNotebook(nb->uid())) {
            qCDebug(lcMkcal) << "notebook" << nb->uid() << "already removed from calendar";
        }
    }
    mCalendar->close();
    mNotebooks.clear();
    mDefaultNotebook = Notebook::Ptr();
}

bool ExtendedStorage::addNotebook(const Notebook::Ptr &nb)
{
    if (!nb || d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (nb->uid().length() < 7) {
        // Cannot accept this id, create better one.
        QString uid(QUuid::createUuid().toString());
        nb->setUid(uid.mid(1, uid.length() - 2));
    }

    if (!StorageBackend::addNotebook(*nb, nb == d->mDefaultNotebook)) {
        return false;
    }

    d->mNotebooks.insert(nb->uid(), nb);
    if (!calendar()->addNotebook(nb->uid(), nb->isVisible())
        && !calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "notebook" << nb->uid() << "already in calendar";
    }

    return true;
}

bool ExtendedStorage::updateNotebook(const Notebook::Ptr &nb)
{
    if (!nb || !d->mNotebooks.contains(nb->uid()) ||
            d->mNotebooks.value(nb->uid()) != nb) {
        return false;
    }

    if (!StorageBackend::updateNotebook(*nb, nb == d->mDefaultNotebook)) {
        return false;
    }

    if (!calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "cannot update notebook" << nb->uid() << "in calendar";
        return false;
    }

    return true;
}

bool ExtendedStorage::deleteNotebook(const Notebook::Ptr &nb)
{
    if (!nb || !d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!StorageBackend::deleteNotebook(*nb)) {
        return false;
    }

    calendar()->unregisterObserver(d);
    const Incidence::List list = calendar()->incidences(nb->uid());
    qCDebug(lcMkcal) << "deleting" << list.size() << "incidences from calendar";
    for (const Incidence::Ptr &toDelete : list) {
        // Need to test the existence of toDelete inside the calendar here,
        // because KCalendarCore::Calendar::incidences(nbuid) is returning
        // all incidences associated to nbuid, even those that have been
        // deleted already.
        // In addition, Calendar::deleteIncidence() is also deleting all exceptions
        // of a recurring event, so exceptions may have been already removed and
        // their existence should be checked to avoid warnings.
        if (calendar()->incidence(toDelete->uid(), toDelete->recurrenceId()))
            calendar()->deleteIncidence(toDelete);
    }
    calendar()->registerObserver(d);

    if (!calendar()->deleteNotebook(nb->uid())) {
        qCWarning(lcMkcal) << "notebook" << nb->uid() << "already deleted from calendar";
    }

    d->mNotebooks.remove(nb->uid());

    if (d->mDefaultNotebook == nb) {
        d->mDefaultNotebook.clear();
    }

    return true;
}

bool ExtendedStorage::setDefaultNotebook(const Notebook::Ptr &nb)
{
    d->mDefaultNotebook = nb;

    if (!nb
        || (d->mNotebooks.contains(nb->uid()) && !updateNotebook(nb))
        || (!d->mNotebooks.contains(nb->uid()) && !addNotebook(nb))) {
        return false;
    }

    if (!calendar()->setDefaultNotebook(nb->uid())) {
        qCWarning(lcMkcal) << "cannot set notebook" << nb->uid() << "as default in calendar";
    }

    return true;
}

Notebook::Ptr ExtendedStorage::defaultNotebook()
{
    return d->mDefaultNotebook;
}

Notebook::List ExtendedStorage::notebooks()
{
    return d->mNotebooks.values();
}

Notebook::Ptr ExtendedStorage::notebook(const QString &uid)
{
    return d->mNotebooks.value(uid);
}

void ExtendedStorage::setValidateNotebooks(bool validateNotebooks)
{
    d->mValidateNotebooks = validateNotebooks;
}

bool ExtendedStorage::validateNotebooks()
{
    return d->mValidateNotebooks;
}

bool ExtendedStorage::isValidNotebook(const QString &notebookUid)
{
    Notebook::Ptr nb = notebook(notebookUid);
    if (!nb.isNull()) {
        if (nb->isRunTimeOnly() || nb->isReadOnly()) {
            qCDebug(lcMkcal) << "notebook" << notebookUid << "isRunTimeOnly or isReadOnly";
            return false;
        }
    } else if (validateNotebooks()) {
        qCDebug(lcMkcal) << "notebook" << notebookUid << "is not valid for this storage";
        return false;
    } else if (calendar()->hasValidNotebook(notebookUid)) {
        qCDebug(lcMkcal) << "notebook" << notebookUid << "is saved by another storage";
        return false;
    }
    return true;
}

Incidence::Ptr ExtendedStorage::checkAlarm(const QString &uid, const QString &recurrenceId,
                                           bool loadAlways)
{
    QDateTime rid;

    if (!recurrenceId.isEmpty()) {
        rid = QDateTime::fromString(recurrenceId, Qt::ISODate);
    }
    Incidence::Ptr incidence = calendar()->incidence(uid, rid);
    if (!incidence || loadAlways) {
        load(uid, rid);
        incidence = calendar()->incidence(uid, rid);
    }
    if (incidence && incidence->hasEnabledAlarms()) {
        // Return incidence if it exists and has active alarms.
        return incidence;
    }
    return Incidence::Ptr();
}
