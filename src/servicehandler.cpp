/*
  This file is part of the libextendedkcal library.

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

#include <QtCore/QHash>
#include <QtCore/QPluginLoader>
#include <QtCore/QStringList>
#include <QtCore/QDir>

#include <kdebug.h>

#include "servicehandler.h"
#include "servicehandlerif.h"
#include <invitationhandlerif.h>

using namespace mKCal;
using namespace KCalCore;

enum ExecutedPlugin {
  None = 0,
  SendInvitation,
  SendResponse,
  SendUpdate
};

ServiceHandler *mInstance = 0;

class ServiceHandlerPrivate
{

public:
    QHash<QString, InvitationHandlerInterface*> mPlugins;
    QHash<QString, ServiceInterface*> mServices;

    bool mLoaded;
    ExecutedPlugin mExecutedPlugin;

    void loadPlugins();
  bool executePlugin(const Incidence::Ptr &invitation, const QString body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage);


    ServiceHandlerPrivate();

};

ServiceHandlerPrivate::ServiceHandlerPrivate() : mLoaded(false)
{

}

void ServiceHandlerPrivate::loadPlugins()
{
    QDir pluginsDir(QLatin1String("/usr/lib/calendar/mkcalplugins")); //TODO HARDCODED!!
    kDebug() << "Plugin directory" << pluginsDir.path();

    foreach (const QString &fileName, pluginsDir.entryList(QDir::Files)) {
        QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));

        if (ServiceInterface* interface = qobject_cast<ServiceInterface*> (loader.instance())) {
            mServices.insert(interface->serviceName(), interface);
            kDebug() << "Loaded service:" << interface->serviceName();
        }
	if (InvitationHandlerInterface* interface = qobject_cast<InvitationHandlerInterface*> (loader.instance())) {
	    mPlugins.insert(interface->pluginName(), interface);
	    kDebug() << "Loaded plugin:" << interface->pluginName();
        }
    }

    mLoaded = true;
}

bool ServiceHandlerPrivate::executePlugin(const Incidence::Ptr &invitation, const QString body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{
    QString pluginName;
    QString accountId;

    QString notebookUid = calendar->notebook(invitation);
    if (storage->isValidNotebook(notebookUid)) {
        pluginName = storage->notebook(notebookUid)->pluginName();
        accountId  = storage->notebook(notebookUid)->account();
    }
    if (pluginName.isEmpty())
        pluginName = defaultName;
    kDebug() <<  "Using plugin:" << pluginName;

    QHash<QString, InvitationHandlerInterface*>::const_iterator i;
    i = mPlugins.find(pluginName);
    if (i == mPlugins.end() && pluginName != defaultName)
        i = mPlugins.find(defaultName);

    if (i != mPlugins.end())
        if (mExecutedPlugin == SendInvitation)
	    return i.value()->sendInvitation(accountId, invitation, body);
        else if (mExecutedPlugin == SendResponse)
	    return i.value()->sendResponse(accountId, invitation, body);
        else if (mExecutedPlugin == SendUpdate)
          return i.value()->sendUpdate(accountId, invitation, body);
      else
	    return false;
    else
        return false;
}

ServiceHandler::ServiceHandler():
        d(new ServiceHandlerPrivate())
{

}

bool ServiceHandler::sendInvitation(const Incidence::Ptr &invitation, const QString &body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{
    if (!d->mLoaded)
        d->loadPlugins();

    d->mExecutedPlugin = SendInvitation;
    return d->executePlugin( invitation, body, calendar, storage );
}


bool ServiceHandler::sendUpdate(const Incidence::Ptr &invitation, const QString &body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{
  if (!d->mLoaded)
      d->loadPlugins();

  d->mExecutedPlugin = SendUpdate;
  return d->executePlugin( invitation, body, calendar, storage );
}


bool ServiceHandler::sendResponse(const Incidence::Ptr &invitation, const QString &body, const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{
    if (!d->mLoaded)
        d->loadPlugins();

    d->mExecutedPlugin = SendResponse;
    return d->executePlugin( invitation, body, calendar, storage );
}


QIcon ServiceHandler::icon(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (!storage->isValidNotebook(notebook->uid()))
        return QIcon();

    if (notebook->pluginName().isEmpty())
        return QIcon();

    if (!d->mLoaded)
        d->loadPlugins();

    kDebug() <<  "Using service:" << notebook->pluginName();

    QHash<QString, ServiceInterface*>::const_iterator i;
    i = d->mServices.find(notebook->pluginName());

    if (i != d->mServices.end())
        return i.value()->icon();
    else
        return QIcon();
}


bool ServiceHandler::multiCalendar(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (!storage->isValidNotebook(notebook->uid()))
        return false;

    if (notebook->pluginName().isEmpty())
        return false;

    if (!d->mLoaded)
        d->loadPlugins();

    kDebug() <<  "Using service:" << notebook->pluginName();

    QHash<QString, ServiceInterface*>::const_iterator i;
    i = d->mServices.find(notebook->pluginName());

    if (i != d->mServices.end())
      return i.value()->multiCalendar();
    else
        return false;
}

QString ServiceHandler::emailAddress(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (!storage->isValidNotebook(notebook->uid()))
        return QString();

    if (notebook->pluginName().isEmpty())
        return QString();

    if (!d->mLoaded)
        d->loadPlugins();

    kDebug() <<  "Using service:" << notebook->pluginName();

    QHash<QString, ServiceInterface*>::const_iterator i;
    i = d->mServices.find(notebook->pluginName());

    if (i != d->mServices.end())
        return i.value()->emailAddress(notebook);
    else
        return QString();
}

QString ServiceHandler::displayName(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (!storage->isValidNotebook(notebook->uid()))
        return QString();

    if (notebook->pluginName().isEmpty())
        return QString();

    if (!d->mLoaded)
        d->loadPlugins();

    kDebug() <<  "Using service:" << notebook->pluginName();

    QHash<QString, ServiceInterface*>::const_iterator i;
    i = d->mServices.find(notebook->pluginName());

    if (i != d->mServices.end())
        return i.value()->displayName(notebook);
    else
        return QString();
}

bool ServiceHandler::downloadAttachment(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage, const QString &uri)
{
    if (!storage->isValidNotebook(notebook->uid()))
        return false;

    if (notebook->pluginName().isEmpty())
        return false;

    if (!d->mLoaded)
        d->loadPlugins();

    kDebug() <<  "Using service:" << notebook->pluginName();

    QHash<QString, ServiceInterface*>::const_iterator i;
    i = d->mServices.find(notebook->pluginName());

    if (i != d->mServices.end())
        return i.value()->downloadAttachment(uri);
    else
        return false;
}

bool ServiceHandler::shareNotebook(const Notebook::Ptr &notebook, const QStringList &sharedWith, const ExtendedStorage::Ptr &storage)
{
    kDebug() <<  "shareNotebook";

    if (!storage->isValidNotebook(notebook->uid()))
        return false;

    if (notebook->pluginName().isEmpty())
        return false;

    if (!d->mLoaded)
        d->loadPlugins();

    kDebug() <<  "Using service:" << notebook->pluginName();

    QHash<QString, ServiceInterface*>::const_iterator i;
    i = d->mServices.find(notebook->pluginName());

    if (i != d->mServices.end())
        return i.value()->shareNotebook(notebook, sharedWith);
    else
        return false;
}

QStringList ServiceHandler::sharedWith(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (!storage->isValidNotebook(notebook->uid()))
        return QStringList();

    if (notebook->pluginName().isEmpty())
        return QStringList();

    if (!d->mLoaded)
        d->loadPlugins();

    kDebug() <<  "Using service:" << notebook->pluginName();

    QHash<QString, ServiceInterface*>::const_iterator i;
    i = d->mServices.find(notebook->pluginName());

    if (i != d->mServices.end())
        return i.value()->sharedWith(notebook);
    else
        return QStringList();
}

ServiceHandler::~ServiceHandler()
{

    if (mInstance == 0) {
        delete mInstance;
    }
    delete d;
}
