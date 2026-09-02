#include "modelmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr auto kSettingsKey = "activeModelId";
constexpr QLatin1StringView kDefaultModel("base");
}

ModelManager::ModelManager(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    QDir().mkpath(modelsDir());
}

QString ModelManager::modelsDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(base).filePath(QStringLiteral("models"));
}

QString ModelManager::localPath(const QString &id) const
{
    const ModelInfo *info = ModelCatalog::find(id);
    if (!info)
        return QString();
    return QDir(modelsDir()).filePath(info->filename);
}

bool ModelManager::isDownloaded(const QString &id) const
{
    const QString path = localPath(id);
    return !path.isEmpty() && QFileInfo::exists(path);
}

QString ModelManager::activeModelId() const
{
    return QSettings().value(kSettingsKey, QString(kDefaultModel)).toString();
}

void ModelManager::setActiveModelId(const QString &id)
{
    if (id == activeModelId())
        return;
    QSettings().setValue(kSettingsKey, id);
    emit activeModelChanged(id);
}

bool ModelManager::isDownloading(const QString &id) const
{
    return m_downloads.contains(id);
}

void ModelManager::download(const QString &id)
{
    if (m_downloads.contains(id))
        return;

    const ModelInfo *info = ModelCatalog::find(id);
    if (!info) {
        emit downloadFinished(id, false, tr("Unknown model \"%1\"").arg(id));
        return;
    }

    QDir().mkpath(modelsDir());

    const QString finalPath = localPath(id);
    const QString tmpPath = finalPath + QStringLiteral(".part");

    auto *file = new QFile(tmpPath, this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete file;
        emit downloadFinished(id, false, tr("Could not write to %1").arg(tmpPath));
        return;
    }

    QNetworkRequest request(QUrl(ModelCatalog::downloadUrl(*info)));
    // Hugging Face serves the actual bytes from a CDN redirect; this policy
    // follows https->https redirects but still refuses a downgrade to http.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_net->get(request);
    m_downloads.insert(id, {reply, file, tmpPath,
                            std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256)});

    connect(reply, &QNetworkReply::readyRead, this, [this, id] {
        auto it = m_downloads.find(id);
        if (it != m_downloads.end()) {
            const QByteArray chunk = it->reply->readAll();
            it->file->write(chunk);
            it->hash->addData(chunk);
        }
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, id](qint64 received, qint64 total) {
                emit downloadProgress(id, received, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, id, finalPath] {
        auto it = m_downloads.find(id);
        if (it == m_downloads.end())
            return; // already cleaned up via cancel

        QNetworkReply *reply = it->reply;
        QFile *file = it->file;
        const QString tmpPath = it->tmpPath;
        bool ok = reply->error() == QNetworkReply::NoError;
        QString error = ok ? QString() : reply->errorString();

        // Verify against the hash pinned in the catalog: a file that made it
        // through TLS but doesn't match upstream is corrupt or tampered —
        // either way it never reaches the models directory.
        if (ok) {
            const ModelInfo *info = ModelCatalog::find(id);
            const QString got = QString::fromLatin1(it->hash->result().toHex());
            if (info && !info->sha256.isEmpty() && got != info->sha256) {
                ok = false;
                error = tr("Checksum mismatch. The downloaded file does not match "
                           "the published model. It was discarded; try again.");
            }
        }

        file->close();
        if (ok) {
            QFile::remove(finalPath); // in case a previous file is there
            QFile::rename(tmpPath, finalPath);
        } else {
            QFile::remove(tmpPath);
        }

        reply->deleteLater();
        file->deleteLater();
        m_downloads.remove(id);

        emit downloadFinished(id, ok, error);
    });
}

void ModelManager::cancelDownload(const QString &id)
{
    auto it = m_downloads.find(id);
    if (it != m_downloads.end())
        it->reply->abort(); // finished() handler above does the cleanup
}

void ModelManager::removeDownloaded(const QString &id)
{
    QFile::remove(localPath(id));
}
