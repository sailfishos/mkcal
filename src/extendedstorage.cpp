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
#include "logging_p.h"

#include <QtCore/QUuid>

#include <KCalendarCore/Exceptions>
#include <KCalendarCore/Calendar>
using namespace KCalendarCore;

#ifdef TIMED_SUPPORT
# include <timed-qt5/interface.h>
# include <timed-qt5/event-declarations.h>
# include <timed-qt5/exception.h>
# include <QtCore/QMap>
# include <QtDBus/QDBusReply>
using namespace Maemo;
static const QLatin1String RESET_ALARMS_CMD("invoker --type=generic -n /usr/bin/mkcaltool --reset-alarms");
#endif

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
          mIsRecurrenceLoaded(false),
          mIsUncompletedTodosLoaded(false),
          mIsCompletedTodosDateLoaded(false),
          mIsCompletedTodosCreatedLoaded(false),
          mIsDateLoaded(false),
          mIsCreatedLoaded(false),
          mIsFutureDateLoaded(false),
          mIsGeoDateLoaded(false),
          mIsGeoCreatedLoaded(false),
          mIsJournalsLoaded(false)
    {}

    bool mValidateNotebooks;
    QList<Range> mRanges;
    bool mIsRecurrenceLoaded;
    bool mIsUncompletedTodosLoaded;
    bool mIsCompletedTodosDateLoaded;
    bool mIsCompletedTodosCreatedLoaded;
    bool mIsDateLoaded;
    bool mIsCreatedLoaded;
    bool mIsFutureDateLoaded;
    bool mIsGeoDateLoaded;
    bool mIsGeoCreatedLoaded;
    bool mIsJournalsLoaded;
    QList<ExtendedStorageObserver *> mObservers;
    QHash<QString, Notebook> mNotebooks; // uid to notebook
    QString mDefaultNotebookId;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToInsert;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToUpdate;
    QMultiHash<QString, Incidence::Ptr> mIncidencesToDelete;

    // Internal Calendar Listener Methods //
    virtual void calendarModified(bool modified, Calendar *calendar) override;
    virtual void calendarIncidenceAdded(const Incidence::Ptr &incidence) override;
    virtual void calendarIncidenceChanged(const Incidence::Ptr &incidence) override;
    virtual void calendarIncidenceDeleted(const Incidence::Ptr &incidence, const Calendar *calendar) override;
    virtual void calendarIncidenceAdditionCanceled(const Incidence::Ptr &incidence) override;

#if defined(TIMED_SUPPORT)
    // These alarm methods are used to communicate with an external
    // daemon, like timed, to bind Incidence::Alarm with the system notification.
    void clearAlarms(const Incidence::Ptr &incidence);
    void clearAlarms(const Incidence::List &incidences);
    void clearAlarms(const QString &notebookUid);
    void setAlarms(const Incidence::List &incidences, const Calendar::Ptr &calendar);
    void resetAlarms(const Incidence::List &incidences, const Calendar::Ptr &calendar);

    void setAlarmsForNotebook(const Incidence::List &incidences, const QString &notebookUid);
    void setAlarms(const Incidence::Ptr &incidence, const QString &nbuid, Timed::Event::List &events, const QDateTime &now);
    void commitEvents(Timed::Event::List &events);
#endif
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
    QList<Notebook> notebooks;
    if (!loadNotebooks(&notebooks, &d->mDefaultNotebookId)) {
        qCWarning(lcMkcal) << "loading notebooks failed";
        return false;
    }

    for (QList<Notebook>::ConstIterator it = notebooks.constBegin();
         it != notebooks.constEnd(); it++) {
        d->mNotebooks.insert(it->uid(), *it);
        if (!calendar()->addNotebook(it->uid(), it->isVisible())
            && !calendar()->updateNotebook(it->uid(), it->isVisible())) {
            qCWarning(lcMkcal) << "notebook" << it->uid() << "already in calendar";
        }
    }
    if (!d->mDefaultNotebookId.isEmpty() && !calendar()->setDefaultNotebook(d->mDefaultNotebookId)) {
        qCWarning(lcMkcal) << "cannot set notebook" << d->mDefaultNotebookId << "as default in calendar";
    }
    if (notebooks.isEmpty()) {
        qCDebug(lcMkcal) << "Storage is empty, initializing";
        if (!setDefaultNotebook(Notebook(QString::fromLatin1("Default"), QString()))) {
            qCWarning(lcMkcal) << "Unable to add a default notebook.";
            return false;
        }
    }
    if (timeZone().isValid()) {
        calendar()->setTimeZone(timeZone());
    }

    return true;
}

bool ExtendedStorage::close()
{
    clearLoaded();

    d->mNotebooks.clear();
    d->mDefaultNotebookId = QString();
    d->mIncidencesToInsert.clear();
    d->mIncidencesToUpdate.clear();
    d->mIncidencesToDelete.clear();

    return true;
}

bool ExtendedStorage::save()
{
    return save(ExtendedStorage::MarkDeleted);
}

bool ExtendedStorage::save(ExtendedStorage::DeleteAction deleteAction)
{
    QMultiHash<QString, Incidence::Ptr> added;
    QMultiHash<QString, Incidence::Ptr> modified;
    QMultiHash<QString, Incidence::Ptr> deleted;

    // Notice : we allow to save/delete incidences in a read-only
    // notebook. The read-only flag is a hint only. This allows
    // to update a marked as read-only notebook to reflect external
    // changes.
    QHash<QString, Incidence::Ptr>::ConstIterator it;
    for (it = d->mIncidencesToInsert.constBegin();
         it != d->mIncidencesToInsert.constEnd(); it++) {
        const QString notebookUid = calendar()->notebook(*it);
        const Notebook notebook = d->mNotebooks.value(notebookUid);
        if (notebook.isRunTimeOnly() || !validateNotebooks() || notebook.isValid()) {
            added.insert(notebookUid, *it);
        } else {
            qCWarning(lcMkcal) << "invalid notebook - not saving incidence" << (*it)->uid();
        }
    }
    d->mIncidencesToInsert.clear();
    for (it = d->mIncidencesToUpdate.constBegin();
         it != d->mIncidencesToUpdate.constEnd(); it++) {
        const QString notebookUid = calendar()->notebook(*it);
        const Notebook notebook = d->mNotebooks.value(notebookUid);
        if (notebook.isRunTimeOnly() || !validateNotebooks() || notebook.isValid()) {
            modified.insert(notebookUid, *it);
        } else {
            qCWarning(lcMkcal) << "invalid notebook - not updating incidence" << (*it)->uid();
        }
    }
    d->mIncidencesToUpdate.clear();
    for (it = d->mIncidencesToDelete.constBegin();
         it != d->mIncidencesToDelete.constEnd(); it++) {
        deleted.insert(calendar()->notebook(*it), *it);
    }
    d->mIncidencesToDelete.clear();

    return storeIncidences(added, modified, deleted, deleteAction);
}

void ExtendedStorage::clearLoaded()
{
    d->mRanges.clear();
    d->mIsRecurrenceLoaded = false;
    d->mIsUncompletedTodosLoaded = false;
    d->mIsCompletedTodosDateLoaded = false;
    d->mIsCompletedTodosCreatedLoaded = false;
    d->mIsDateLoaded = false;
    d->mIsCreatedLoaded = false;
    d->mIsFutureDateLoaded = false;
    d->mIsGeoDateLoaded = false;
    d->mIsGeoCreatedLoaded = false;
    d->mIsJournalsLoaded = false;
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

void ExtendedStorage::Private::calendarIncidenceDeleted(const Incidence::Ptr &incidence, const Calendar *calendar)
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

void ExtendedStorageObserver::incidenceLoaded(ExtendedStorage *storage,
                                              const QMultiHash<QString, Incidence::Ptr> &incidences)
{
    Q_UNUSED(storage);
    Q_UNUSED(incidences);
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
    const QStringList list = d->mNotebooks.keys();
    for (const QString &uid : list) {
        if (!calendar()->deleteNotebook(uid)) {
            qCDebug(lcMkcal) << "notebook" << uid << "already removed from calendar";
        }
    }
    calendar()->close();
    ExtendedStorage::close();
    ExtendedStorage::open();

    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageModified(this, info);
    }
}

void ExtendedStorage::setFinished(bool error, const QString &info)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageFinished(this, error, info);
    }
}

void ExtendedStorage::setUpdated(const Incidence::List &added,
                                 const Incidence::List &modified,
                                 const Incidence::List &deleted)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageUpdated(this, added, modified, deleted);
    }
#if defined(TIMED_SUPPORT)
    if (!added.isEmpty())
        d->setAlarms(added, calendar());
    if (!modified.isEmpty())
        d->resetAlarms(modified, calendar());
    if (!deleted.isEmpty())
        d->clearAlarms(deleted);
#endif
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

void ExtendedStorage::setLoaded(const QMultiHash<QString, Incidence::Ptr> &incidences)
{
    calendar()->unregisterObserver(d);
    for(QMultiHash<QString, Incidence::Ptr>::ConstIterator it = incidences.constBegin();
        it != incidences.constEnd(); it++) {
        const Incidence::Ptr &incidence = it.value();
        bool added = true;
        bool hasNotebook = calendar()->hasValidNotebook(it.key());
        // Cannot use .contains(incidence->uid(), incidence) here, like
        // in the rest of the file, since incidence here is a new one
        // returned by the selectComponents() that cannot by design be already
        // in the multihash tables.
        if (isContaining(d->mIncidencesToInsert, incidence) ||
            isContaining(d->mIncidencesToUpdate, incidence) ||
            isContaining(d->mIncidencesToDelete, incidence) ||
            (validateNotebooks() && !hasNotebook)) {
            qCWarning(lcMkcal) << "not loading" << incidence->uid() << it.key()
                               << (!hasNotebook ? "(invalidated notebook)" : "(local changes)");
            added = false;
        } else {
            Incidence::Ptr old(calendar()->incidence(incidence->uid(),
                                                     incidence->recurrenceId()));
            if (old) {
                if (incidence->revision() > old->revision()) {
                    calendar()->deleteIncidence(old);   // move old to deleted
                    // and replace it with the new one.
                } else {
                    added = false;
                }
            }
        }
        if (added && !calendar().staticCast<ExtendedCalendar>()->addIncidence(incidence, it.key())) {
            qCWarning(lcMkcal) << "cannot add incidence" << incidence->uid()
                               << "to notebook" << it.key();
        }
    }
    calendar()->registerObserver(d);

    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->incidenceLoaded(this, incidences);
    }
}

bool ExtendedStorage::addNotebook(const Notebook &nb)
{
    if (!nb.isValid() || d->mNotebooks.contains(nb.uid())) {
        return false;
    }

    if (!nb.isRunTimeOnly() && !modifyNotebook(nb, DBInsert)) {
        return false;
    }
    d->mNotebooks.insert(nb.uid(), nb);

    if (!calendar()->addNotebook(nb.uid(), nb.isVisible())
        && !calendar()->updateNotebook(nb.uid(), nb.isVisible())) {
        qCWarning(lcMkcal) << "notebook" << nb.uid() << "already in calendar";
    }

    return true;
}

bool ExtendedStorage::updateNotebook(const Notebook &nb)
{
    if (!nb.isValid() || !d->mNotebooks.contains(nb.uid())) {
        return false;
    }

    if (!nb.isRunTimeOnly() && !modifyNotebook(nb, DBUpdate)) {
        return false;
    }
    d->mNotebooks.insert(nb.uid(), nb);

    bool wasVisible = calendar()->isVisible(nb.uid());
    if (!calendar()->updateNotebook(nb.uid(), nb.isVisible())) {
        qCWarning(lcMkcal) << "cannot update notebook" << nb.uid() << "in calendar";
    }

#if defined(TIMED_SUPPORT)
    if (!nb.isRunTimeOnly()) {
        if (wasVisible && !nb.isVisible()) {
            d->clearAlarms(nb.uid());
        } else if (!wasVisible && nb.isVisible()) {
            Incidence::List list;
            if (allIncidences(&list, nb.uid())) {
                d->setAlarmsForNotebook(list, nb.uid());
            }
        }
    }
#endif

    return true;
}

bool ExtendedStorage::deleteNotebook(const QString &nbid)
{
    if (!d->mNotebooks.contains(nbid)) {
        return true;
    }
    const Notebook &nb = d->mNotebooks.value(nbid);

    if (!nb.isRunTimeOnly() && !modifyNotebook(d->mNotebooks.value(nbid), DBDelete)) {
        return false;
    }

    calendar()->unregisterObserver(d);
    const Incidence::List list = calendar()->incidences(nbid);
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
    if (!calendar()->deleteNotebook(nbid)) {
        qCWarning(lcMkcal) << "notebook" << nbid << "already deleted from calendar";
    }
#if defined(TIMED_SUPPORT)
    d->clearAlarms(nbid);
#endif

    d->mNotebooks.remove(nbid);

    if (d->mDefaultNotebookId == nbid) {
        d->mDefaultNotebookId = QString();
    }

    return true;
}

bool ExtendedStorage::setDefaultNotebook(const Notebook &nb)
{
    d->mDefaultNotebookId = nb.uid();

    if ((d->mNotebooks.contains(nb.uid()) && !updateNotebook(nb))
        || (!d->mNotebooks.contains(nb.uid()) && !addNotebook(nb))) {
        return false;
    }

    if (!calendar()->setDefaultNotebook(nb.uid())) {
        qCWarning(lcMkcal) << "cannot set notebook" << nb.uid() << "as default in calendar";
    }

    return true;
}

QString ExtendedStorage::defaultNotebookId() const
{
    return d->mDefaultNotebookId;
}

QList<Notebook> ExtendedStorage::notebooks() const
{
    return d->mNotebooks.values();
}

Notebook ExtendedStorage::notebook(const QString &uid) const
{
    return d->mNotebooks.value(uid);
}

bool ExtendedStorage::containsNotebook(const QString &uid) const
{
    return d->mNotebooks.contains(uid);
}

void ExtendedStorage::setValidateNotebooks(bool validateNotebooks)
{
    d->mValidateNotebooks = validateNotebooks;
}

bool ExtendedStorage::validateNotebooks() const
{
    return d->mValidateNotebooks;
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

#if defined(TIMED_SUPPORT)
// Todo: move this into a service plugin that is a ExtendedStorageObserver.
void ExtendedStorage::Private::resetAlarms(const Incidence::List &incidences,
                                           const Calendar::Ptr &calendar)
{
    clearAlarms(incidences);
    setAlarms(incidences, calendar);
}

void ExtendedStorage::Private::setAlarms(const Incidence::List &incidences,
                                         const Calendar::Ptr &calendar)
{
    const QDateTime now = QDateTime::currentDateTime();
    Timed::Event::List events;
    foreach (const Incidence::Ptr incidence, incidences) {
        // The incidence from the list must be in the calendar and in a notebook.
        const QString &nbuid = calendar->notebook(incidence->uid());
        if (!calendar->isVisible(incidence) || nbuid.isEmpty()) {
            continue;
        }
        setAlarms(incidence, nbuid, events, now);
    }
    commitEvents(events);
}

void ExtendedStorage::Private::clearAlarms(const Incidence::Ptr &incidence)
{
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["uid"] = incidence->uid();

    Timed::Interface timed;
    if (!timed.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << incidence->uid()
                           << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-")
                           << "alarm interface is not valid" << timed.lastError();
        return;
    }
    QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    if (!reply.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << incidence->uid()
                           << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-")
                           << timed.lastError();
        return;
    }

    const QList<QVariant> &result = reply.value();
    for (int i = 0; i < result.size(); i++) {
        uint32_t cookie = result[i].toUInt();
        // We got a list of all alarm matching UID of this incidence
        // - single event -> delete the alarm
        // - recurring parent event -> the recurs() case, delete if
        //   recurrenceId attribute is empty (thus invalid QDateTime)
        // - recurring exception event -> the hasRecurrenceId() case,
        //   delete if the recurrenceId attribute is matching in terms of QDateTime.
        if (incidence->recurs() || incidence->hasRecurrenceId()) {
            QDBusReply<QMap<QString, QVariant> > attributesReply = timed.query_attributes_sync(cookie);
            const QMap<QString, QVariant> attributeMap = attributesReply.value();
            const QVariant recurrenceId = attributeMap.value("recurrenceId", QVariant(QString()));
            QDateTime recid = QDateTime::fromString(recurrenceId.toString(), Qt::ISODate);
            if (incidence->recurrenceId() != recid) {
                continue;
            }
        }
        qCDebug(lcMkcal) << "removing alarm" << cookie << incidence->uid()
                         << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-");
        QDBusReply<bool> reply = timed.cancel_sync(cookie);
        if (!reply.isValid() || !reply.value()) {
            qCWarning(lcMkcal) << "cannot remove alarm" << cookie << incidence->uid()
                               << (incidence->hasRecurrenceId() ? incidence->recurrenceId().toString(Qt::ISODate) : "-")
                               << reply.value() << timed.lastError();
        }
    }
}

void ExtendedStorage::Private::clearAlarms(const Incidence::List &incidences)
{
    foreach (const Incidence::Ptr incidence, incidences) {
        clearAlarms(incidence);
    }
}

void ExtendedStorage::Private::clearAlarms(const QString &notebookUid)
{
    QMap<QString, QVariant> map;
    map["APPLICATION"] = "libextendedkcal";
    map["notebook"] = notebookUid;

    Timed::Interface timed;
    if (!timed.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << notebookUid
                 << "alarm interface is not valid" << timed.lastError();
        return;
    }
    QDBusReply<QList<QVariant> > reply = timed.query_sync(map);
    if (!reply.isValid()) {
        qCWarning(lcMkcal) << "cannot clear alarms for" << notebookUid << timed.lastError();
        return;
    }
    const QList<QVariant> &result = reply.value();
    for (int i = 0; i < result.size(); i++) {
        uint32_t cookie = result[i].toUInt();
        qCDebug(lcMkcal) << "removing alarm" << cookie << notebookUid;
        QDBusReply<bool> reply = timed.cancel_sync(cookie);
        if (!reply.isValid() || !reply.value()) {
            qCWarning(lcMkcal) << "cannot remove alarm" << cookie << notebookUid;
        }
    }
}

void ExtendedStorage::Private::setAlarmsForNotebook(const Incidence::List &incidences, const QString &notebookUid)
{
    const QDateTime now = QDateTime::currentDateTime();
    // list of all timed events
    Timed::Event::List events;
    foreach (const Incidence::Ptr incidence, incidences) {
        setAlarms(incidence, notebookUid, events, now);
    }
    commitEvents(events);
}

void ExtendedStorage::Private::setAlarms(const Incidence::Ptr &incidence,
                                         const QString &nbuid,
                                         Timed::Event::List &events,
                                         const QDateTime &now)
{
    if (incidence->status() == Incidence::StatusCanceled) {
        return;
    }

    const Alarm::List alarms = incidence->alarms();
    foreach (const Alarm::Ptr alarm, alarms) {
        if (!alarm->enabled()) {
            continue;
        }

        QDateTime preTime = now;
        if (incidence->recurs()) {
            QDateTime nextRecurrence = incidence->recurrence()->getNextDateTime(now);
            if (nextRecurrence.isValid() && alarm->startOffset().asSeconds() < 0) {
                if (now.addSecs(::abs(alarm->startOffset().asSeconds())) >= nextRecurrence) {
                    preTime = nextRecurrence;
                }
            }
        }

        QDateTime alarmTime = alarm->nextTime(preTime, true);
        if (!alarmTime.isValid()) {
            continue;
        }

        if (now.addSecs(60) > alarmTime) {
            // don't allow alarms at the same minute -> take next alarm if so
            alarmTime = alarm->nextTime(preTime.addSecs(60), true);
            if (!alarmTime.isValid()) {
                continue;
            }
        }
        Timed::Event &e = events.append();
        e.setUserModeFlag();
        e.setMaximalTimeoutSnoozeCounter(2);
        e.setTicker(alarmTime.toUTC().toTime_t());
        // The code'll crash (=exception) iff the content is empty. So
        // we have to check here.
        QString s;

        s = incidence->summary();
        // Timed braindeath: Required field, BUT if empty, it asserts
        if (s.isEmpty()) {
            s = ' ';
        }
        e.setAttribute("TITLE", s);
        e.setAttribute("PLUGIN", "libCalendarReminder");
        e.setAttribute("APPLICATION", "libextendedkcal");
        //e.setAttribute( "translation", "organiser" );
        // This really has to exist or code is badly broken
        Q_ASSERT(!incidence->uid().isEmpty());
        e.setAttribute("uid", incidence->uid());
#ifndef QT_NO_DEBUG_OUTPUT //Helps debuggin
        e.setAttribute("alarmtime", alarmTime.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
#endif
        if (!incidence->location().isEmpty()) {
            e.setAttribute("location", incidence->location());
        }
        if (incidence->recurs()) {
            e.setAttribute("recurs", "true");
            Timed::Event::Action &a = e.addAction();
            a.runCommand(QString("%1 %2 %3")
                         .arg(RESET_ALARMS_CMD)
                         .arg(nbuid)
                         .arg(incidence->uid()));
            a.whenServed();
        }

        // TODO - consider this how it should behave for recurrence
        if ((incidence->type() == Incidence::TypeTodo)) {
            Todo::Ptr todo = incidence.staticCast<Todo>();

            if (todo->hasDueDate()) {
                e.setAttribute("time", todo->dtDue(true).toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            }
            e.setAttribute("type", "todo");
        } else if (incidence->dtStart().isValid()) {
            QDateTime eventStart;

            if (incidence->recurs()) {
                // assuming alarms not later than event start
                eventStart = incidence->recurrence()->getNextDateTime(alarmTime.addSecs(-60));
            } else {
                eventStart = incidence->dtStart();
            }
            e.setAttribute("time", eventStart.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            e.setAttribute("startDate", eventStart.toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            if (incidence->endDateForStart(eventStart).isValid()) {
                e.setAttribute("endDate", incidence->endDateForStart(eventStart).toTimeSpec(Qt::OffsetFromUTC).toString(Qt::ISODate));
            }
            e.setAttribute("type", "event");
        }

        if (incidence->hasRecurrenceId()) {
            e.setAttribute("recurrenceId", incidence->recurrenceId().toString(Qt::ISODate));
        }
        e.setAttribute("notebook", nbuid);

        if (alarm->type() == Alarm::Procedure) {
            QString prog = alarm->programFile();
            if (!prog.isEmpty()) {
                Timed::Event::Action &a = e.addAction();
                a.runCommand(prog + " " + alarm->programArguments());
                a.whenFinalized();
            }
        } else {
            e.setReminderFlag();
            e.setAlignedSnoozeFlag();
        }
    }
}

void ExtendedStorage::Private::commitEvents(Timed::Event::List &events)
{
    if (events.count() > 0) {
        Timed::Interface timed;
        if (!timed.isValid()) {
            qCWarning(lcMkcal) << "cannot set alarm for incidence: "
                               << "alarm interface is not valid" << timed.lastError();
            return;
        }
        QDBusReply < QList<QVariant> > reply = timed.add_events_sync(events);
        if (reply.isValid()) {
            foreach (QVariant v, reply.value()) {
                bool ok = true;
                uint cookie = v.toUInt(&ok);
                if (ok && cookie) {
                    qCDebug(lcMkcal) << "added alarm: " << cookie;
                } else {
                    qCWarning(lcMkcal) << "failed to add alarm";
                }
            }
        } else {
            qCWarning(lcMkcal) << "failed to add alarms: " << reply.error().message();
        }
    } else {
        qCDebug(lcMkcal) << "No alarms to send";
    }
}
#endif
