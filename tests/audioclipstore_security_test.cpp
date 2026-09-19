#include "audioclipstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Whisperlet"));
    QCoreApplication::setApplicationName(QStringLiteral("AudioClipStoreSecurityTest"));

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString audioDir = QDir(dataDir).filePath(QStringLiteral("audio"));
    QDir(dataDir).removeRecursively();

    const std::vector<float> samples = {0.1f, -0.2f, 0.3f};

    // A malicious id trying to escape the audio directory must never
    // resolve to (or be able to create) a file outside it.
    const QString escapeTarget = QDir(dataDir).filePath(QStringLiteral("escaped.wav"));
    QFile::remove(escapeTarget);

    const QString maliciousId = QStringLiteral("../escaped");
    check(AudioClipStore::path(maliciousId).isEmpty(), "path() rejects an id containing '..'");
    check(!AudioClipStore::save(maliciousId, samples, 16000), "save() refuses an id containing '..'");
    check(!QFile::exists(escapeTarget), "save() did not create a file outside the audio directory");
    check(!AudioClipStore::exists(maliciousId), "exists() reports false for a traversal id");
    check(AudioClipStore::load(maliciousId).empty(), "load() returns empty for a traversal id");
    AudioClipStore::remove(maliciousId); // must not throw/crash and must not touch escapeTarget
    check(!QFile::exists(escapeTarget), "remove() did not touch a file outside the audio directory");

    // Absolute-path and separator-bearing ids are rejected the same way.
    check(AudioClipStore::path(QStringLiteral("/etc/passwd")).isEmpty(),
          "path() rejects an absolute-path id");
    check(AudioClipStore::path(QStringLiteral("sub/dir")).isEmpty(),
          "path() rejects an id containing a path separator");
    check(AudioClipStore::path(QStringLiteral("back\\slash")).isEmpty(),
          "path() rejects an id containing a backslash");
    check(AudioClipStore::path(QString()).isEmpty(), "path() rejects an empty id");

    // A normal id still works exactly as before.
    const QString goodId = QStringLiteral("normal-id-123");
    check(AudioClipStore::save(goodId, samples, 16000), "save() accepts a well-formed id");
    check(AudioClipStore::exists(goodId), "exists() finds a clip saved under a well-formed id");
    check(!AudioClipStore::load(goodId).empty(), "load() reads back a clip saved under a well-formed id");
    check(QFile::exists(QDir(audioDir).filePath(goodId + QStringLiteral(".wav"))),
          "well-formed id resolves inside the audio directory");
    AudioClipStore::remove(goodId);
    check(!AudioClipStore::exists(goodId), "remove() deletes a clip saved under a well-formed id");

    QDir(dataDir).removeRecursively();

    if (failures == 0)
        std::printf("All audio clip store security tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
