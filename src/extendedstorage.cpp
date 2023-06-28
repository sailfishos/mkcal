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
#include "alarmhandler_p.h"
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
class mKCal::ExtendedStorage::Private: public AlarmHandler
{
public:
    Private(ExtendedStorage *storage, bool validateNotebooks)
        : mStorage(storage)
        , mValidateNotebooks(validateNotebooks)
        , mIsRecurrenceLoaded(false)
    {}

    ~Private()
    {}

    ExtendedStorage *mStorage;
    bool mValidateNotebooks;
    QList<Range> mRanges;
    bool mIsRecurrenceLoaded;
    QList<ExtendedStorageObserver *> mObservers;
    QHash<QString, Notebook::Ptr> mNotebooks; // uid to notebook
    Notebook::Ptr mDefaultNotebook;

    bool clear();

    Incidence::List incidencesWithAlarms(const QString &notebookUid,
                                         const QString &uid) override;
};

bool ExtendedStorage::Private::clear()
{
    mRanges.clear();
    mIsRecurrenceLoaded = false;
    mNotebooks.clear();
    mDefaultNotebook = Notebook::Ptr();

    return true;
}

Incidence::List ExtendedStorage::Private::incidencesWithAlarms(const QString &notebookUid, const QString &uid)
{
    Incidence::List list;
    if (!mNotebooks.contains(notebookUid)
        || !mNotebooks.value(notebookUid)->isVisible()) {
        return list;
    }

    // The assumption on wether the incidences are already in memory
    // or not is explained in AlarmHandler::incidencesWithAlarms() documentation
    // and is reminded here for completeness.
    if (uid.isEmpty()) {
        // This case is called when changing notebook visibility.
        // There is no guarantee that the calendar contains all incidences.
        Incidence::List all;
        mStorage->allIncidences(&all, notebookUid);
        for (Incidence::List::ConstIterator it = all.constBegin();
             it != all.constEnd(); it++) {
            // Recurring incidences may not have alarms but their exception may.
            if ((*it)->hasEnabledAlarms() || (*it)->recurs()) {
                list.append(*it);
            }
        }
    } else {
        // This case is called when modifying (insertion, update or deletion)
        // one or several incidences. The series is guaranteed to be already
        // in memory.
        Incidence::Ptr parent = mStorage->calendar()->incidence(uid);
        if (!parent) {
            return list;
        }
        if (parent->hasEnabledAlarms()) {
            list.append(parent);
        }
        for (const Incidence::Ptr &exception : mStorage->calendar()->instances(parent)) {
            if (exception->hasEnabledAlarms() || parent->hasEnabledAlarms()) {
                list.append(exception);
            }
        }
    }
    return list;
}
//@endcond

ExtendedStorage::ExtendedStorage(const ExtendedCalendar::Ptr &cal, bool validateNotebooks)
    : CalStorage(cal),
      d(new ExtendedStorage::Private(this, validateNotebooks))
{
    cal->registerObserver(this);
}

ExtendedStorage::~ExtendedStorage()
{
    calendar()->unregisterObserver(this);
    delete d;
}

bool ExtendedStorage::close()
{
    return d->clear();
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

bool ExtendedStorage::loadSeries(const QString &uid)
{
    qCWarning(lcMkcal) << "deprecated call to loadSeries(), use load() instead.";
    return load(uid);
}

bool ExtendedStorage::load(const QString &uid, const QDateTime &recurrenceId)
{
    Q_UNUSED(recurrenceId);

    qCWarning(lcMkcal) << "deprecated call to load(uid, recid), use load(uid) instead.";
    return load(uid);
}

bool ExtendedStorage::loadIncidenceInstance(const QString &instanceIdentifier)
{
    QString uid;
    // At the moment, from KCalendarCore, if the instance is an exception,
    // the instanceIdentifier will ends with yyyy-MM-ddTHH:mm:ss[Z|[+|-]HH:mm]
    // This is tested in tst_loadIncidenceInstance() to ensure that any
    // future breakage would be properly detected.
    if (instanceIdentifier.endsWith('Z')) {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 20);
    } else if (instanceIdentifier.length() > 19
               && instanceIdentifier[instanceIdentifier.length() - 9] == 'T') {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 19);
    } else if (instanceIdentifier.length() > 25
               && instanceIdentifier[instanceIdentifier.length() - 3] == ':') {
        uid = instanceIdentifier.left(instanceIdentifier.length() - 25);
    } else {
        uid = instanceIdentifier;
    }

    // Even if we're looking for a specific incidence instance, we load all
    // the series for recurring event, to avoid orphaned exceptions in the
    // calendar or recurring events without their exceptions.
    return load(uid);
}

bool ExtendedStorage::load(const QDate &date)
{
    return date.isValid() && load(date, date.addDays(1));
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
                                             const KCalendarCore::Incidence::List &added,
                                             const KCalendarCore::Incidence::List &modified,
                                             const KCalendarCore::Incidence::List &deleted)
{
    Q_UNUSED(storage);
    Q_UNUSED(added);
    Q_UNUSED(modified);
    Q_UNUSED(deleted);
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

void ExtendedStorage::emitStorageModified(const QString &info)
{
    const QStringList list = d->mNotebooks.keys();
    for (const QString &uid : list) {
        if (!calendar()->deleteNotebook(uid)) {
            qCDebug(lcMkcal) << "notebook" << uid << "already removed from calendar";
        }
    }
    calendar()->close();
    d->clear();
    if (!loadNotebooks()) {
        qCWarning(lcMkcal) << "loading notebooks failed";
    }

    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageModified(this, info);
    }
}

void ExtendedStorage::emitStorageFinished(bool error, const QString &info)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageFinished(this, error, info);
    }
}

void ExtendedStorage::emitStorageUpdated(const KCalendarCore::Incidence::List &added,
                                         const KCalendarCore::Incidence::List &modified,
                                         const KCalendarCore::Incidence::List &deleted)
{
    foreach (ExtendedStorageObserver *observer, d->mObservers) {
        observer->storageUpdated(this, added, modified, deleted);
    }

    QSet<QPair<QString, QString>> uids;
    for (const Incidence::Ptr &incidence : added + modified + deleted) {
        uids.insert(QPair<QString, QString>(calendar()->notebook(incidence),
                                            incidence->uid()));
    }
    d->setupAlarms(uids);
}

bool ExtendedStorage::addNotebook(const Notebook::Ptr &nb)
{
    if (!nb || d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!nb->isRunTimeOnly() && !insertNotebook(nb)) {
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

    if (!nb->isRunTimeOnly() && !modifyNotebook(nb)) {
        return false;
    }

    bool wasVisible = calendar()->isVisible(nb->uid());
    if (!calendar()->updateNotebook(nb->uid(), nb->isVisible())) {
        qCWarning(lcMkcal) << "cannot update notebook" << nb->uid() << "in calendar";
        return false;
    }

    if (wasVisible && !nb->isVisible()) {
        d->clearAlarms(nb->uid());
    } else if (!wasVisible && nb->isVisible()) {
        d->setupAlarms(nb->uid());
    }

    return true;
}

bool ExtendedStorage::deleteNotebook(const Notebook::Ptr &nb)
{
    if (!nb || !d->mNotebooks.contains(nb->uid())) {
        return false;
    }

    if (!nb->isRunTimeOnly() && !eraseNotebook(nb)) {
        return false;
    }

    // remove all notebook incidences from calendar
    calendar()->unregisterObserver(this);
    for (const Incidence::Ptr &toDelete : calendar()->incidences(nb->uid())) {
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
    if (!calendar()->deleteNotebook(nb->uid())) {
        qCWarning(lcMkcal) << "notebook" << nb->uid() << "already deleted from calendar";
    }
    calendar()->registerObserver(this);

    d->mNotebooks.remove(nb->uid());

    if (d->mDefaultNotebook == nb) {
        d->mDefaultNotebook = Notebook::Ptr();
    }

    if (!nb->isRunTimeOnly()) {
        d->clearAlarms(nb->uid());
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
