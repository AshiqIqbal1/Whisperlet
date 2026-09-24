#include "modelmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr auto kSettingsKey = "activeModelId";
constexpr QLatin1StringView kDefaultModel("base");

QString sidecarPath(const QString &modelPath)
{
    return modelPath + QStringLiteral(".verified");
}

bool writeSidecar(const QString &modelPath, const QString &sha256, qint64 size)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("sha256"), sha256);
    obj.insert(QStringLiteral("size"), size);

    QSaveFile out(sidecarPath(modelPath));
    if (!out.open(QIODevice::WriteOnly))
        return false;
    const QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    if (out.write(json) != json.size()) {
        out.cancelWriting();
        return false;
    }
    return out.commit();
}

// The sidecar is only ever written after the file was hashed and records
// what it hashed to, so matching the catalog hash (and the file still being that exact size) is
// what "downloaded" means. An empty or malformed expected hash never
// matches: no pinned hash means nothing can be trusted, not everything.
bool sidecarMatches(const QString &modelPath, const QString &expectedSha256)
{
    if (!ModelCatalog::isWellFormedSha256(expectedSha256))
        return false;

    const QFileInfo fi(modelPath);
    if (!fi.isFile())
        return false;

    QFile in(sidecarPath(modelPath));
    if (!in.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject obj = QJsonDocument::fromJson(in.read(4096)).object();
    const QJsonValue size = obj.value(QStringLiteral("size"));
    return obj.value(QStringLiteral("sha256")).toString() == expectedSha256
        && size.isDouble() && qint64(size.toDouble()) == fi.size();
}
} // namespace

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

const QString &ModelManager::expectedSha256(const ModelInfo &info) const
{
#ifdef WHISPERLET_TESTING
    const auto it = m_testSources.constFind(info.id);
    if (it != m_testSources.cend())
        return it->sha256;
#endif
    return info.sha256;
}

bool ModelManager::isDownloaded(const QString &id) const
{
    const ModelInfo *info = ModelCatalog::find(id);
    return info && sidecarMatches(localPath(id), expectedSha256(*info));
}

bool ModelManager::needsVerification(const QString &id) const
{
    const QString path = localPath(id);
    return !path.isEmpty() && QFileInfo(path).isFile() && !QFileInfo::exists(sidecarPath(path));
}

bool ModelManager::verifyLocalFile(const QString &id) const
{
    if (!needsVerification(id))
        return isDownloaded(id);

    const ModelInfo *info = ModelCatalog::find(id);
    const QString expected = expectedSha256(*info);
    if (!ModelCatalog::isWellFormedSha256(expected))
        return false;

    const QString path = localPath(id);
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly))
        return false;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 size = 0;
    QByteArray buf(1 << 20, Qt::Uninitialized);
    for (;;) {
        const qint64 n = in.read(buf.data(), buf.size());
        if (n < 0)
            return false;
        if (n == 0)
            break;
        hash.addData(QByteArrayView(buf.constData(), n));
        size += n;
    }

    return writeSidecar(path, QString::fromLatin1(hash.result().toHex()), size) && isDownloaded(id);
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

#ifdef WHISPERLET_TESTING
void ModelManager::setSourceForTesting(const QString &id, const QUrl &url, const QString &sha256)
{
    m_testSources.insert(id, {url, sha256});
}
#endif

void ModelManager::download(const QString &id)
{
    if (m_downloads.contains(id))
        return;

    const ModelInfo *info = ModelCatalog::find(id);
    if (!info) {
        emit downloadFinished(id, false, tr("Unknown model \"%1\"").arg(id));
        return;
    }
    if (!ModelCatalog::isWellFormedSha256(expectedSha256(*info))) {
        emit downloadFinished(id, false,
                              tr("Model \"%1\" has no valid checksum to verify it against.").arg(id));
        return;
    }

    QDir().mkpath(modelsDir());

    const QString finalPath = localPath(id);

    auto *file = new QSaveFile(finalPath, this);
    if (!file->open(QIODevice::WriteOnly)) {
        delete file;
        emit downloadFinished(id, false, tr("Could not write to %1").arg(finalPath));
        return;
    }

    QUrl url(ModelCatalog::downloadUrl(*info));
#ifdef WHISPERLET_TESTING
    if (const auto it = m_testSources.constFind(id); it != m_testSources.cend())
        url = it->url;
#endif
    QNetworkRequest request(url);
    // Hugging Face serves the actual bytes from a CDN redirect; this policy
    // follows https->https redirects but still refuses a downgrade to http.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_net->get(request);
    m_downloads.insert(id, {reply, file,
                            std::make_shared<QCryptographicHash>(QCryptographicHash::Sha256)});

    // Only bytes that actually landed in the file are hashed. A short write
    // (full disk, say) stops the download right there instead of letting a
    // truncated file carry on to the checksum.
    const auto consume = [](DownloadState &st) {
        if (!st.writeError.isEmpty())
            return false;
        const QByteArray chunk = st.reply->readAll();
        if (st.file->write(chunk) != chunk.size()) {
            st.writeError = st.file->errorString();
            st.file->cancelWriting();
            return false;
        }
        st.hash->addData(chunk);
        st.written += chunk.size();
        return true;
    };

    connect(reply, &QNetworkReply::readyRead, this, [this, id, consume] {
        auto it = m_downloads.find(id);
        if (it != m_downloads.end() && !consume(*it))
            it->reply->abort(); // finished() handler below reports writeError
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, id](qint64 received, qint64 total) {
                emit downloadProgress(id, received, total);
            });

    connect(reply, &QNetworkReply::finished, this, [this, id, finalPath, consume] {
        auto it = m_downloads.find(id);
        if (it == m_downloads.end())
            return; // already cleaned up via cancel

        QNetworkReply *reply = it->reply;
        QSaveFile *file = it->file;
        bool ok = reply->error() == QNetworkReply::NoError && consume(*it);
        QString error;
        if (!it->writeError.isEmpty())
            error = tr("Could not write the model to disk: %1").arg(it->writeError);
        else if (!ok)
            error = reply->errorString();

        // Verify against the hash pinned in the catalog: a file that made it
        // through TLS but doesn't match upstream is corrupt or tampered —
        // either way it never reaches the models directory.
        const QString got = QString::fromLatin1(it->hash->result().toHex());
        if (ok) {
            const ModelInfo *info = ModelCatalog::find(id);
            if (!info || got != expectedSha256(*info)) {
                ok = false;
                error = tr("Checksum mismatch. The downloaded file does not match "
                           "the published model. It was discarded; try again.");
            }
        }

        // commit() atomically replaces the model file and fails if any
        // write, flush or the rename itself did; until it succeeds the old
        // file (if any) is untouched. Only then is the sidecar written.
        if (ok) {
            if (!file->commit()) {
                ok = false;
                error = tr("Could not save the model to %1: %2").arg(finalPath, file->errorString());
            } else if (!writeSidecar(finalPath, got, it->written)) {
                ok = false;
                error = tr("Could not record the verified model next to %1").arg(finalPath);
            }
        } else {
            file->cancelWriting();
        }

        reply->deleteLater();
        delete file; // removes the temp file now if nothing was committed
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
    const QString path = localPath(id);
    if (path.isEmpty())
        return;
    QFile::remove(sidecarPath(path));
    QFile::remove(path);
}
