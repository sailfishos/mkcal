/*
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
    QHash<QString, InvitationHandlerInterface *> mPlugins;
    QHash<QString, ServiceInterface *> mServices;

    bool mLoaded;
    int mDownloadId;
    ServiceHandler::ErrorCode mError;
    ExecutedPlugin mExecutedPlugin;

    void loadPlugins();
    bool executePlugin(const Incidence::Ptr &invitation, const QString body, const ExtendedCalendar::Ptr &calendar,
                       const ExtendedStorage::Ptr &storage);
    ServiceInterface *getServicePlugin( const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage);


    ServiceHandlerPrivate();

};

ServiceHandlerPrivate::ServiceHandlerPrivate() : mLoaded(false), mDownloadId(0),
    mError(ServiceHandler::ErrorOk), mExecutedPlugin(None)

{

}

void ServiceHandlerPrivate::loadPlugins()
{
    QDir pluginsDir(QLatin1String("/usr/lib/mkcalplugins")); //TODO HARDCODED!!
    kDebug() << "LOADING !!!! Plugin directory" << pluginsDir.path();

    foreach (const QString &fileName, pluginsDir.entryList(QDir::Files)) {
        qDebug() << fileName;
        QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
        QObject *plugin = loader.instance();
        qDebug() << loader.errorString();
        if (plugin) {
            if (ServiceInterface *interface = qobject_cast<ServiceInterface *>( plugin ) ) {
                mServices.insert(interface->serviceName(), interface);
                kDebug() << "Loaded service:" << interface->serviceName();
            }
            if (InvitationHandlerInterface *interface = qobject_cast<InvitationHandlerInterface *>( plugin ) ) {
                mPlugins.insert( interface->pluginName(), interface );
                kDebug() << "Loaded plugin:" << interface->pluginName();
            }
        }  else {
            qDebug() << fileName << " Not a plugin";
        }
    }

    mLoaded = true;
}

bool ServiceHandlerPrivate::executePlugin(const Incidence::Ptr &invitation, const QString body,
                                          const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{

    if ( storage.isNull() || invitation.isNull() || calendar.isNull() )
        return false;

    QString pluginName;
    QString accountId;

    QString notebookUid = calendar->notebook(invitation);
    if (storage->isValidNotebook(notebookUid)) {
        pluginName = storage->notebook(notebookUid)->pluginName();
        accountId  = storage->notebook(notebookUid)->account();
    }
    if (pluginName.isEmpty() || !mPlugins.contains(pluginName))
        pluginName = defaultName;
    kDebug() <<  "Using plugin:" << pluginName;

    QHash<QString, InvitationHandlerInterface *>::const_iterator i;
    i = mPlugins.find(pluginName);

    if (i != mPlugins.end()) {
        // service needed to get possible error, because
        // invitationhandlerinterface does'n have error-function
        QHash<QString, ServiceInterface *>::const_iterator is;
        is = mServices.find( pluginName );

        if (mExecutedPlugin == SendInvitation) {
            if (i.value()->sendInvitation(accountId, notebookUid, invitation, body))
                return true;
            else {
                mError = (ServiceHandler::ErrorCode) is.value()->error();
                return false;
            }
        } else if (mExecutedPlugin == SendResponse)
            if (i.value()->sendResponse(accountId, invitation, body))
                return true;
            else {
                mError = (ServiceHandler::ErrorCode) is.value()->error();
                return false;
            } else if (mExecutedPlugin == SendUpdate)
            if (i.value()->sendUpdate(accountId, invitation, body))
                return true;
            else {
                mError = (ServiceHandler::ErrorCode) is.value()->error();
                return false;
            } else
            return false;
    } else
        return false;
}

ServiceInterface *ServiceHandlerPrivate::getServicePlugin( const Notebook::Ptr &notebook,
                                                           const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || notebook.isNull() )
        return 0;

    if (!storage->isValidNotebook(notebook->uid()))
        return 0;

    QString name( notebook->pluginName() );

    if (name.isEmpty()) {
        name = defaultName;
    }

    if (!mLoaded) {
        loadPlugins();
    }

    kDebug() <<  "Using service:" << name;

    QHash<QString, ServiceInterface *>::const_iterator i;
    i = mServices.find( name );

    if (i != mServices.end()) {
        return i.value();
    } else {
        return 0;
    }
}

ServiceHandler::ServiceHandler():
    d(new ServiceHandlerPrivate())
{

}

bool ServiceHandler::sendInvitation(const Incidence::Ptr &invitation, const QString &body,
                                    const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || invitation.isNull() || calendar.isNull() )
        return false;

    if (!d->mLoaded)
        d->loadPlugins();

    d->mExecutedPlugin = SendInvitation;
    return d->executePlugin( invitation, body, calendar, storage );
}


bool ServiceHandler::sendUpdate(const Incidence::Ptr &invitation, const QString &body,
                                const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || invitation.isNull() || calendar.isNull() )
        return false;

    if (!d->mLoaded)
        d->loadPlugins();

    d->mExecutedPlugin = SendUpdate;
    return d->executePlugin( invitation, body, calendar, storage );
}


bool ServiceHandler::sendResponse(const Incidence::Ptr &invitation, const QString &body,
                                  const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || invitation.isNull() || calendar.isNull() )
        return false;

    if (!d->mLoaded)
        d->loadPlugins();

    d->mExecutedPlugin = SendResponse;
    return d->executePlugin( invitation, body, calendar, storage );
}


QString ServiceHandler::icon(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || notebook.isNull() )
        return QString();

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        QString res ( service->icon() );
        if ( res.isNull() ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return QString();
    }
}


bool ServiceHandler::multiCalendar(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || notebook.isNull() )
        return false;

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        bool res = service->multiCalendar();
        if ( !res ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return false;
    }
}

QString ServiceHandler::emailAddress(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || notebook.isNull() )
        return QString();

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        QString res =  service->emailAddress(notebook);
        if ( res.isNull() ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return QString();
    }
}

QString ServiceHandler::displayName(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || notebook.isNull() )
        return false;

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        QString res = service->displayName(notebook);
        if ( res.isNull() ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return QString();
    }
}

int ServiceHandler::downloadAttachment(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage,
                                       const QString &uri, const QString &path)
{
    if ( storage.isNull() || notebook.isNull() )
        return -1;

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        bool res = service->downloadAttachment(notebook, uri, path);
        if ( !res ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return d->mDownloadId++;
    } else {
        return -1;
    }
}

bool ServiceHandler::deleteAttachment(const KCalCore::Incidence::Ptr &incidence, const Notebook::Ptr &notebook,
                                      const ExtendedStorage::Ptr &storage, const QString &uri)
{
    if ( storage.isNull() || notebook.isNull() || incidence.isNull() )
        return false;

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        bool res = service->deleteAttachment(notebook, incidence, uri);
        if ( !res ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return false;
    }
}

bool ServiceHandler::shareNotebook(const Notebook::Ptr &notebook, const QStringList &sharedWith,
                                   const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || notebook.isNull() )
        return false;

    kDebug() <<  "shareNotebook";

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        bool res = service->shareNotebook(notebook, sharedWith);
        if ( !res ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return false;
    }
}

QStringList ServiceHandler::sharedWith(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if ( storage.isNull() || notebook.isNull() )
        return QStringList();

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if ( service ) {
        QStringList res = service->sharedWith( notebook );
        if ( res.isEmpty() ) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return QStringList();
    }
}

QString ServiceHandler::defaultNotebook(const QString &productId)
{
    Q_UNUSED(productId);

    return QString(); //Empty implementation so far

}


QStringList ServiceHandler::availableServices()
{
    if (!d->mLoaded)
        d->loadPlugins();
    QStringList result;

    foreach ( ServiceInterface *service, d->mServices) {
        result.append( service->serviceName() );
    }

    return result;

}

QString ServiceHandler::icon(QString serviceId)
{
    if (!d->mLoaded)
        d->loadPlugins();

    QHash<QString, ServiceInterface *>::const_iterator i;
    i = d->mServices.find( serviceId );

    if (i != d->mServices.end()) {
        return i.value()->icon();
    } else {
        return QString();
    }
}

QString ServiceHandler::uiName(QString serviceId)
{
    if (!d->mLoaded)
        d->loadPlugins();

    QHash<QString, ServiceInterface *>::const_iterator i;
    i = d->mServices.find( serviceId );

    if (i != d->mServices.end()) {
        return i.value()->uiName();
    } else {
        return QString();
    }
}


ServiceHandler::ErrorCode ServiceHandler::error() const
{
    return d->mError;
}

ServiceHandler::~ServiceHandler()
{

    if (mInstance == 0) {
        delete mInstance;
    }
    delete d;
}
