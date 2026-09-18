#include "singleinstanceguard.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

namespace {
constexpr int kPingTimeoutMs = 200;
constexpr int kLockTimeoutMs = 200;
constexpr int kLockRetryAttempts = 5;

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
    // and listen. If another process holds the lock but hasn't listened yet,
    // retry a bounded number of times rather than assuming primary status
    // without ever creating a listening server.
    for (int attempt = 0; attempt < kLockRetryAttempts; ++attempt) {
        if (m_lockFile.tryLock(kLockTimeoutMs)) {
            QLocalServer::removeServer(m_key);

            m_server = std::make_unique<QLocalServer>(this);
            connect(m_server.get(), &QLocalServer::newConnection, this,
                    &SingleInstanceGuard::handleNewConnection);

            if (!m_server->listen(m_key)) {
                m_server.reset();
                m_lockFile.unlock();
                return false;
            }

            return true;
        }

        QLocalSocket recheck;
        recheck.connectToServer(m_key);
        if (recheck.waitForConnected(kPingTimeoutMs)) {
            recheck.write("activate");
            recheck.waitForBytesWritten(kPingTimeoutMs);
            recheck.disconnectFromServer();
            return false;
        }
    }

    // Could not acquire the lock and could not reach a listener after
    // repeated attempts. Fail closed: do not claim to be primary without a
    // listening server, even in this rare, bounded-time ambiguous case.
    return false;
}

void SingleInstanceGuard::handleNewConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        emit activationRequested();
    }
}
