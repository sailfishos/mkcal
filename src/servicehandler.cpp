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

#include "servicehandler.h"
#include "servicehandlerif.h"
#include "invitationhandlerif.h"
#include "logging_p.h"

using namespace mKCal;
using namespace KCalendarCore;

enum ExecutedPlugin {
    None = 0,
    SendInvitation,
    SendResponse,
    SendUpdate
};

class ServiceHandlerPrivate
{
public:
    QHash<QString, InvitationHandlerInterface *> mPlugins;
    QHash<QString, ServiceInterface *> mServices;

    bool mLoaded;
    int mDownloadId;
    ServiceHandler::ErrorCode mError;

    void loadPlugins();
    bool executePlugin(ExecutedPlugin action, const Incidence::Ptr &invitation, const QString &body,
                       const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage,
                       const Notebook::Ptr &notebook);
    ServiceInterface *getServicePlugin(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage);

    ServiceHandlerPrivate();
};

ServiceHandlerPrivate::ServiceHandlerPrivate() : mLoaded(false), mDownloadId(0),
    mError(ServiceHandler::ErrorOk)
{
}

void ServiceHandlerPrivate::loadPlugins()
{
    QString pluginPath = QLatin1String(qgetenv("MKCAL_PLUGIN_DIR"));
    if (pluginPath.isEmpty())
        pluginPath = QLatin1String(MKCALPLUGINDIR);
    QDir pluginsDir(pluginPath);
    qCDebug(lcMkcal) << "LOADING !!!! Plugin directory" << pluginsDir.path();

    foreach (const QString &fileName, pluginsDir.entryList(QDir::Files)) {
        qCDebug(lcMkcal) << "Loading service handler plugin" << fileName;
        QPluginLoader loader(pluginsDir.absoluteFilePath(fileName));
        QObject *plugin = loader.instance();

        if (!loader.isLoaded()) {
            qCDebug(lcMkcal) << "Failed to load plugin:" << loader.errorString();
        }
        if (plugin) {
            if (ServiceInterface *interface = qobject_cast<ServiceInterface *>(plugin)) {
                mServices.insert(interface->serviceName(), interface);
                qCDebug(lcMkcal) << "Loaded service:" << interface->serviceName();
            }
            if (InvitationHandlerInterface *interface = qobject_cast<InvitationHandlerInterface *>(plugin)) {
                mPlugins.insert(interface->pluginName(), interface);
                qCDebug(lcMkcal) << "Loaded plugin:" << interface->pluginName();
            }
        }  else {
            qCDebug(lcMkcal) << fileName << " Not a plugin";
        }
    }

    mLoaded = true;
}

bool ServiceHandlerPrivate::executePlugin(ExecutedPlugin action, const Incidence::Ptr &invitation, const QString &body,
                                          const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage,
                                          const Notebook::Ptr &notebook)
{
    if (storage.isNull() || invitation.isNull() || calendar.isNull())
        return false;

    if (!mLoaded)
        loadPlugins();

    Notebook accountNotebook;
    QString notebookUid;

    if (!notebook.isNull()) {
        accountNotebook = *notebook;
        notebookUid = notebook->uid();
    } else {
        notebookUid = calendar->notebook(invitation);
        if (storage->isValidNotebook(notebookUid)) {
            accountNotebook = storage->notebook(notebookUid);
        }
    }

    if (!accountNotebook.isValid()) {
        qCWarning(lcMkcal) << "No notebook available for invitation plugin to use";
        return false;
    }

    QString pluginName = accountNotebook.pluginName();
    QString accountId = accountNotebook.account() ;

    if (pluginName.isEmpty() || !mPlugins.contains(pluginName))
        pluginName = defaultName;

    qCDebug(lcMkcal) <<  "Using plugin:" << pluginName;

    QHash<QString, InvitationHandlerInterface *>::const_iterator i = mPlugins.find(pluginName);

    if (i != mPlugins.end()) {
        // service needed to get possible error, because
        // invitationhandlerinterface doesn't have error-function
        QHash<QString, ServiceInterface *>::const_iterator is = mServices.find(pluginName);

        switch (action) {
        case SendInvitation:
            if (i.value()->sendInvitation(accountId, notebookUid, invitation, body)) {
                return true;
            } else {
                mError = (ServiceHandler::ErrorCode) is.value()->error();
                return false;
            }

        case SendResponse:
            if (i.value()->sendResponse(accountId, invitation, body)) {
                return true;
            } else {
                mError = (ServiceHandler::ErrorCode) is.value()->error();
                return false;
            }

        case SendUpdate:
            if (i.value()->sendUpdate(accountId, invitation, body)) {
                return true;
            } else {
                mError = (ServiceHandler::ErrorCode) is.value()->error();
                return false;
            }

        default:
            return false;
        }
    } else {
        return false;
    }
}

ServiceInterface *ServiceHandlerPrivate::getServicePlugin(const Notebook::Ptr &notebook,
                                                          const ExtendedStorage::Ptr &storage)
{
    if (storage.isNull() || notebook.isNull())
        return 0;

    if (!storage->isValidNotebook(notebook->uid()))
        return 0;

    QString name(notebook->pluginName());

    if (name.isEmpty() || !mServices.contains(name)) {
        name = defaultName;
    }

    if (!mLoaded) {
        loadPlugins();
    }

    qCDebug(lcMkcal) <<  "Using service:" << name;

    QHash<QString, ServiceInterface *>::const_iterator i = mServices.find(name);

    if (i != mServices.end()) {
        return i.value();
    } else {
        return 0;
    }
}

ServiceHandler::ServiceHandler()
    : d(new ServiceHandlerPrivate())
{
}

bool ServiceHandler::sendInvitation(const Incidence::Ptr &invitation, const QString &body,
                                    const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage,
                                    const Notebook::Ptr &notebook)
{
    if (storage.isNull() || invitation.isNull() || calendar.isNull())
        return false;

    return d->executePlugin(SendInvitation, invitation, body, calendar, storage, notebook);
}


bool ServiceHandler::sendUpdate(const Incidence::Ptr &invitation, const QString &body,
                                const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage,
                                const Notebook::Ptr &notebook)
{
    if (storage.isNull() || invitation.isNull() || calendar.isNull())
        return false;

    return d->executePlugin(SendUpdate, invitation, body, calendar, storage, notebook);
}


bool ServiceHandler::sendResponse(const Incidence::Ptr &invitation, const QString &body,
                                  const ExtendedCalendar::Ptr &calendar, const ExtendedStorage::Ptr &storage,
                                  const Notebook::Ptr &notebook)
{
    if (storage.isNull() || invitation.isNull() || calendar.isNull())
        return false;

    return d->executePlugin(SendResponse, invitation, body, calendar, storage, notebook);
}

QString ServiceHandler::icon(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (storage.isNull() || notebook.isNull())
        return QString();

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        QString res(service->icon());
        if (res.isNull()) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return QString();
    }
}

bool ServiceHandler::multiCalendar(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (storage.isNull() || notebook.isNull())
        return false;

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        bool res = service->multiCalendar();
        if (!res) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return false;
    }
}

QString ServiceHandler::emailAddress(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (storage.isNull() || notebook.isNull())
        return QString();

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        QString res = service->emailAddress(notebook);
        if (res.isNull()) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return QString();
    }
}

QString ServiceHandler::displayName(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (storage.isNull() || notebook.isNull())
        return QString();

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        QString res = service->displayName(notebook);
        if (res.isNull()) {
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
    if (storage.isNull() || notebook.isNull())
        return -1;

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        bool res = service->downloadAttachment(notebook, uri, path);
        if (!res) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return d->mDownloadId++;
    } else {
        return -1;
    }
}

bool ServiceHandler::deleteAttachment(const KCalendarCore::Incidence::Ptr &incidence, const Notebook::Ptr &notebook,
                                      const ExtendedStorage::Ptr &storage, const QString &uri)
{
    if (storage.isNull() || notebook.isNull() || incidence.isNull())
        return false;

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        bool res = service->deleteAttachment(notebook, incidence, uri);
        if (!res) {
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
    if (storage.isNull() || notebook.isNull())
        return false;

    qCDebug(lcMkcal) <<  "shareNotebook";

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        bool res = service->shareNotebook(notebook, sharedWith);
        if (!res) {
            d->mError = (ServiceHandler::ErrorCode) service->error(); //Right now convert directly
        }
        return res;
    } else {
        return false;
    }
}

QStringList ServiceHandler::sharedWith(const Notebook::Ptr &notebook, const ExtendedStorage::Ptr &storage)
{
    if (storage.isNull() || notebook.isNull())
        return QStringList();

    ServiceInterface *service = d->getServicePlugin(notebook, storage);

    if (service) {
        QStringList res = service->sharedWith(notebook);
        if (res.isEmpty()) {
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

    foreach (ServiceInterface *service, d->mServices) {
        result.append(service->serviceName());
    }

    return result;
}

QString ServiceHandler::icon(QString serviceId)
{
    ServiceInterface *plugin = service(serviceId);
    return plugin ? plugin->icon() : QString();
}

QString ServiceHandler::uiName(QString serviceId)
{
    ServiceInterface *plugin = service(serviceId);
    return plugin ? plugin->uiName() : QString();
}

ServiceInterface* ServiceHandler::service(const QString &serviceId)
{
    if (!d->mLoaded)
        d->loadPlugins();

    QHash<QString, ServiceInterface *>::const_iterator i = d->mServices.find(serviceId);

    if (i != d->mServices.end()) {
        return i.value();
    } else {
        return nullptr;
    }
}

ServiceHandler::ErrorCode ServiceHandler::error() const
{
    return d->mError;
}

ServiceHandler::~ServiceHandler()
{
    delete d;
}
