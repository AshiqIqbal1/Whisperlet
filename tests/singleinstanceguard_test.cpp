#include "singleinstanceguard.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocalServer>
#include <QSignalSpy>
#include <QString>

#include <cstdio>
#include <cstdlib>

#ifndef Q_OS_WIN
#include <cstring>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// A key unique to this test run so it can never collide with a real running
// instance of the app (or a concurrent test run) on the same machine.
QString uniqueKey()
{
    return QStringLiteral("Whisperlet-single-instance-test-%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QDateTime::currentMSecsSinceEpoch());
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // First launch of a given key becomes primary and gets no activation ping.
    {
        const QString key = uniqueKey();
        SingleInstanceGuard first(key);
        check(first.tryAcquire(), "first instance becomes primary");

        // Second launch with the same key should find the first already
        // running, forward it an activation ping, and report it is not
        // primary so main() knows to exit instead of showing a window.
        SingleInstanceGuard second(key);
        QSignalSpy spy(&first, &SingleInstanceGuard::activationRequested);
        check(!second.tryAcquire(), "second instance defers to the running one");
        check(spy.wait(1000), "primary instance is notified to activate its window");
    }

    // A crash (or kill -9) closes the socket's file descriptor without ever
    // calling unlink() on it, so the socket file lingers on disk with
    // nothing listening on it. Reproduce that exact filesystem state — bind
    // a raw socket at the path Qt would use, then close() (not unlink) it —
    // and confirm a fresh launch cleans up the leftover file and becomes
    // primary rather than refusing to start forever.
    //
    // Windows-only: skipped there. QLocalServer uses named pipes, which the
    // OS itself discards when the owning process dies, so there's no
    // leftover artifact to reproduce.
#ifndef Q_OS_WIN
    {
        const QString key = uniqueKey();

        QString socketPath;
        {
            QLocalServer probe;
            check(probe.listen(key), "probe server learns the socket path for this key");
            socketPath = probe.fullServerName();
        } // probe's destructor closes and unlinks it; only the path is kept

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        const QByteArray pathBytes = socketPath.toLocal8Bit();
        check(static_cast<size_t>(pathBytes.size()) < sizeof(addr.sun_path),
              "socket path fits in sockaddr_un");
        std::memcpy(addr.sun_path, pathBytes.constData(), pathBytes.size());

        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        check(fd >= 0, "can open a raw unix socket to fake a crash artifact");
        check(::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0,
              "binds the stale socket file at the guard's expected path");
        ::close(fd); // no unlink(): leaves the socket file behind, like a crash would

        SingleInstanceGuard afterCrash(key);
        check(afterCrash.tryAcquire(), "recovers from a stale lock left by an unclean exit");
    }
#endif

    if (failures == 0)
        std::printf("All single-instance guard tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
