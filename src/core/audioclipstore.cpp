#include "audioclipstore.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtEndian>

#include <algorithm>

namespace {

constexpr quint16 kChannels = 1;
constexpr quint16 kBitsPerSample = 16;

QString audioDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString dir = QDir(base).filePath(QStringLiteral("audio"));
    QDir().mkpath(dir);
    return dir;
}

} // namespace

QString AudioClipStore::path(const QString &id)
{
    return QDir(audioDir()).filePath(id + QStringLiteral(".wav"));
}

bool AudioClipStore::exists(const QString &id)
{
    return QFile::exists(path(id));
}

bool AudioClipStore::save(const QString &id, const std::vector<float> &samples, int rate)
{
    QFile file(path(id));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    // Recordings of the user's voice: owner only, same reasoning as the
    // transcript store.
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    const quint32 kSampleRate = quint32(rate > 0 ? rate : 16000);
    const quint32 dataBytes = quint32(samples.size()) * sizeof(qint16);
    const quint32 byteRate = kSampleRate * kChannels * kBitsPerSample / 8;

    // Canonical 44-byte PCM WAV header.
    file.write("RIFF", 4);
    out << quint32(36 + dataBytes);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    out << quint32(16);                                   // fmt chunk size
    out << quint16(1);                                    // PCM
    out << kChannels;
    out << kSampleRate;
    out << byteRate;
    out << quint16(kChannels * kBitsPerSample / 8);       // block align
    out << kBitsPerSample;
    file.write("data", 4);
    out << dataBytes;

    // Convert into one buffer and write once — pushing samples through
    // QDataStream one at a time costs hundreds of ms per minute of audio.
    QByteArray pcm(qsizetype(samples.size()) * qsizetype(sizeof(qint16)), Qt::Uninitialized);
    qint16 *dst = reinterpret_cast<qint16 *>(pcm.data());
    for (size_t i = 0; i < samples.size(); ++i)
        qToLittleEndian<qint16>(qint16(std::clamp(samples[i], -1.0f, 1.0f) * 32767.0f), &dst[i]);
    file.write(pcm);

    return true;
}

std::vector<float> AudioClipStore::load(const QString &id, int *rateOut)
{
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
    QFile::remove(path(id));
}
