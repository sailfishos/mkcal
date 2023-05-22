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

class ServiceHandlerPrivate
{
public:
    QHash<QString, InvitationHandlerInterface *> mPlugins;
    QHash<QString, ServiceInterface *> mServices;

    bool mLoaded;
    int mDownloadId;
    ServiceHandler::ErrorCode mError;

    void loadPlugins();
    InvitationHandlerInterface* invitationPlugin(const QString &pluginName);

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

InvitationHandlerInterface* ServiceHandlerPrivate::invitationPlugin(const QString &pluginName)
{
    if (!mLoaded)
        loadPlugins();

    QHash<QString, InvitationHandlerInterface *>::const_iterator i = mPlugins.find(pluginName);
    if (i != mPlugins.end()) {
        return i.value();
    } else if ((i = mPlugins.find(defaultName)) != mPlugins.end()) {
        return i.value();
    } else {
        return nullptr;
    }
}

ServiceHandler::ServiceHandler()
    : d(new ServiceHandlerPrivate())
{
}

bool ServiceHandler::sendInvitation(const Notebook::Ptr &notebook, const Incidence::Ptr &invitation, const QString &body)
{
    if (!notebook || !invitation)
        return false;

    InvitationHandlerInterface* plugin = d->invitationPlugin(notebook->pluginName());

    d->mError = ErrorOk;
    if (plugin) {
        if (plugin->sendInvitation(notebook->account(), notebook->uid(), invitation, body)) {
            return true;
        } else {
            // service needed to get possible error, because
            // invitationhandlerinterface doesn't have error-function
            // This is utterly broken by design, except when the invitation plugin is also a service plugin
            ServiceInterface *interface = service(notebook->pluginName());
            if (!interface) {
                interface = service(defaultName);
            }
            d->mError = interface ? (ServiceHandler::ErrorCode)interface->error() : ErrorInternal;
            return false;
        }
    } else {
        return false;
    }
}


bool ServiceHandler::sendUpdate(const Notebook::Ptr &notebook, const Incidence::Ptr &invitation, const QString &body)
{
    if (!notebook || !invitation)
        return false;

    InvitationHandlerInterface* plugin = d->invitationPlugin(notebook->pluginName());

    d->mError = ErrorOk;
    if (plugin) {
        if (plugin->sendUpdate(notebook->account(), invitation, body)) {
            return true;
        } else {
            // service needed to get possible error, because
            // invitationhandlerinterface doesn't have error-function
            // This is utterly broken by design, except when the invitation plugin is also a service plugin
            ServiceInterface *interface = service(notebook->pluginName());
            if (!interface) {
                interface = service(defaultName);
            }
            d->mError = interface ? (ServiceHandler::ErrorCode)interface->error() : ErrorInternal;
            return false;
        }
    } else {
        return false;
    }
}


bool ServiceHandler::sendResponse(const Notebook::Ptr &notebook, const Incidence::Ptr &invitation, const QString &body)
{
    if (!notebook || !invitation)
        return false;

    InvitationHandlerInterface* plugin = d->invitationPlugin(notebook->pluginName());

    d->mError = ErrorOk;
    if (plugin) {
        if (plugin->sendResponse(notebook->account(), invitation, body)) {
            return true;
        } else {
            // service needed to get possible error, because
            // invitationhandlerinterface doesn't have error-function
            // This is utterly broken by design, except when the invitation plugin is also a service plugin
            ServiceInterface *interface = service(notebook->pluginName());
            if (!interface) {
                interface = service(defaultName);
            }
            d->mError = interface ? (ServiceHandler::ErrorCode)interface->error() : ErrorInternal;
            return false;
        }
    } else {
        return false;
    }
}

QString ServiceHandler::icon(const QString &serviceId)
{
    ServiceInterface *plugin = service(serviceId);
    if (!plugin) {
        plugin = service(defaultName);
    }

    return plugin ? plugin->icon() : QString();
}

bool ServiceHandler::multiCalendar(const QString &serviceId)
{
    ServiceInterface *plugin = service(serviceId);
    if (!plugin) {
        plugin = service(defaultName);
    }

    d->mError = ErrorOk;
    if (plugin) {
        bool res = plugin->multiCalendar();
        d->mError = (ServiceHandler::ErrorCode) plugin->error(); //Right now convert directly
        return res;
    } else {
        return false;
    }
}

QString ServiceHandler::emailAddress(const Notebook::Ptr &notebook)
{
    ServiceInterface *plugin = service(notebook ? notebook->pluginName() : defaultName);
    if (!plugin) {
        plugin = service(defaultName);
    }

    return plugin ? plugin->emailAddress(notebook) : QString();
}

QString ServiceHandler::displayName(const Notebook::Ptr &notebook)
{
    ServiceInterface *plugin = service(notebook ? notebook->pluginName() : defaultName);
    if (!plugin) {
        plugin = service(defaultName);
    }

    return plugin ? plugin->displayName(notebook) : QString();
}

int ServiceHandler::downloadAttachment(const Notebook::Ptr &notebook, const QString &uri, const QString &path)
{
    if (!notebook) {
        return -1;
    }

    ServiceInterface *plugin = service(notebook->pluginName());
    if (!plugin) {
        plugin = service(defaultName);
    }

    d->mError = ErrorOk;
    if (plugin) {
        bool res = plugin->downloadAttachment(notebook, uri, path);
        if (!res) {
            d->mError = (ServiceHandler::ErrorCode) plugin->error(); //Right now convert directly
        }
        return d->mDownloadId++;
    } else {
        return -1;
    }
}

bool ServiceHandler::deleteAttachment(const KCalendarCore::Incidence::Ptr &incidence, const Notebook::Ptr &notebook,
                                      const QString &uri)
{
    if (!notebook) {
        return false;
    }

    ServiceInterface *plugin = service(notebook->pluginName());
    if (!plugin) {
        plugin = service(defaultName);
    }

    d->mError = ErrorOk;
    if (plugin) {
        bool res = plugin->deleteAttachment(notebook, incidence, uri);
        if (!res) {
            d->mError = (ServiceHandler::ErrorCode) plugin->error(); //Right now convert directly
        }
        return res;
    } else {
        return false;
    }
}

bool ServiceHandler::shareNotebook(const Notebook::Ptr &notebook, const QStringList &sharedWith)
{
    ServiceInterface *plugin = service(notebook ? notebook->pluginName() : defaultName);
    if (!plugin) {
        plugin = service(defaultName);
    }

    d->mError = ErrorOk;
    if (plugin) {
        bool res = plugin->shareNotebook(notebook, sharedWith);
        if (!res) {
            d->mError = (ServiceHandler::ErrorCode) plugin->error(); //Right now convert directly
        }
        return res;
    } else {
        return false;
    }
}

QStringList ServiceHandler::sharedWith(const Notebook::Ptr &notebook)
{
    ServiceInterface *plugin = service(notebook ? notebook->pluginName() : defaultName);
    if (!plugin) {
        plugin = service(defaultName);
    }

    d->mError = ErrorOk;
    if (plugin) {
        QStringList res = plugin->sharedWith(notebook);
        d->mError = (ServiceHandler::ErrorCode) plugin->error(); //Right now convert directly
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
