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
#include "extendedstorageobserver.h"
#include "timed.h"
#include "logging_p.h"

#include <KCalendarCore/Exceptions>
#include <KCalendarCore/Calendar>
using namespace KCalendarCore;

using namespace mKCal;

struct Range
{
    Range(const QDate &start, const QDate &end)
        : mStart(start), mEnd(end) { }

    bool contains(const QDate &at) const
    {
        return at.isValid()
            && (mStart.isNull() || at >= mStart)
            && (mEnd.isNull() || at <= mEnd);
    }

    QDate mStart, mEnd;
};

// Range a is strictly before range b.
bool operator<(const Range &a, const Range &b)
{
    return a.mEnd.isValid() && b.mStart.isValid() && a.mEnd < b.mStart;
}
// Date a is strictly before range b.
bool operator<(const QDate &at, const Range &range)
{
    return at.isNull() || (range.mStart.isValid() && at < range.mStart);
}

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@cond PRIVATE
class mKCal::ExtendedStorage::Private: public Calendar::CalendarObserver
{
public:
    Private(bool validateNotebooks)
        : mValidateNotebooks(validateNotebooks),
          mDefaultNotebook(0)
    {}

    bool mValidateNotebooks;
    QList<Range> mRanges;
    bool mIsRecurrenceLoaded = false;
    bool mIsUncompletedTodosLoaded = false;
    bool mIsCompletedTodosDateLoaded = false;
    bool mIsCompletedTodosCreatedLoaded = false;
    bool mIsDateLoaded = false;
    bool mIsCreatedLoaded = false;
    bool mIsFutureDateLoaded = false;
    bool mIsGeoDateLoaded = false;
    bool mIsGeoCreatedLoaded = false;
    bool mIsJournalsLoaded = false;
    TimedPlugin mAlarms;
    QList<ExtendedStorageObserver *> mObservers;
    QHash<QString, Notebook::Ptr> mNotebooks; // uid to notebook
    Notebook::Ptr mDefaultNotebook;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToInsert;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToUpdate;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToDelete;
    bool mIsBatchLoading = false;
    QList<DBLoadOperationWrapper> mBatchLoadingOperations;

    void calendarModified(bool modified, Calendar *calendar);
    void calendarIncidenceCreated(const Incidence::Ptr &incidence);
    void calendarIncidenceAdded(const Incidence::Ptr &incidence);
    void calendarIncidenceChanged(const Incidence::Ptr &incidence);
    void calendarIncidenceDeleted(const Incidence::Ptr &incidence, const Calendar *calendar);
    void calendarIncidenceAdditionCanceled(const Incidence::Ptr &incidence);

    void clearLoaded();
};
//@endcond

ExtendedStorage::ExtendedStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks)
    : CalStorage(cal),
      d(new ExtendedStorage::Private(validateNotebooks))
{
    cal->registerObserver(d);
}

ExtendedStorage::~ExtendedStorage()
{
    calendar()->unregisterObserver(d);
    delete d;
}

bool ExtendedStorage::open()
{
    registerDirectObserver(&d->mAlarms);
    return true;
}

bool ExtendedStorage::close()
{
    d->clearLoaded();

    unregisterDirectObserver(&d->mAlarms);
    d->mNotebooks.clear();
    d->mDefaultNotebook = Notebook::Ptr();

    return true;
}

bool ExtendedStorage::save()
{
    return save(ExtendedStorage::MarkDeleted);
}

bool ExtendedStorage::save(ExtendedStorage::DeleteAction deleteAction)
{
    // Notice : we allow to save/delete incidences in a read-only
    // notebook. The read-only flag is a hint only. This allows
    // to update a marked as read-only notebook to reflect external
    // changes.
    QMultiHash<QString, Incidence::Ptr>::Iterator it;
    it = d->mIncidencesToInsert.begin();
    while (it != d->mIncidencesToInsert.end()) {
        const QString notebookUid = calendar()->notebook(*it);
        const Notebook::Ptr nb = notebook(notebookUid);
        if ((nb && nb->isRunTimeOnly()) ||
            (!nb && validateNotebooks())) {
            qCWarning(lcMkcal) << "invalid notebook - not adding incidence" << (*it)->uid();
            it = d->mIncidencesToInsert.erase(it);
        } else {
            ++it;
        }
    }
    it = d->mIncidencesToUpdate.begin();
    while (it != d->mIncidencesToUpdate.end()) {
        const QString notebookUid = calendar()->notebook(*it);
        const Notebook::Ptr nb = notebook(notebookUid);
        if ((nb && nb->isRunTimeOnly()) ||
            (!nb && validateNotebooks())) {
            qCWarning(lcMkcal) << "invalid notebook - not updating incidence" << (*it)->uid();
            it = d->mIncidencesToUpdate.erase(it);
        } else {
            ++it;
        }
    }
    bool success = storeIncidences(d->mIncidencesToInsert,
                                   d->mIncidencesToUpdate,
                                   d->mIncidencesToDelete,
                                   deleteAction);
    // TODO What if there were errors? Options: 1) rollback 2) best effort.
    d->mIncidencesToInsert.clear();
    d->mIncidencesToUpdate.clear();
    d->mIncidencesToDelete.clear();

    setFinished(!success, success ? "save completed" : "errors saving incidences");

    return success;
}

void ExtendedStorage::Private::clearLoaded()
{
    mRanges.clear();
    mIsRecurrenceLoaded = false;
    mIsUncompletedTodosLoaded = false;
    mIsCompletedTodosDateLoaded = false;
    mIsCompletedTodosCreatedLoaded = false;
    mIsDateLoaded = false;
    mIsCreatedLoaded = false;
    mIsFutureDateLoaded = false;
    mIsGeoDateLoaded = false;
    mIsGeoCreatedLoaded = false;
    mIsJournalsLoaded = false;
}

bool ExtendedStorage::getLoadDates(const QDate &start, const QDate &end,
                                   QDateTime *loadStart, QDateTime *loadEnd) const
{
    loadStart->setDate(start);   // may be null if start is not valid
    loadEnd->setDate(end);   // may be null if end is not valid

    // Check the need to load from db.
    for (const Range &loadedRange : d->mRanges) {
        bool startIsIn = loadedRange.contains(loadStart->date())
            || (loadedRange.mStart.isNull() && loadStart->date().isNull());
        bool endIsIn = loadedRange.contains(loadEnd->date().addDays(-1))
            || (loadedRange.mEnd.isNull() && loadEnd->date().isNull());
        if (startIsIn && endIsIn) {
            return false;
        } else if (startIsIn) {
            loadStart->setDate(loadedRange.mEnd.addDays(1));
        } else if (endIsIn) {
            loadEnd->setDate(loadedRange.mStart);
        }
    }
    if (loadStart->isValid() && loadEnd->isValid() && *loadStart >= *loadEnd) {
        return false;
    }

    if (loadStart->isValid()) {
        loadStart->setTimeZone(calendar()->timeZone());
    }
    if (loadEnd->isValid()) {
        loadEnd->setTimeZone(calendar()->timeZone());
    }

    qCDebug(lcMkcal) << "get load dates" << start << end << *loadStart << *loadEnd;

    return true;
}

void ExtendedStorage::addLoadedRange(const QDate &start, const QDate &end) const
{
    qCDebug(lcMkcal) << "set load dates" << start << end;

    Range range(start, end.addDays(-1));
    QList<Range>::Iterator it = d->mRanges.begin();
    while (it != d->mRanges.end()) {
        if (range < *it) {
            d->mRanges.insert(it, range);
            return;
        } else if (it->contains(end)) {
            if (start < *it) {
                it->mStart = start;
            }
            return;
        } else if (start < *it) {
            it = d->mRanges.erase(it);
        } else if (it->contains(start)) {
            range.mStart = it->mStart;
            it = d->mRanges.erase(it);
        } else {
            it++;
        }
    }
    d->mRanges.append(range);
}

bool ExtendedStorage::isRecurrenceLoaded() const
{
    return d->mIsRecurrenceLoaded;
}

void ExtendedStorage::setIsRecurrenceLoaded(bool loaded)
{
    d->mIsRecurrenceLoaded = loaded;
}

bool ExtendedStorage::isUncompletedTodosLoaded()
{
    return d->mIsUncompletedTodosLoaded;
}

void ExtendedStorage::setIsUncompletedTodosLoaded(bool loaded)
{
    d->mIsUncompletedTodosLoaded = loaded;
}

bool ExtendedStorage::isCompletedTodosDateLoaded()
{
    return d->mIsCompletedTodosDateLoaded;
}

void ExtendedStorage::setIsCompletedTodosDateLoaded(bool loaded)
{
    d->mIsCompletedTodosDateLoaded = loaded;
}

bool ExtendedStorage::isCompletedTodosCreatedLoaded()
{
    return d->mIsCompletedTodosCreatedLoaded;
}

void ExtendedStorage::setIsCompletedTodosCreatedLoaded(bool loaded)
{
    d->mIsCompletedTodosCreatedLoaded = loaded;
}

bool ExtendedStorage::isDateLoaded()
{
    return d->mIsDateLoaded;
}

void ExtendedStorage::setIsDateLoaded(bool loaded)
{
    d->mIsDateLoaded = loaded;
}

bool ExtendedStorage::isFutureDateLoaded()
{
    return d->mIsFutureDateLoaded;
}

void ExtendedStorage::setIsFutureDateLoaded(bool loaded)
{
    d->mIsFutureDateLoaded = loaded;
}

bool ExtendedStorage::isJournalsLoaded()
{
    return d->mIsJournalsLoaded;
}

void ExtendedStorage::setIsJournalsLoaded(bool loaded)
{
    d->mIsJournalsLoaded = loaded;
}

bool ExtendedStorage::isCreatedLoaded()
{
    return d->mIsCreatedLoaded;
}

void ExtendedStorage::setIsCreatedLoaded(bool loaded)
{
    d->mIsCreatedLoaded = loaded;
}

bool ExtendedStorage::isGeoDateLoaded()
{
    return d->mIsGeoDateLoaded;
}

void ExtendedStorage::setIsGeoDateLoaded(bool loaded)
{
    d->mIsGeoDateLoaded = loaded;
}

bool ExtendedStorage::isGeoCreatedLoaded()
{
    return d->mIsGeoCreatedLoaded;
}

void ExtendedStorage::setIsGeoCreatedLoaded(bool loaded)
{
    d->mIsGeoCreatedLoaded = loaded;
}

bool ExtendedStorage::runLoadOperation(const DBLoadOperation &dbop)
{
    if (d->mIsBatchLoading) {
        for (const DBLoadOperationWrapper &wrapper
                 : const_cast<const QList<DBLoadOperationWrapper>&>(d->mBatchLoadingOperations)) {
            if (*wrapper.dbop == dbop) {
                return false;
            }
        }
        d->mBatchLoadingOperations << DBLoadOperationWrapper(&dbop);
    }
    return !d->mIsBatchLoading;
}

bool ExtendedStorage::startBatchLoading()
{
    if (d->mIsBatchLoading) {
        qCWarning(lcMkcal) << "batch loading already active";
        return false;
    }
    d->mIsBatchLoading = true;
    return true;
}

bool ExtendedStorage::runBatchLoading()
{
    if (!d->mIsBatchLoading) {
        qCWarning(lcMkcal) << "batch loading not started";
        return false;
    }
    d->mIsBatchLoading = false;
    bool success = loadBatch(d->mBatchLoadingOperations);
    d->mBatchLoadingOperations.clear();
    return success;
}

bool ExtendedStorage::load()
{
    return loadIncidences(DBLoadAll{});
}

bool ExtendedStorage::load(const QString &uid, const QDateTime &recurrenceId)
{
    return loadIncidences(DBLoadByIds{uid, recurrenceId});
}

bool ExtendedStorage::loadSeries(const QString &uid)
{
    return loadIncidences(DBLoadSeries{uid});
}

bool ExtendedStorage::load(const QDate &date)
{
    if (date.isValid()) {
        return load(date, date.addDays(1));
    }

    return false;
}

bool ExtendedStorage::load(const QDate &start, const QDate &end)
{
    // We have no way to know if a recurring incidence
    // is happening within [start, end[, so load them all.
    if ((start.isValid() || end.isValid())
        && !loadRecurringIncidences()) {
        return false;
    }

    QDateTime loadStart;
    QDateTime loadEnd;
    bool success = true;
    if (getLoadDates(start, end, &loadStart, &loadEnd)) {
        success = loadIncidences(DBLoadByDateTimes{loadStart, loadEnd});
    }
    return success;
}

bool ExtendedStorage::loadNotebookIncidences(const QString &notebookUid)
{
    return loadIncidences(DBLoadByNotebook{notebookUid});
}

bool ExtendedStorage::loadJournals()
{
    return isJournalsLoaded() ? true : loadIncidences(DBLoadJournals{});
}

bool ExtendedStorage::loadPlainIncidences()
{
    return loadIncidences(DBLoadPlainIncidences{});
}

bool ExtendedStorage::loadRecurringIncidences()
{
    return isRecurrenceLoaded() ? true : loadIncidences(DBLoadRecursiveIncidences{});
}

bool ExtendedStorage::loadGeoIncidences()
{
    return loadIncidences(DBLoadGeoIncidences{});
}

bool ExtendedStorage::loadGeoIncidences(float geoLatitude, float geoLongitude,
                                      float diffLatitude, float diffLongitude)
{
    return loadIncidences(DBLoadGeoIncidences{geoLatitude, geoLongitude, diffLatitude, diffLongitude});
}

bool ExtendedStorage::loadAttendeeIncidences()
{
    return loadIncidences(DBLoadAttendeeIncidences{});
}

int ExtendedStorage::loadUncompletedTodos()
{
    return isUncompletedTodosLoaded() ? 0 : loadIncidences(DBLoadUncompletedTodos{});
}

int ExtendedStorage::loadCompletedTodos(bool hasDate, int limit, QDateTime *last)
{
    if (!last) {
        return -1;
    }

    int count = 0;
    if (hasDate && !isCompletedTodosDateLoaded()) {
        count = loadIncidences(DBLoadCompletedTodos{last},
                               last, limit, hasDate);
    } else if (!hasDate && !isCompletedTodosCreatedLoaded()) {
        count = loadIncidences(DBLoadCreatedAndCompletedTodos{last},
                               last, limit, hasDate);
    }
    return count;
}

int ExtendedStorage::loadJournals(int limit, QDateTime *last)
{
    if (!last)
        return -1;

    return isJournalsLoaded() ? 0 : loadIncidences(DBLoadJournals{last},
                                                   last, limit, true);
}

int ExtendedStorage::loadIncidences(bool hasDate, int limit, QDateTime *last)
{
    if (!last) {
        return -1;
    }

    int count = 0;
    if (hasDate && !isDateLoaded()) {
        count = loadIncidences(DBLoadByEnd{last},
                               last, limit, hasDate);
    } else if (!hasDate && !isCreatedLoaded()) {
        count = loadIncidences(DBLoadByCreated{last},
                               last, limit, hasDate);
    }
    return count;
}

int ExtendedStorage::loadFutureIncidences(int limit, QDateTime *last)
{
    if (!last)
        return -1;

    return isFutureDateLoaded() ? 0 : loadIncidences(DBLoadFuture{last},
                                                     last, limit, true);
}

int ExtendedStorage::loadGeoIncidences(bool hasDate, int limit, QDateTime *last)
{
    if (!last) {
        return -1;
    }

    int count = 0;
    if (hasDate && !isGeoDateLoaded()) {
        count = loadIncidences(DBLoadGeoIncidences{last},
                               last, limit, hasDate);
    } else if (!hasDate && !isGeoCreatedLoaded()) {
        count = loadIncidences(DBLoadCreatedGeoIncidences{last},
                               last, limit, hasDate);
    }
    return count;
}

int ExtendedStorage::loadContactIncidences(const Person &person, int limit, QDateTime *last)
{
    return loadIncidences(DBLoadByContacts{person, last}, last, limit, false);
}

bool ExtendedStorage::loadIncidenceInstance(const QString &instanceIdentifier)
{
    QString uid;
    QDateTime recId;
    // At the moment, from KCalendarCore, if the instance is an exception,
    // the instanceIdentifier will ends with yyyy-MM-ddTHH:mm:ss[Z|[+|-]HH:mm]
    // This is tested in tst_loadIncidenceInstance() to ensure that any
    // future breakage would be properly detected.
    if (instanceIdentifier.endsWith('Z')) {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 20);
        recId = QDateTime::fromString(instanceIdentifier.right(20), Qt::ISODate);
    } else if (instanceIdentifier.length() > 19
               && instanceIdentifier[instanceIdentifier.length() - 9] == 'T') {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 19);
        recId = QDateTime::fromString(instanceIdentifier.right(19), Qt::ISODate);
    } else if (instanceIdentifier.length() > 25
               && instanceIdentifier[instanceIdentifier.length() - 3] == ':') {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 25);
        recId = QDateTime::fromString(instanceIdentifier.right(25), Qt::ISODate);
    }
    if (!recId.isValid()) {
        uid = instanceIdentifier;
    }

    return load(uid, recId);
}

void ExtendedStorageObserver::storageModified(ExtendedStorage *storage,
                                              const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(info);
}

void ExtendedStorageObserver::storageFinished(ExtendedStorage *storage,
                                              bool error, const QString &info)
{
    Q_UNUSED(storage);
    Q_UNUSED(error);
    Q_UNUSED(info);
}

void ExtendedStorageObserver::storageUpdated(ExtendedStorage *storage,
                                             const Incidence::List &added,
                                             const Incidence::List &modified,
                                             const Incidence::List &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(added);
    Q_UNUSED(modified);
    Q_UNUSED(deleted);
}

void ExtendedStorageObserver::storageOpened(ExtendedStorage *storage)
{
    Q_UNUSED(storage);
}

void ExtendedStorageObserver::storageClosed(ExtendedStorage *storage)
{
    Q_UNUSED(storage);
}

void ExtendedStorageObserver::storageLoaded(ExtendedStorage *storage,
                                            const KCalendarCore::Incidence::List &incidences)
{
    Q_UNUSED(storage);
    Q_UNUSED(incidences);
}

void ExtendedStorage::Private::calendarModified(bool modified, Calendar *calendar)
{
    Q_UNUSED(calendar);
    // It's a bit hackish to do this here, but there is no
    // callback on calendar close and MemoryCalendar is calling
    // setModified(false) only on close(), so why not for the moment.
    // Since closing the calendar all stored incidences,
    // it's better to also clear all load flags.
    if (!modified) {
        clearLoaded();
    }
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

void ExtendedStorage::Private::calendarIncidenceDeleted(const Incidence::Ptr &incidence, const Calendar *calendar)
{
    Q_UNUSED(calendar);

    if (mIncidencesToInsert.contains(incidence->uid(), incidence)) {
        qCDebug(lcMkcal) << "removing incidence from inserted" << incidence->uid();
        mIncidencesToInsert.remove(incidence->uid(), incidence);
    } else {
        if (!mIncidencesToDelete.contains(incidence->uid(), incidence)) {
            qCDebug(lcMkcal) << "appending incidence" << incidence->uid() << "for database delete";
            mIncidencesToDelete.insert(incidence->uid(), incidence);
        }
    }
}

void ExtendedStorage::Private::calendarIncidenceAdditionCanceled(const Incidence::Ptr &incidence)
{
    if (mIncidencesToInsert.contains(incidence->uid())) {
        qCDebug(lcMkcal) << "duplicate - removing incidence from inserted" << incidence->uid();
        mIncidencesToInsert.remove(incidence->uid(), incidence);
    }
}

void ExtendedStorage::registerObserver(ExtendedStorageObserver *observer)
{
    if (!d->mObservers.contains(observer)) {
        d->mObservers.append(observer);
    }
}

void ExtendedStorage::unregisterObserver(ExtendedStorageObserver *observer)
{
    d->mObservers.removeAll(observer);
}

void ExtendedStorage::setModified(const QString &info)
{
    calendar()->close();
    d->clearLoaded();

    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageModified(this, info);
    }

    if (!loadNotebooks()) {
        qCWarning(lcMkcal) << "loading notebooks failed";
    }
}

void ExtendedStorage::setFinished(bool error, const QString &info)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageFinished(this, error, info);
    }
}

static bool isContaining(const QMultiHash<QString, Incidence::Ptr> &list, const Incidence *incidence)
{
    QMultiHash<QString, Incidence::Ptr>::ConstIterator it = list.find(incidence->uid());
    for (; it != list.constEnd(); ++it) {
        if ((*it)->recurrenceId() == incidence->recurrenceId()) {
            return true;
        }
    }
    return false;
}

void ExtendedStorage::setLoadOperationDone(const DBLoadOperation &dbop, int count, int limit)
{
    bool success = count >= 0;
    bool allLoaded = success && (count < limit || limit < 0);

    switch (dbop.type) {
    case LOAD_ALL:
        setIsRecurrenceLoaded(success);
        if (success) {
            addLoadedRange(QDate(), QDate());
        }
        return;
    case LOAD_BY_DATETIMES: {
        const DBLoadByDateTimes *op = static_cast<const DBLoadByDateTimes*>(&dbop);
        if (success) {
            addLoadedRange(op->start.date(), op->end.date());
        }
        if (op->start.isNull() && op->end.isNull()) {
            setIsRecurrenceLoaded(success);
        }
        return;
    }
    case LOAD_RECURSIVES:
        setIsRecurrenceLoaded(success);
        return;
    case LOAD_BY_UNCOMPLETED_TODOS:
        setIsUncompletedTodosLoaded(success);
        return;
    case LOAD_BY_COMPLETED_TODOS:
        setIsCompletedTodosDateLoaded(allLoaded);
        return;
    case LOAD_BY_CREATED_COMPLETED_TODOS:
        setIsCompletedTodosCreatedLoaded(allLoaded);
        return;
    case LOAD_JOURNALS: {
        const DBLoadJournals *op = static_cast<const DBLoadJournals*>(&dbop);
        setIsJournalsLoaded(op->last ? allLoaded : success);
        return;
    }
    case LOAD_BY_END:
        setIsDateLoaded(allLoaded);
        return;
    case LOAD_BY_CREATED:
        setIsCreatedLoaded(allLoaded);
        return;
    case LOAD_BY_FUTURE:
        setIsFutureDateLoaded(allLoaded);
        return;
    case LOAD_BY_GEO: {
        const DBLoadGeoIncidences *op = static_cast<const DBLoadGeoIncidences*>(&dbop);
        if (op->last) {
            setIsGeoDateLoaded(allLoaded);
        }
        return;
    }
    case LOAD_BY_CREATED_GEO:
        setIsGeoCreatedLoaded(allLoaded);
        return;
    default:
        return;
    }
}

void ExtendedStorage::setLoaded(const QMultiHash<QString, Incidence*> &incidences)
{
    Incidence::List added;
    calendar()->unregisterObserver(d);
    for (QMultiHash<QString, Incidence*>::ConstIterator it = incidences.constBegin();
         it != incidences.constEnd(); it++) {
        if (validateNotebooks() && !calendar()->hasValidNotebook(it.key())) {
            qCWarning(lcMkcal) << "not loading" << (*it)->uid() << it.key() << "(invalid notebook)";
            delete *it;
        } else if (isContaining(d->mIncidencesToInsert, *it) ||
                   isContaining(d->mIncidencesToUpdate, *it) ||
                   isContaining(d->mIncidencesToDelete, *it)) {
            qCWarning(lcMkcal) << "not loading" << (*it)->uid() << it.key() << "(local changes)";
            delete *it;
        } else {
            Incidence::Ptr incidence(*it);
            Incidence::Ptr old(calendar()->incidence(incidence->uid(), incidence->recurrenceId()));
            if (old && incidence->revision() > old->revision()) {
                // Move old to deleted and replace it with the new one.
                calendar()->deleteIncidence(old);
                old.clear();
            }
            if (!old && !calendar().staticCast<ExtendedCalendar>()->addIncidence(incidence, it.key())) {
                qCWarning(lcMkcal) << "cannot add incidence" << (*it)->uid() << "to notebook" << it.key();
            } else if (!old) {
                added << incidence;
            }
        }
    }
    calendar()->registerObserver(d);
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageLoaded(this, added);
    }
}

void ExtendedStorage::incidenceLoaded(const DBLoadOperationWrapper &wrapper, int count, int limit,
                                      const QMultiHash<QString, Incidence*> &incidences)
{
    setLoadOperationDone(*wrapper.dbop, count, limit);
    setLoaded(incidences);
    setFinished(count < 0, "load completed");
}

void ExtendedStorage::incidenceLoadedByBatch(const QList<DBLoadOperationWrapper> &wrappers,
                                             const QList<bool> &results,
                                             const QMultiHash<QString, Incidence*> &incidences)
{
    bool success = true;
    for (int i = 0; i < wrappers.count(); i++) {
        setLoadOperationDone(*wrappers[i].dbop, results[i] ? 0 : -1);
        success = success && results[i];
    }
    setLoaded(incidences);
    setFinished(!success, "batch load completed");
}

void ExtendedStorage::setOpened(const QList<Notebook*> notebooks, Notebook* defaultNb)
{
    const QStringList list = d->mNotebooks.keys();
    for (const QString &uid : list) {
        if (!calendar()->deleteNotebook(uid)) {
            qCDebug(lcMkcal) << "notebook" << uid << "already removed from calendar";
        }
    }
    d->mNotebooks.clear();
    d->mDefaultNotebook.clear();

    for (QList<Notebook*>::ConstIterator it = notebooks.constBegin();
         it != notebooks.constEnd(); it++) {
        Notebook::Ptr nb(*it);
        d->mNotebooks.insert(nb->uid(), nb);
        if (!calendar()->addNotebook(nb->uid(), nb->isVisible())
            && !calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
            qCWarning(lcMkcal) << "notebook" << nb->uid() << "already in calendar";
        }
    }
    if (defaultNb) {
        // Ownership of defaultNb already done since it is also in the notebook list.
        d->mDefaultNotebook = notebook(defaultNb->uid());
        if (!calendar()->setDefaultNotebook(defaultNb->uid())) {
            qCWarning(lcMkcal) << "cannot set default notebook" << defaultNb->uid() << "in calendar";
        }
    }

    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageOpened(this);
    }
}

void ExtendedStorage::setClosed()
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageClosed(this);
    }
}

void ExtendedStorage::setUpdated(const Incidence::List &added,
                                 const Incidence::List &modified,
                                 const Incidence::List &deleted)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageUpdated(this, added, modified, deleted);
    }
}

bool ExtendedStorage::addNotebook(const Notebook::Ptr &nb)
{
    if (!nb || d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!nb->isRunTimeOnly() && !modifyNotebook(nb, DBInsert)) {
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

    if (!nb->isRunTimeOnly() && !modifyNotebook(nb, DBUpdate)) {
        return false;
    }
    if (!calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "cannot update notebook" << nb->uid() << "in calendar";
    }

    return true;
}

bool ExtendedStorage::deleteNotebook(const Notebook::Ptr &nb)
{
    if (!nb || !d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!nb->isRunTimeOnly() && !modifyNotebook(nb, DBDelete)) {
        return false;
    }
    d->mNotebooks.remove(nb->uid());
    if (!calendar()->deleteNotebook(nb->uid())) {
        qCWarning(lcMkcal) << "notebook" << nb->uid() << "already deleted from calendar";
    }
    if (d->mDefaultNotebook == nb) {
        d->mDefaultNotebook = Notebook::Ptr();
    }
    const Incidence::List toDelete(calendar()->incidences(nb->uid()));
    calendar()->unregisterObserver(d);
    for (const Incidence::Ptr incidence : toDelete) {
        if (incidence->hasRecurrenceId()) {
            // May have already been removed by the parent.
            if (calendar()->incidence(incidence->uid(), incidence->recurrenceId())) {
                calendar()->deleteIncidence(incidence);
            }
        } else {
            calendar()->deleteIncidence(incidence);
        }
    }
    calendar()->registerObserver(d);

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

Notebook::Ptr ExtendedStorage::notebook(const QString &uid) const
{
    return d->mNotebooks.value(uid);
}

void ExtendedStorage::setValidateNotebooks(bool validateNotebooks)
{
    d->mValidateNotebooks = validateNotebooks;
}

bool ExtendedStorage::validateNotebooks() const
{
    return d->mValidateNotebooks;
}

bool ExtendedStorage::isValidNotebook(const QString &notebookUid) const
{
    const Notebook::Ptr nb = notebook(notebookUid);
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

Notebook::Ptr ExtendedStorage::createDefaultNotebook(QString name, QString color)
{
    qCWarning(lcMkcal) << "Deprecated call to createDefaultNotebook(),"
                       << "create a notebook and make it default with setDefaultNotebook() instead";
    if (name.isEmpty())
        name = "Default";
    if (color.isEmpty())
        color = "#0000FF";
    Notebook::Ptr nbDefault(new Notebook(name, QString(), color));
    return setDefaultNotebook(nbDefault) ? nbDefault : Notebook::Ptr();
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
