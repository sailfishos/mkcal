/*
  This file is part of the mkcal library.

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
  defines the Notebook class.

  @brief
  This class is a Maemo incidence placeholder.

  @author Tero Aho \<ext-tero.1.aho@nokia.com\>
*/

#include "notebook.h"
#include "logging_p.h"
using namespace KCalendarCore;

#include <QtCore/QStringList>
#include <QtCore/QHash>
#include <QtCore/QUuid>

using namespace mKCal;

static const int FLAG_ALLOW_EVENT = (1 << 0);
static const int FLAG_ALLOW_JOURNAL = (1 << 1);
static const int FLAG_ALLOW_TODO = (1 << 2);
static const int FLAG_IS_SHARED = (1 << 3);
static const int FLAG_IS_MASTER = (1 << 4);
static const int FLAG_IS_SYNCED = (1 << 5);
static const int FLAG_IS_READONLY = (1 << 6);
static const int FLAG_IS_VISIBLE = (1 << 7);
static const int FLAG_IS_RUNTIMEONLY = (1 << 8);
static const int FLAG_IS_SHAREABLE = (1 << 9);

#define NOTEBOOK_FLAGS_ALLOW_ALL        \
  ( FLAG_ALLOW_EVENT |                  \
    FLAG_ALLOW_JOURNAL |                \
    FLAG_ALLOW_TODO )

#define DEFAULT_NOTEBOOK_FLAGS          \
  ( NOTEBOOK_FLAGS_ALLOW_ALL |          \
    FLAG_IS_MASTER |                    \
    FLAG_IS_VISIBLE )

#define SET_BIT_OR_RETURN(var,bit,value)          \
do {                                              \
  if ( ( ( (var) & (bit) ) > 0 ) == !!(value) ) { \
    return;                                       \
  }                                               \
  if ( value ) {                                  \
    var |= bit;                                   \
  } else {                                        \
    var &= ~(bit);                                \
  }                                               \
} while(0)

/**
  Private class that helps to provide binary compatibility between releases.
  @internal
*/
//@Cond PRIVATE
class mKCal::Notebook::Private
{
public:
    Private()
    {}

    Private(const QString &uid)
        : mUid(uid)
    {
        if (mUid.length() < 7) {
            // Could use QUuid::WithoutBraces when moving to Qt5.11.
            const QString uid(QUuid::createUuid().toString());
            mUid = uid.mid(1, uid.length() - 2);
        }
    }

    Private(const Private &other)
        : mUid(other.mUid),
          mName(other.mName),
          mDescription(other.mDescription),
          mColor(other.mColor),
          mFlags(other.mFlags),
          mSyncDate(other.mSyncDate),
          mPluginName(other.mPluginName),
          mAccount(other.mAccount),
          mAttachmentSize(other.mAttachmentSize),
          mModifiedDate(other.mModifiedDate),
          mSharedWith(other.mSharedWith),
          mSyncProfile(other.mSyncProfile),
          mCreationDate(other.mCreationDate),
          mCustomProperties(other.mCustomProperties)
    {}

    QString mUid;
    QString mName;
    QString mDescription;
    QString mColor;
    int mFlags = DEFAULT_NOTEBOOK_FLAGS;
    QDateTime mSyncDate;
    QString mPluginName;
    QString mAccount;
    int mAttachmentSize = -1;
    QDateTime mModifiedDate;
    QStringList mSharedWith;
    QString mSyncProfile;
    QDateTime mCreationDate;
    QHash<QByteArray, QString> mCustomProperties;
};
//@endcond

Notebook::Notebook()
    : d(new Notebook::Private())
{
}

Notebook::Notebook(const QString &name, const QString &description, const QString &color)
    : d(new Notebook::Private(QString()))
{
    setName(name);
    setDescription(description);
    setColor(color);
}

Notebook::Notebook(const QString &uid, const QString &name,
                   const QString &description, const QString &color,
                   bool isShared, bool isMaster, bool isSynced,
                   bool isReadOnly, bool isVisible)
    : d(new Notebook::Private(uid))
{
    setName(name);
    setDescription(description);
    setColor(color);
    setIsShared(isShared);
    setIsMaster(isMaster);
    setIsSynchronized(isSynced);
    setIsReadOnly(isReadOnly);
    setIsVisible(isVisible);
}

Notebook::Notebook(const QString &uid, const QString &name,
                   const QString &description, const QString &color,
                   bool isShared, bool isMaster, bool isSynced,
                   bool isReadOnly, bool isVisible, const QString &pluginName,
                   const QString &account, int attachmentSize)
    : d(new Notebook::Private(uid))
{
    setName(name);
    setDescription(description);
    setColor(color);
    setIsShared(isShared);
    setIsMaster(isMaster);
    setIsSynchronized(isSynced);
    setIsReadOnly(isReadOnly);
    setIsVisible(isVisible);
    setPluginName(pluginName);
    setAccount(account);
    setAttachmentSize(attachmentSize);
}

Notebook::Notebook(const Notebook &i)
    : d(new Notebook::Private(*i.d))
{
}

Notebook::~Notebook()
{
    delete d;
}

Notebook &Notebook::operator=(const Notebook &other)
{
    // check for self assignment
    if (&other == this) {
        return *this;
    }
    *d = *other.d;
    return *this;
}

bool Notebook::operator==(const Notebook &i2) const
{
    return
        d->mUid == i2.uid() &&
        d->mName == i2.name() &&
        d->mDescription == i2.description() &&
        d->mColor == i2.color() &&
        d->mFlags == i2.d->mFlags &&
        d->mSyncDate == i2.syncDate() &&
        d->mPluginName == i2.pluginName() &&
        d->mModifiedDate == i2.modifiedDate() &&
        d->mSharedWith == i2.sharedWith() &&
        d->mCreationDate == i2.creationDate();
}

QString Notebook::uid() const
{
    return d->mUid;
}

void Notebook::setUid(const QString &uid)
{
    d->mUid = uid;
}

QString Notebook::name() const
{
    return d->mName;
}

void Notebook::setName(const QString &name)
{
    d->mName = name;
}

QString Notebook::description() const
{
    return d->mDescription;
}

void Notebook::setDescription(const QString &description)
{
    d->mDescription = description;
}

QString Notebook::color() const
{
    return d->mColor;
}

void Notebook::setColor(const QString &color)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mColor = color.isEmpty() ? QString::fromLatin1("#0000FF") : color;
}

bool Notebook::isShared() const
{
    return d->mFlags & FLAG_IS_SHARED;
}

void Notebook::setIsShared(bool isShared)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_IS_SHARED, isShared);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::isMaster() const
{
    return d->mFlags & FLAG_IS_MASTER;
}

void Notebook::setIsMaster(bool isMaster)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_IS_MASTER, isMaster);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::isSynchronized() const
{
    return d->mFlags & FLAG_IS_SYNCED;
}

void Notebook::setIsSynchronized(bool isSynced)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_IS_SYNCED, isSynced);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::isReadOnly() const
{
    return d->mFlags & FLAG_IS_READONLY;
}

void Notebook::setIsReadOnly(bool isReadOnly)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_IS_READONLY, isReadOnly);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::isVisible() const
{
    return d->mFlags & FLAG_IS_VISIBLE;
}

void Notebook::setIsVisible(bool isVisible)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_IS_VISIBLE, isVisible);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::isRunTimeOnly() const
{
    return d->mFlags & FLAG_IS_RUNTIMEONLY;
}

void Notebook::setRunTimeOnly(bool isRunTime)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_IS_RUNTIMEONLY, isRunTime);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

QDateTime Notebook::syncDate() const
{
    return d->mSyncDate;
}

void Notebook::setSyncDate(const QDateTime &syncDate)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mSyncDate = syncDate;
}

QString Notebook::pluginName() const
{
    return d->mPluginName;
}

void Notebook::setPluginName(const QString &pluginName)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mPluginName = pluginName;
}

QString Notebook::account() const
{
    return d->mAccount;
}

void Notebook::setAccount(const QString &account)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mAccount = account;
}

int Notebook::attachmentSize() const
{
    return d->mAttachmentSize;
}

void Notebook::setAttachmentSize(int size)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mAttachmentSize = size;
}

QDateTime Notebook::modifiedDate() const
{
    return d->mModifiedDate;
}

void Notebook::setModifiedDate(const QDateTime &modifiedDate)
{
    d->mModifiedDate = modifiedDate;
}

QDateTime Notebook::creationDate() const
{
    return d->mCreationDate;
}

void Notebook::setCreationDate(const QDateTime &date)
{
    d->mCreationDate = date;
}

bool Notebook::isShareable() const
{
    return d->mFlags & FLAG_IS_SHAREABLE;
}

void Notebook::setIsShareable(bool isShareable)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_IS_SHAREABLE, isShareable);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

QStringList Notebook::sharedWith() const
{
    return d->mSharedWith;
}

QString Notebook::sharedWithStr() const
{
    return d->mSharedWith.join(",");
}

void Notebook::setSharedWith(const QStringList &sharedWith)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mSharedWith = sharedWith;
}

void Notebook::setSharedWithStr(const QString &sharedWithStr)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mSharedWith.clear();

    if (sharedWithStr.isEmpty()) {
        return;
    }

    d->mSharedWith = sharedWithStr.split(',');

    QStringList::Iterator it;
    for (it = d->mSharedWith.begin(); it != d->mSharedWith.end(); ++it) {
        *it = (*it).trimmed();
    }
}

QString Notebook::syncProfile() const
{
    return d->mSyncProfile;
}

void Notebook::setSyncProfile(const QString &syncProfile)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    d->mSyncProfile = syncProfile;
}

void Notebook::setEventsAllowed(bool eventsAllowed)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_ALLOW_EVENT, eventsAllowed);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::eventsAllowed() const
{
    return d->mFlags & FLAG_ALLOW_EVENT;
}

void Notebook::setJournalsAllowed(bool journalsAllowed)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_ALLOW_JOURNAL, journalsAllowed);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::journalsAllowed() const
{
    return d->mFlags & FLAG_ALLOW_JOURNAL;
}

void Notebook::setTodosAllowed(bool todosAllowed)
{
    SET_BIT_OR_RETURN(d->mFlags, FLAG_ALLOW_TODO, todosAllowed);
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
}

bool Notebook::todosAllowed() const
{
    return d->mFlags & FLAG_ALLOW_TODO;
}

bool Notebook::incidenceAllowed(Incidence::Ptr incidence) const
{
    // First off, handle invalid incidence pointers. We're not
    // interested in those.
    if (!incidence) {
        return false;
    }

    // Then, consider the type of incidence - can it be added to this
    // type of notebook?
    if (incidence->type() == Incidence::TypeEvent) {
        if (!eventsAllowed()) {
            qCDebug(lcMkcal) << "unable add event to this notebook";
            return false;
        }
    } else if (incidence->type() == Incidence::TypeTodo) {
        if (!todosAllowed()) {
            qCDebug(lcMkcal) << "unable add todo to this notebook";
            return false;
        }
    } else if (incidence->type() == Incidence::TypeJournal) {
        if (!journalsAllowed()) {
            qCDebug(lcMkcal) << "unable add journal to this notebook";
            return false;
        }
    }

    // Default accept
    return true;
}

void Notebook::setCustomProperty(const QByteArray &key, const QString &value)
{
    d->mModifiedDate = QDateTime::currentDateTimeUtc();
    if (value.isEmpty()) {
        d->mCustomProperties.remove(key);
    } else {
        d->mCustomProperties.insert(key, value);
    }
}

QString Notebook::customProperty(const QByteArray &key, const QString &defaultValue) const
{
    return d->mCustomProperties.value(key, defaultValue);
}

QList<QByteArray> Notebook::customPropertyKeys() const
{
    return d->mCustomProperties.uniqueKeys();
}
