#include "audiorecorder.h"

#include "audioutil.h"

#include <QAudioSource>
#include <QMediaDevices>
#include <QSettings>
#include <QtEndian>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kTargetRate = AudioUtil::kWhisperRate;

float sampleToFloat(const char *ptr, QAudioFormat::SampleFormat fmt)
{
    switch (fmt) {
    case QAudioFormat::UInt8:
        return (static_cast<int>(static_cast<quint8>(*ptr)) - 128) / 128.0f;
    case QAudioFormat::Int16:
        return qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(ptr)) / 32768.0f;
    case QAudioFormat::Int32:
        return qFromLittleEndian<qint32>(reinterpret_cast<const uchar *>(ptr)) / 2147483648.0f;
    case QAudioFormat::Float: {
        float v;
        std::memcpy(&v, ptr, sizeof(float));
        return v;
    }
    default:
        return 0.0f;
    }
}

} // namespace

AudioRecorder::AudioRecorder(QObject *parent)
    : QObject(parent)
{
}

AudioRecorder::~AudioRecorder()
{
    if (m_source)
        stop();
}

bool AudioRecorder::start()
{
    if (m_source)
        return true; // already recording

    // Honor the mic picked in the footer menu; fall back to the system
    // default when unset or when that device is gone (unplugged USB mic).
    QAudioDevice device = QMediaDevices::defaultAudioInput();
    const QByteArray wantedId = QSettings().value(QStringLiteral("inputDeviceId")).toByteArray();
    if (!wantedId.isEmpty()) {
        const auto all = QMediaDevices::audioInputs();
        for (const QAudioDevice &d : all) {
            if (d.id() == wantedId) {
                device = d;
                break;
            }
        }
    }
    if (device.isNull()) {
        m_lastError = QStringLiteral("No microphone found");
        return false;
    }

    // Ask for the mic's own preferred format when it's better than 16kHz:
    // playback of the saved clip then sounds like real audio instead of a
    // phone call, and the model still gets a properly resampled 16kHz copy.
    QAudioFormat desired;
    desired.setSampleRate(kTargetRate);
    desired.setChannelCount(1);
    desired.setSampleFormat(QAudioFormat::Int16);

    const QAudioFormat preferred = device.preferredFormat();
    if (preferred.sampleRate() > kTargetRate && device.isFormatSupported(preferred))
        m_format = preferred;
    else if (device.isFormatSupported(desired))
        m_format = desired;
    else
        m_format = preferred;

    m_source = new QAudioSource(device, m_format, this);
    m_device = m_source->start();
    if (!m_device) {
        m_lastError = QStringLiteral("Failed to open microphone stream");
        delete m_source;
        m_source = nullptr;
        return false;
    }

    m_samples.clear();
    m_pending.clear();
    m_smoothedLevel = 0.0;
    m_levelClock.start();
    connect(m_device, &QIODevice::readyRead, this, &AudioRecorder::onReadyRead);
    return true;
}

void AudioRecorder::onReadyRead()
{
    if (!m_device)
        return;

    m_pending += m_device->readAll();

    const int bytesPerSample = m_format.bytesPerSample();
    const int channels = m_format.channelCount();
    const int frameSize = bytesPerSample * channels;
    if (frameSize <= 0)
        return;

    const int frames = m_pending.size() / frameSize;
    if (frames == 0)
        return;

    const size_t before = m_samples.size();
    m_samples.reserve(before + frames);

    const char *data = m_pending.constData();
    for (int f = 0; f < frames; ++f) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += sampleToFloat(data + f * frameSize + c * bytesPerSample, m_format.sampleFormat());
        }
        m_samples.push_back(sum / channels);
    }

    m_pending.remove(0, frames * frameSize);

    // RMS level over just this chunk, scaled up a bit so normal speech
    // visibly moves the ring instead of hugging zero.
    double sumSq = 0.0;
    for (size_t i = before; i < m_samples.size(); ++i)
        sumSq += double(m_samples[i]) * double(m_samples[i]);
    const double rms = std::sqrt(sumSq / double(m_samples.size() - before));

    // EMA so the ring breathes instead of strobing at audio-chunk rate;
    // emit at most every ~50ms to keep repaints off the hot path.
    m_smoothedLevel = 0.75 * m_smoothedLevel + 0.25 * std::clamp(rms * 4.0, 0.0, 1.0);
    if (m_levelClock.elapsed() >= 50) {
        m_levelClock.restart();
        emit levelChanged(m_smoothedLevel);
    }
}

std::vector<float> AudioRecorder::stop()
{
    if (!m_source)
        return {};

    m_source->stop();
    m_source->deleteLater();
    m_source = nullptr;
    m_device = nullptr;

    // Condition ONCE at the native rate — high-pass + speech-level AGC +
    // limiter — so any mic at any system gain gives the same healthy signal
    // with no manual OS settings.
    const int nativeRate = m_format.sampleRate();
    AudioUtil::condition(m_samples, nativeRate);

    // Keep the full-quality version for playback; hand the model its 16kHz.
    m_nativeAudio = m_samples;
    m_nativeRate = nativeRate;

    std::vector<float> result = nativeRate == kTargetRate
        ? std::move(m_samples)
        : AudioUtil::resample(std::move(m_samples), nativeRate, kTargetRate);

    m_samples.clear();
    m_pending.clear();
    return result;
}

std::vector<float> AudioRecorder::takeNativeAudio()
{
    return std::move(m_nativeAudio);
}
