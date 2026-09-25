#include "audioclipstore.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtDebug>
#include <QtEndian>

#include <algorithm>

namespace {

constexpr quint16 kChannels = 1;
constexpr quint16 kBitsPerSample = 16;

QString audioDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString dir = QDir(base).filePath(QStringLiteral("audio"));
    QDir().mkpath(dir);
    return dir;
}

// Ids end up on disk verbatim (transcripts.json, drag-and-drop, ids echoed
// back through Qt signals), so treat them as untrusted input: reject
// anything that could turn `id + ".wav"` into a path that escapes
// audioDir() rather than trying to sanitize it, since silently stripping
// characters would just create id collisions.
bool isSafeId(const QString &id)
{
    if (id.isEmpty())
        return false;
    if (id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('\\')))
        return false;
    if (id.contains(QStringLiteral("..")))
        return false;
    if (id.contains(QChar(0)))
        return false;
    return true;
}

} // namespace

QString AudioClipStore::path(const QString &id)
{
    if (!isSafeId(id))
        return QString();
    return QDir(audioDir()).filePath(id + QStringLiteral(".wav"));
}

bool AudioClipStore::exists(const QString &id)
{
    if (!isSafeId(id))
        return false;
    return QFile::exists(path(id));
}

bool AudioClipStore::save(const QString &id, const std::vector<float> &samples, int rate)
{
    if (!isSafeId(id))
        return false;

    // Written to a temp file and only swapped in on commit(), so a failed
    // or interrupted write never leaves a truncated clip behind.
    QSaveFile file(path(id));
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning("AudioClipStore: cannot write %s: %s",
                 qPrintable(file.fileName()), qPrintable(file.errorString()));
        return false;
    }
    // Recordings of the user's voice: owner only, same reasoning as the
    // transcript store.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    const quint32 kSampleRate = quint32(rate > 0 ? rate : 16000);
    const quint32 dataBytes = quint32(samples.size()) * sizeof(qint16);
    const quint32 byteRate = kSampleRate * kChannels * kBitsPerSample / 8;

    // Canonical 44-byte PCM WAV header.
    bool ok = file.write("RIFF", 4) == 4;
    out << quint32(36 + dataBytes);
    ok = ok && file.write("WAVE", 4) == 4;
    ok = ok && file.write("fmt ", 4) == 4;
    out << quint32(16);                                   // fmt chunk size
    out << quint16(1);                                    // PCM
    out << kChannels;
    out << kSampleRate;
    out << byteRate;
    out << quint16(kChannels * kBitsPerSample / 8);       // block align
    out << kBitsPerSample;
    ok = ok && file.write("data", 4) == 4;
    out << dataBytes;

    // Convert into one buffer and write once - pushing samples through
    // QDataStream one at a time costs hundreds of ms per minute of audio.
    QByteArray pcm(qsizetype(samples.size()) * qsizetype(sizeof(qint16)), Qt::Uninitialized);
    qint16 *dst = reinterpret_cast<qint16 *>(pcm.data());
    for (size_t i = 0; i < samples.size(); ++i)
        qToLittleEndian<qint16>(qint16(std::clamp(samples[i], -1.0f, 1.0f) * 32767.0f), &dst[i]);
    ok = ok && file.write(pcm) == pcm.size();

    if (!ok || out.status() != QDataStream::Ok) {
        qWarning("AudioClipStore: writing %s failed: %s",
                 qPrintable(file.fileName()), qPrintable(file.errorString()));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        qWarning("AudioClipStore: saving %s failed: %s",
                 qPrintable(file.fileName()), qPrintable(file.errorString()));
        return false;
    }
    return true;
}

std::vector<float> AudioClipStore::load(const QString &id, int *rateOut)
{
    if (!isSafeId(id))
        return {};

    QFile file(path(id));
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 44)
        return {};

    // We only ever read files we wrote, so fixed header offsets are safe
    // here — this is not a general-purpose WAV parser. Sample rate lives at
    // byte 24 of the canonical 44-byte header.
    if (rateOut) {
        file.seek(24);
        QDataStream in(&file);
        in.setByteOrder(QDataStream::LittleEndian);
        quint32 rate = 0;
        in >> rate;
        *rateOut = rate > 0 ? int(rate) : 16000;
    }

    file.seek(44);
    const QByteArray raw = file.readAll();

    const qint16 *pcm = reinterpret_cast<const qint16 *>(raw.constData());
    const size_t count = size_t(raw.size()) / sizeof(qint16);

    std::vector<float> samples(count);
    for (size_t i = 0; i < count; ++i)
        samples[i] = qFromLittleEndian<qint16>(&pcm[i]) / 32768.0f;
    return samples;
}

void AudioClipStore::remove(const QString &id)
{
    if (!isSafeId(id))
        return;
    QFile::remove(path(id));
}
