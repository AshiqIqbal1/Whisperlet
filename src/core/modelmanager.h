#ifndef MODELMANAGER_H
#define MODELMANAGER_H

#include "modelcatalog.h"

#include <QCryptographicHash>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QUrl>

#include <memory>

class QSaveFile;
class QNetworkAccessManager;
class QNetworkReply;

// Owns the on-disk model cache and the "which model is active" setting.
// Downloading is streamed straight to disk (never buffered fully in RAM —
// the large model is 1.5GB+) and survives app restarts: whatever finished
// downloading last time is still there, whatever didn't gets cleaned up.
//
// A model only counts as downloaded once it has a "<file>.verified"
// sidecar recording the sha256 and size it had when it was hashed, and that
// sha256 is the catalog's. The file alone is never trusted: whisper.cpp does
// little validation of what it loads.
class ModelManager : public QObject
{
    Q_OBJECT

public:
    explicit ModelManager(QObject *parent = nullptr);

    // Directory models are cached in: <AppLocalDataLocation>/models/
    QString modelsDir() const;

    // File present, sidecar present, and the sidecar matches both the
    // catalog hash and the file's current size. Cheap: never hashes.
    bool isDownloaded(const QString &id) const;
    QString localPath(const QString &id) const;

    // The model file is there but has no sidecar yet, e.g. it was carried
    // over from an older version. verifyLocalFile() settles it.
    bool needsVerification(const QString &id) const;

    // Hashes the local file once and records the result in the sidecar, so
    // a mismatching file stays not-downloaded without being hashed again on
    // every launch. Returns isDownloaded(id) afterwards. Blocking (a full read of
    // up to 1.6GB), so call it off the UI thread; it touches no QObject state.
    bool verifyLocalFile(const QString &id) const;

    QString activeModelId() const;
    void setActiveModelId(const QString &id);

    bool isDownloading(const QString &id) const;

#ifdef WHISPERLET_TESTING
    // Serve a model from somewhere else and pin a different hash for it,
    // so tests can drive the real download path against a local server.
    void setSourceForTesting(const QString &id, const QUrl &url, const QString &sha256);
#endif

public slots:
    void download(const QString &id);
    void cancelDownload(const QString &id);
    void removeDownloaded(const QString &id);

signals:
    void downloadProgress(const QString &id, qint64 received, qint64 total);
    void downloadFinished(const QString &id, bool ok, const QString &error);
    void activeModelChanged(const QString &id);

private:
    struct DownloadState
    {
        QNetworkReply *reply = nullptr;
        // Writes to a temp file and only replaces the model on commit(),
        // which fails if any write along the way did.
        QSaveFile *file = nullptr;
        // Hashed incrementally as chunks stream in, checked on finish.
        std::shared_ptr<QCryptographicHash> hash;
        qint64 written = 0;
        QString writeError;
    };

    const QString &expectedSha256(const ModelInfo &info) const;

    QNetworkAccessManager *m_net = nullptr;
    QMap<QString, DownloadState> m_downloads;

#ifdef WHISPERLET_TESTING
    struct TestSource
    {
        QUrl url;
        QString sha256;
    };
    QHash<QString, TestSource> m_testSources;
#endif
};

#endif // MODELMANAGER_H
