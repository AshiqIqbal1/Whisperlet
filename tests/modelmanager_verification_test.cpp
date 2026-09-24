// Locks in the model download hardening: every catalog hash is well formed,
// a download is only installed once the bytes that hit the disk match the
// pinned hash and QSaveFile::commit() succeeds, and a model file only
// counts as downloaded with a matching ".verified" sidecar. Downloads run
// against a local stand-in HTTP server; QStandardPaths test mode keeps the
// models directory away from the real one.

#include "modelcatalog.h"
#include "modelmanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>

#include <cstdio>
#include <cstdlib>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/resource.h>
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

const QString kId = QStringLiteral("tiny");

QString sha256Of(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

// Serves the same body to every request it receives.
class StubServer : public QObject
{
public:
    explicit StubServer(const QByteArray &body)
        : m_body(body)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = m_server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                socket->readAll();
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
                              "Content-Length: " + QByteArray::number(m_body.size())
                              + "\r\nConnection: close\r\n\r\n");
                socket->write(m_body);
                socket->disconnectFromHost();
            });
        });
        m_server.listen(QHostAddress::LocalHost);
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/model.bin").arg(m_server.serverPort()));
    }

private:
    QTcpServer m_server;
    QByteArray m_body;
};

void resetModelsDir(const ModelManager &models)
{
    QDir(models.modelsDir()).removeRecursively();
    QDir().mkpath(models.modelsDir());
}

void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(data);
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

bool onlyModelFileIn(const ModelManager &models, const QString &path)
{
    const QStringList entries = QDir(models.modelsDir()).entryList(QDir::Files | QDir::Hidden);
    return entries.isEmpty() || entries == QStringList{QFileInfo(path).fileName()};
}

// Runs one download to completion and returns downloadFinished's arguments.
QList<QVariant> runDownload(ModelManager &models)
{
    QSignalSpy spy(&models, &ModelManager::downloadFinished);
    models.download(kId);
    if (spy.isEmpty() && !spy.wait(10000))
        return {};
    return spy.takeFirst();
}

void testCatalogHashesAreWellFormed()
{
    QSet<QString> ids;
    QSet<QString> hashes;
    for (const ModelInfo &info : ModelCatalog::all()) {
        const QByteArray what = QStringLiteral("catalog sha256 for %1 is 64 lowercase hex digits")
                                    .arg(info.id).toUtf8();
        check(ModelCatalog::isWellFormedSha256(info.sha256), what.constData());
        ids.insert(info.id);
        hashes.insert(info.sha256);
    }
    check(ids.size() == ModelCatalog::all().size(), "catalog ids are unique");
    check(hashes.size() == ModelCatalog::all().size(), "catalog hashes are unique");

    check(!ModelCatalog::isWellFormedSha256(QString()), "empty hash is malformed");
    check(!ModelCatalog::isWellFormedSha256(QString(63, u'a')), "63-digit hash is malformed");
    check(!ModelCatalog::isWellFormedSha256(QString(64, u'A')), "uppercase hash is malformed");
    check(!ModelCatalog::isWellFormedSha256(QString(64, u'g')), "non-hex hash is malformed");
    check(ModelCatalog::isWellFormedSha256(sha256Of("x")), "a real sha256 is well formed");
}

void testSuccessfulDownloadWritesSidecar()
{
    ModelManager models;
    resetModelsDir(models);
    const QByteArray body(300'000, 'm');
    StubServer server(body);
    models.setSourceForTesting(kId, server.url(), sha256Of(body));

    const QList<QVariant> args = runDownload(models);
    check(!args.isEmpty() && args.at(1).toBool(), "matching download reports success");
    check(readFile(models.localPath(kId)) == body, "matching download is installed intact");
    check(QFileInfo::exists(models.localPath(kId) + QStringLiteral(".verified")),
          "matching download writes the sidecar");
    check(models.isDownloaded(kId), "matching download counts as downloaded");
}

void testHashMismatchIsRejected()
{
    ModelManager models;
    resetModelsDir(models);
    const QByteArray body(300'000, 'm');
    StubServer server(body);
    models.setSourceForTesting(kId, server.url(), sha256Of("something else"));

    const QList<QVariant> args = runDownload(models);
    check(!args.isEmpty() && !args.at(1).toBool(), "mismatched download reports failure");
    check(!QFileInfo::exists(models.localPath(kId)), "mismatched download is not installed");
    check(onlyModelFileIn(models, models.localPath(kId)), "mismatched download leaves no temp file");
    check(!models.isDownloaded(kId), "mismatched download does not count as downloaded");
}

void testFailedDownloadKeepsPreviousModel()
{
    ModelManager models;
    resetModelsDir(models);
    const QByteArray good(1000, 'g');
    const QByteArray bad(300'000, 'b');

    StubServer goodServer(good);
    models.setSourceForTesting(kId, goodServer.url(), sha256Of(good));
    runDownload(models);
    check(models.isDownloaded(kId), "first download installed");

    StubServer badServer(bad);
    models.setSourceForTesting(kId, badServer.url(), sha256Of(good));
    const QList<QVariant> args = runDownload(models);
    check(!args.isEmpty() && !args.at(1).toBool(), "bad re-download reports failure");
    check(readFile(models.localPath(kId)) == good, "bad re-download leaves previous model untouched");
    check(models.isDownloaded(kId), "previous model still counts as downloaded");
}

void testMissingOrMalformedHashFailsClosed()
{
    ModelManager models;
    resetModelsDir(models);
    const QByteArray body(1000, 'm');
    StubServer server(body);

    for (const QString &hash : {QString(), QStringLiteral("not-a-hash")}) {
        models.setSourceForTesting(kId, server.url(), hash);
        const QList<QVariant> args = runDownload(models);
        check(!args.isEmpty() && !args.at(1).toBool(), "download without a valid pinned hash is refused");
        check(!QFileInfo::exists(models.localPath(kId)), "nothing installed without a valid pinned hash");
    }

    // A file plus a sidecar that echoes the (empty) expected hash still
    // doesn't count.
    models.setSourceForTesting(kId, server.url(), QString());
    writeFile(models.localPath(kId), body);
    writeFile(models.localPath(kId) + QStringLiteral(".verified"), R"({"sha256":"","size":1000})");
    check(!models.isDownloaded(kId), "empty pinned hash never counts as downloaded");
    check(!models.verifyLocalFile(kId), "empty pinned hash never verifies");
}

#ifdef Q_OS_UNIX
// RLIMIT_FSIZE makes any write past the limit fail (EFBIG), the same short
// write a full disk produces, without needing a full disk.
void testWriteFailureIsRejected()
{
    if (geteuid() == 0)
        return; // root isn't bound by the limit the same way on every OS

    ModelManager models;
    resetModelsDir(models);
    const QByteArray body(4 * 1024 * 1024, 'w');
    StubServer server(body);
    models.setSourceForTesting(kId, server.url(), sha256Of(body));

    struct rlimit old {};
    getrlimit(RLIMIT_FSIZE, &old);
    struct rlimit capped = old;
    capped.rlim_cur = 64 * 1024;
    std::signal(SIGXFSZ, SIG_IGN);
    setrlimit(RLIMIT_FSIZE, &capped);

    const QList<QVariant> args = runDownload(models);

    setrlimit(RLIMIT_FSIZE, &old);

    check(!args.isEmpty() && !args.at(1).toBool(), "short-written download reports failure");
    check(!args.isEmpty() && !args.at(2).toString().contains(QStringLiteral("Checksum")),
          "short write is reported as a write error, not a checksum mismatch");
    check(!QFileInfo::exists(models.localPath(kId)), "short-written download is not installed");
    check(onlyModelFileIn(models, models.localPath(kId)), "short-written download leaves no temp file");
    check(!models.isDownloaded(kId), "short-written download does not count as downloaded");
}
#endif

void testSidecarGate()
{
    ModelManager models;
    resetModelsDir(models);
    const QByteArray body(1000, 's');
    const QString hash = sha256Of(body);
    models.setSourceForTesting(kId, QUrl(), hash);
    const QString path = models.localPath(kId);
    const QString sidecar = path + QStringLiteral(".verified");

    writeFile(path, body);
    check(!models.isDownloaded(kId), "file without sidecar does not count as downloaded");
    check(models.needsVerification(kId), "file without sidecar needs verification");

    writeFile(sidecar, QStringLiteral(R"({"sha256":"%1","size":999})").arg(hash).toUtf8());
    check(!models.isDownloaded(kId), "sidecar with the wrong size is rejected");
    check(!models.needsVerification(kId), "a present sidecar is not silently re-verified");

    writeFile(sidecar, QStringLiteral(R"({"sha256":"%1","size":1000})").arg(sha256Of("x")).toUtf8());
    check(!models.isDownloaded(kId), "sidecar with the wrong hash is rejected");

    writeFile(sidecar, "not json");
    check(!models.isDownloaded(kId), "garbage sidecar is rejected");

    writeFile(sidecar, QStringLiteral(R"({"sha256":"%1","size":1000})").arg(hash).toUtf8());
    check(models.isDownloaded(kId), "matching sidecar counts as downloaded");

    // File changed size after verification (truncated, appended to).
    writeFile(path, body + "x");
    check(!models.isDownloaded(kId), "file that changed size after verification is rejected");

    models.removeDownloaded(kId);
    check(!QFileInfo::exists(path) && !QFileInfo::exists(sidecar), "removing a model removes its sidecar");
}

// The migration case: a file from before sidecars existed.
void testVerifyLocalFile()
{
    ModelManager models;
    resetModelsDir(models);
    const QByteArray body(1000, 'v');
    models.setSourceForTesting(kId, QUrl(), sha256Of(body));
    const QString path = models.localPath(kId);

    writeFile(path, body);
    check(models.verifyLocalFile(kId), "matching legacy file verifies");
    check(QFileInfo::exists(path + QStringLiteral(".verified")), "verifying writes the sidecar");
    check(models.isDownloaded(kId), "verified legacy file counts as downloaded");
    check(!models.needsVerification(kId), "verified legacy file isn't hashed again");

    models.removeDownloaded(kId);
    writeFile(path, QByteArray(1000, 'x'));
    check(!models.verifyLocalFile(kId), "mismatching legacy file fails verification");
    check(!QFileInfo::exists(path + QStringLiteral(".verified")), "mismatching legacy file gets no sidecar");
    check(!models.isDownloaded(kId), "mismatching legacy file does not count as downloaded");

    // Real catalog hash: arbitrary bytes never pass as a real model.
    ModelManager real;
    resetModelsDir(real);
    writeFile(real.localPath(kId), body);
    check(!real.verifyLocalFile(kId), "arbitrary file never verifies against the real catalog");
}
} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("WhisperletTest"));
    QCoreApplication::setApplicationName(QStringLiteral("ModelManagerVerificationTest"));

    testCatalogHashesAreWellFormed();
    testSuccessfulDownloadWritesSidecar();
    testHashMismatchIsRejected();
    testFailedDownloadKeepsPreviousModel();
    testMissingOrMalformedHashFailsClosed();
#ifdef Q_OS_UNIX
    testWriteFailureIsRejected();
#endif
    testSidecarGate();
    testVerifyLocalFile();

    QDir(ModelManager().modelsDir()).removeRecursively();

    if (failures == 0)
        std::printf("All model manager verification tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
