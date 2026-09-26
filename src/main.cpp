#include "cpufeatures.h"
#include "mainwindow.h"
#include "singleinstanceguard.h"
#include "theme.h"
#include "version.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTranslator>

namespace {

// The app used to be called WhisperFlow. If a data dir from that name is
// still around and the new one doesn't exist yet, move it over so models
// (up to 1.6GB of downloads) and transcripts survive the rename. Settings
// are copied too. Safe to delete a release or two after the rename.
void migrateFromWhisperFlow()
{
    const QString newDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString oldDir = newDir;
    oldDir.replace(QStringLiteral("Whisperlet"), QStringLiteral("WhisperFlow"));

    if (oldDir != newDir && QDir(oldDir).exists() && !QDir(newDir).exists()) {
        QDir().mkpath(QFileInfo(newDir).path());
        QDir().rename(oldDir, newDir);
    }

    QSettings newSettings;
    if (newSettings.contains(QStringLiteral("activeModelId")))
        return; // already migrated (or fresh install that made its own choices)

    QSettings oldSettings(QStringLiteral("WhisperFlow"), QStringLiteral("WhisperFlow"));
    const QStringList keys = oldSettings.allKeys();
    for (const QString &key : keys)
        newSettings.setValue(key, oldSettings.value(key));
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Theme::pinDarkColorScheme();

    // Give QSettings and QStandardPaths::AppLocalDataLocation a stable home
    // (~/Library/Application Support/Whisperlet on macOS, %LOCALAPPDATA% on
    // Windows). Must happen before anything touches settings or the model dir.
    QCoreApplication::setOrganizationName(QStringLiteral("Whisperlet"));
    QCoreApplication::setApplicationName(QStringLiteral("Whisperlet"));
    QCoreApplication::setApplicationVersion(QStringLiteral(WHISPERLET_VERSION_STRING));
    a.setWindowIcon(QIcon(QStringLiteral(":/assets/icon-64.png")));

    migrateFromWhisperFlow();

    // Refuse to run a second instance side by side; instead bring the
    // running one to the front. Prevents duplicate global hotkeys, duplicate
    // access to the same recordings/model files, and confusing double tray
    // icons if a user double-clicks the app (or launches at login) while
    // it's already running.
    SingleInstanceGuard singleInstanceGuard(QStringLiteral("Whisperlet-single-instance"));
    if (!singleInstanceGuard.tryAcquire())
        return 0;

    // Everything we store (transcripts, recordings, models) lives here.
    // Qt creates directories world readable by default; on a shared machine
    // that would let any other local account read the user's dictation.
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    QFile::setPermissions(dataDir, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                       | QFileDevice::ExeOwner);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "Whisperlet_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

#ifdef Q_OS_WIN
    // ggml's CPU kernels are built for an AVX2 baseline (CMakeLists.txt).
    // MainWindow preloads the model straight away, which on an older CPU
    // is an illegal instruction crash with no message, so refuse here with
    // one instead (#69).
    const std::vector<const char *> missing = missingCpuFeatures(readCpuIdInfo());
    if (!missing.empty()) {
        QStringList names;
        for (const char *name : missing)
            names << QString::fromLatin1(name);
        QMessageBox::critical(
            nullptr, QCoreApplication::translate("main", "Unsupported processor"),
            QCoreApplication::translate(
                "main",
                "Whisperlet needs a processor with AVX2 support, which most Intel "
                "and AMD processors from 2013 onwards have, but many Pentium, "
                "Celeron and Atom models do not.\n\nThis processor is missing: %1.")
                .arg(names.join(QStringLiteral(", "))));
        return 1;
    }
#endif

    MainWindow w;
    QObject::connect(&singleInstanceGuard, &SingleInstanceGuard::activationRequested, &w, [&w]() {
        w.setWindowState((w.windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        w.show();
        w.raise();
        w.activateWindow();
    });
    w.show();
    return QApplication::exec();
}
