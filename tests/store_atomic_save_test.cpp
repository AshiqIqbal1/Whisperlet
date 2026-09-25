// Both stores write the user's data in place: a failed or interrupted write
// must never report success or leave a damaged file behind, and a damaged
// history file must not be read as "no history" (and then overwritten).

#include "audioclipstore.h"
#include "transcriptstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

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

QString dataDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString historyPath()
{
    return QDir(dataDir()).filePath(QStringLiteral("transcripts.json"));
}

void resetDataDir()
{
    QDir(dataDir()).removeRecursively();
    QDir().mkpath(dataDir());
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(bytes);
}

QStringList corruptCopies()
{
    return QDir(dataDir()).entryList({QStringLiteral("transcripts.json.corrupt-*")}, QDir::Files);
}

QList<Transcript> sampleHistory(int count, int textLength)
{
    QList<Transcript> list;
    for (int i = 0; i < count; ++i) {
        Transcript t;
        t.id = QStringLiteral("id-%1").arg(i);
        t.text = QString(textLength, QLatin1Char('a' + i % 26));
        t.when = QDateTime(QDate(2026, 1, 1), QTime(12, 0));
        t.durationSec = i;
        list.append(t);
    }
    return list;
}

void testRoundTrip()
{
    resetDataDir();
    check(TranscriptStore::load().isEmpty(), "missing history file loads as empty history");
    check(corruptCopies().isEmpty(), "missing history file is not treated as damaged");

    const QList<Transcript> history = sampleHistory(3, 10);
    check(TranscriptStore::save(history), "save() of a small history succeeds");
    const QList<Transcript> loaded = TranscriptStore::load();
    check(loaded.size() == 3 && loaded.at(2).text == history.at(2).text, "saved history loads back");
    check(QFileInfo(historyPath()).permissions()
              == (QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser),
          "history file is owner read/write only");

    check(AudioClipStore::save(QStringLiteral("clip"), {0.1f, -0.2f, 0.3f}, 16000),
          "save() of a small clip succeeds");
    check(AudioClipStore::load(QStringLiteral("clip")).size() == 3, "saved clip loads back");
}

void testDamagedHistoryIsMovedAside(const QByteArray &damaged, const char *what)
{
    resetDataDir();
    writeFile(historyPath(), damaged);

    check(TranscriptStore::load().isEmpty(), what);
    const QStringList copies = corruptCopies();
    check(copies.size() == 1, "damaged history is moved aside to transcripts.json.corrupt-<timestamp>");
    check(!copies.isEmpty() && readFile(QDir(dataDir()).filePath(copies.first())) == damaged,
          "moved-aside copy holds the damaged bytes unchanged");
    check(!QFile::exists(historyPath()), "damaged history is no longer at transcripts.json");

    // The next save starts a new file and leaves the damaged one alone.
    check(TranscriptStore::save(sampleHistory(1, 5)), "save() after a damaged load succeeds");
    check(!copies.isEmpty() && readFile(QDir(dataDir()).filePath(copies.first())) == damaged,
          "save() does not overwrite the moved-aside history");
    check(TranscriptStore::load().size() == 1, "new history loads after the damaged one was moved aside");
}

#ifdef Q_OS_UNIX
// RLIMIT_FSIZE makes any write past the limit fail (EFBIG), the same short
// write a full disk produces, without needing a full disk.
template <typename Fn>
auto withFileSizeLimit(rlim_t limit, Fn fn)
{
    struct rlimit old {};
    getrlimit(RLIMIT_FSIZE, &old);
    struct rlimit capped = old;
    capped.rlim_cur = limit;
    std::signal(SIGXFSZ, SIG_IGN);
    setrlimit(RLIMIT_FSIZE, &capped);
    auto result = fn();
    setrlimit(RLIMIT_FSIZE, &old);
    return result;
}

void testFailedHistoryWriteKeepsPreviousFile()
{
    resetDataDir();
    check(TranscriptStore::save(sampleHistory(2, 10)), "initial history save succeeds");
    const QByteArray before = readFile(historyPath());

    const bool saved = withFileSizeLimit(16 * 1024, [] {
        return TranscriptStore::save(sampleHistory(50, 1024));
    });

    check(!saved, "short-written history save reports failure");
    check(readFile(historyPath()) == before, "short-written history save leaves the previous file intact");
    check(TranscriptStore::load().size() == 2, "previous history still loads after a failed save");
    check(QDir(dataDir()).entryList(QDir::Files) == QStringList{QStringLiteral("transcripts.json")},
          "short-written history save leaves no temp file");
}

void testFailedClipWriteReportsFailure()
{
    resetDataDir();
    const QString id = QStringLiteral("clip");
    const std::vector<float> small = {0.5f, -0.5f};
    check(AudioClipStore::save(id, small, 16000), "initial clip save succeeds");
    const QByteArray before = readFile(AudioClipStore::path(id));

    const std::vector<float> big(64 * 1024, 0.25f);
    const bool saved = withFileSizeLimit(16 * 1024, [&] {
        return AudioClipStore::save(id, big, 16000);
    });

    check(!saved, "short-written clip save reports failure");
    check(readFile(AudioClipStore::path(id)) == before, "short-written clip save leaves the previous clip intact");

    const QString fresh = QStringLiteral("fresh");
    const bool freshSaved = withFileSizeLimit(16 * 1024, [&] {
        return AudioClipStore::save(fresh, big, 16000);
    });
    check(!freshSaved, "short-written new clip reports failure");
    check(!AudioClipStore::exists(fresh), "short-written new clip leaves no truncated file");
    check(QFileInfo(AudioClipStore::path(id)).dir().entryList(QDir::Files) == QStringList{id + QStringLiteral(".wav")},
          "short-written clip saves leave no temp file");
}
#endif
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("WhisperletTest"));
    QCoreApplication::setApplicationName(QStringLiteral("StoreAtomicSaveTest"));

    testRoundTrip();
    testDamagedHistoryIsMovedAside(QByteArray(), "empty history file loads as empty history");
    testDamagedHistoryIsMovedAside(QByteArrayLiteral("[\n    {\n        \"id\": \"abc\",\n        \"te"),
                                   "truncated history file loads as empty history");
    testDamagedHistoryIsMovedAside(QByteArrayLiteral("{\"id\": \"abc\"}"),
                                   "non-array history file loads as empty history");
#ifdef Q_OS_UNIX
    if (geteuid() != 0) { // root isn't bound by the limit the same way on every OS
        testFailedHistoryWriteKeepsPreviousFile();
        testFailedClipWriteReportsFailure();
    }
#endif

    QDir(dataDir()).removeRecursively();

    if (failures == 0)
        std::printf("All store atomic save tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
