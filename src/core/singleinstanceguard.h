#ifndef SINGLEINSTANCEGUARD_H
#define SINGLEINSTANCEGUARD_H

#include <QObject>
#include <QLockFile>
#include <QString>

#include <memory>

class QLocalServer;

// Keeps a second launch of the app from starting alongside a running one.
// The first instance to call tryAcquire() atomically claims a QLockFile and
// then listens on a named local socket, becoming primary; any later instance
// finds the lock already held, forwards the primary a one-shot "activate"
// ping, and should exit without showing a window. If the primary instance
// crashed and left its lock file behind (a stale lock), QLockFile detects
// that the owning process is gone and reclaims it atomically, so tryAcquire()
// can take over rather than refusing to start forever.
class SingleInstanceGuard : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstanceGuard(const QString &key, QObject *parent = nullptr);
    ~SingleInstanceGuard() override;

    // Returns true if this process became (or already is) the primary
    // instance. Returns false if another instance is already running, in
    // which case that instance has been sent an activation ping.
    bool tryAcquire();

signals:
    // Emitted on the primary instance whenever a later launch pings it.
    void activationRequested();

private slots:
    void handleNewConnection();

private:
    QString m_key;
    QLockFile m_lockFile;
    std::unique_ptr<QLocalServer> m_server;
};

#endif // SINGLEINSTANCEGUARD_H
