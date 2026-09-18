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

    // No live instance answered. Try to claim the name outright first; only if
    // that fails because a stale socket file is left behind (e.g. a previous
    // instance crashed) do we remove it and retry, so two processes racing
    // through the probe above can't both delete the file out from under
    // whichever one gets to listen() first.
    m_server = std::make_unique<QLocalServer>(this);
    connect(m_server.get(), &QLocalServer::newConnection, this,
            &SingleInstanceGuard::handleNewConnection);

    if (m_server->listen(m_key)) {
        return true;
    }

    if (m_server->serverError() == QAbstractSocket::AddressInUseError) {
        QLocalSocket recheck;
        recheck.connectToServer(m_key);
        if (recheck.waitForConnected(kPingTimeoutMs)) {
            // Another instance won the race and is now listening.
            recheck.write("activate");
            recheck.waitForBytesWritten(kPingTimeoutMs);
            recheck.disconnectFromServer();
            m_server.reset();
            return false;
        }

        QLocalServer::removeServer(m_key);
        if (m_server->listen(m_key)) {
            return true;
        }
    }

    // Lost a startup race to another instance mid-check, or the platform
    // refused for some other reason. Fail open rather than block the
    // user from launching the app at all.
    m_server.reset();
    return true;
}

void SingleInstanceGuard::handleNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        emit activationRequested();
    }
}
