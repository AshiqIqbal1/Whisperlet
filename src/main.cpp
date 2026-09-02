#include "mainwindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
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

    // Give QSettings and QStandardPaths::AppLocalDataLocation a stable home
    // (~/Library/Application Support/Whisperlet on macOS, %LOCALAPPDATA% on
    // Windows). Must happen before anything touches settings or the model dir.
    QCoreApplication::setOrganizationName(QStringLiteral("Whisperlet"));
    QCoreApplication::setApplicationName(QStringLiteral("Whisperlet"));
    a.setWindowIcon(QIcon(QStringLiteral(":/assets/icon-64.png")));

    migrateFromWhisperFlow();

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
    MainWindow w;
    w.show();
    return QApplication::exec();
}
