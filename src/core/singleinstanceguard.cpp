#include "singleinstanceguard.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr int kPingTimeoutMs = 200;
}

SingleInstanceGuard::SingleInstanceGuard(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
}

SingleInstanceGuard::~SingleInstanceGuard() = default;

bool SingleInstanceGuard::tryAcquire()
{
    QLocalSocket probe;
    probe.connectToServer(m_key);
    if (probe.waitForConnected(kPingTimeoutMs)) {
        probe.write("activate");
        probe.waitForBytesWritten(kPingTimeoutMs);
        probe.disconnectFromServer();
        return false;
    }

    // No live instance answered. Any socket file left at this name belongs
    // to a previous instance that never shut down cleanly (e.g. a crash) —
    // remove it so listen() below doesn't fail thinking it's still in use.
    QLocalServer::removeServer(m_key);

    m_server = std::make_unique<QLocalServer>(this);
    connect(m_server.get(), &QLocalServer::newConnection, this,
            &SingleInstanceGuard::handleNewConnection);

    if (!m_server->listen(m_key)) {
        // Lost a startup race to another instance mid-check, or the platform
        // refused for some other reason. Fail open rather than block the
        // user from launching the app at all.
        m_server.reset();
        return true;
    }

    return true;
}

void SingleInstanceGuard::handleNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        emit activationRequested();
    }
}
