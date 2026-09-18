#include "singleinstanceguard.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr int kPingTimeoutMs = 200;
constexpr int kLockTimeoutMs = 200;

QString lockFilePath(const QString &key)
{
    return QDir::temp().filePath(key + QStringLiteral(".lock"));
}
}

SingleInstanceGuard::SingleInstanceGuard(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
    , m_lockFile(lockFilePath(key))
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

    // No live instance answered. QLockFile::tryLock() atomically claims (or,
    // if the owning process is gone, reclaims) the lock in one step, so two
    // processes racing through the probe above can't both win: only one can
    // hold the lock, and it alone proceeds to remove any stale socket file
    // and listen.
    if (!m_lockFile.tryLock(kLockTimeoutMs)) {
        QLocalSocket recheck;
        recheck.connectToServer(m_key);
        if (recheck.waitForConnected(kPingTimeoutMs)) {
            recheck.write("activate");
            recheck.waitForBytesWritten(kPingTimeoutMs);
            recheck.disconnectFromServer();
            return false;
        }

        // Could not acquire the lock and could not reach a listener either.
        // Fail open rather than block the user from launching the app at all.
        return true;
    }

    QLocalServer::removeServer(m_key);

    m_server = std::make_unique<QLocalServer>(this);
    connect(m_server.get(), &QLocalServer::newConnection, this,
            &SingleInstanceGuard::handleNewConnection);

    if (!m_server->listen(m_key)) {
        m_server.reset();
        m_lockFile.unlock();
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
