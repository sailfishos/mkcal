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


#include "invitationhandler.h"
#include <invitationhandlerif.h>
#include "extendedcalendar.h"
#include "extendedstorage.h"
using namespace KCalCore;

#include <kdebug.h>

#include <QtCore/QDir>
#include <QtCore/QPluginLoader>

using namespace mKCal;
using namespace KCalCore;

enum ExecutedPlugin {
  None = 0,
  SendInvitation,
  SendResponse
};

InvitationHandler *mInstance = 0;

class InvitationHandlerPrivate
{
  public:
    QHash<QString, InvitationHandlerInterface*> mPlugins;

    bool mLoaded;
    ExecutedPlugin mExecutedPlugin;

    void loadPlugins();
    void unloadPlugins();
    bool executePlugin( const Incidence::Ptr &invitation, const QString body,
                        const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage );
    InvitationHandlerPrivate();
};

InvitationHandlerPrivate::InvitationHandlerPrivate() : mLoaded( false )
{
}

void InvitationHandlerPrivate::loadPlugins()
{
  QDir pluginsDir( QLatin1String( "/usr/lib/calendar/mkcalplugins" ) ); //TODO HARDCODED!!
  kDebug() << "Plugin directory" << pluginsDir.path();

  foreach ( const QString &fileName, pluginsDir.entryList( QDir::Files ) ) {
    QPluginLoader loader( pluginsDir.absoluteFilePath( fileName ) );

    if ( InvitationHandlerInterface *interface =
         qobject_cast<InvitationHandlerInterface*> ( loader.instance() ) ) {
      mPlugins.insert( interface->pluginName(), interface );
      kDebug() << "Loaded plugin:" << interface->pluginName();
    }
  }

  mLoaded = true;
}

bool InvitationHandlerPrivate::executePlugin( const Incidence::Ptr &invitation, const QString body,
                                              const ExtendedCalendar::Ptr &calendar,
                                              const ExtendedStorage::Ptr &storage )
{
  QString pluginName;
  QString accountId;

  QString notebookUid = calendar->notebook( invitation );
  if ( storage->isValidNotebook( notebookUid ) ) {
    pluginName = storage->notebook( notebookUid )->pluginName();
    accountId  = storage->notebook( notebookUid )->account();
  }
  if ( pluginName.isEmpty() ) {
    pluginName = defaultName;
  }
  kDebug() <<  "Using plugin:" << pluginName;

  QHash<QString,InvitationHandlerInterface*>::const_iterator i;
  i = mPlugins.find( pluginName );
  if ( i == mPlugins.end() && pluginName != defaultName ) {
    i = mPlugins.find( defaultName );
  }

  if ( i != mPlugins.end() ) {
    if ( mExecutedPlugin == SendInvitation ) {
      return i.value()->sendInvitation( accountId, invitation, body );
    } else if ( mExecutedPlugin == SendResponse ) {
      return i.value()->sendResponse( accountId, invitation, body );
    } else {
      return false;
    }
  } else {
    return false;   //TODO Here we need to send with default one
  }
}

InvitationHandler::InvitationHandler()
  : d( new InvitationHandlerPrivate() )
{
}

bool InvitationHandler::sendInvitation( const Incidence::Ptr &invitation, const QString &body,
                                        const ExtendedCalendar::Ptr &calendar,
                                        const ExtendedStorage::Ptr &storage )
{
  if ( !d->mLoaded ) {
    d->loadPlugins();
  }

  d->mExecutedPlugin = SendInvitation;
  return d->executePlugin( invitation, body, calendar, storage );
}

bool InvitationHandler::sendUpdate( const Incidence::Ptr &invitation, const QString &body,
                                    const ExtendedCalendar::Ptr &calendar,
                                    const ExtendedStorage::Ptr &storage )
{
  Q_UNUSED( invitation );
  Q_UNUSED( calendar );
  Q_UNUSED( storage );
  Q_UNUSED( body );

  return false;
}

bool InvitationHandler::sendResponse( const Incidence::Ptr &invitation, const QString &body,
                                      const ExtendedCalendar::Ptr &calendar,
                                      const ExtendedStorage::Ptr &storage )
{
  if ( !d->mLoaded ) {
    d->loadPlugins();
  }

  d->mExecutedPlugin = SendResponse;
  return d->executePlugin( invitation, body, calendar, storage );
}

bool InvitationHandler::shareNotebook( const Notebook::Ptr &notebook, const QStringList &sharedWith,
                                       const ExtendedStorage::Ptr &storage )
{
  Q_UNUSED(notebook);
  Q_UNUSED(sharedWith);
  Q_UNUSED(storage);


  //TODO Implement this with new interface defined in mKCal

  return false;
}

InvitationHandler::~InvitationHandler()
{
  if ( mInstance == 0 ) {
    delete mInstance;
  }
  delete d;
}

