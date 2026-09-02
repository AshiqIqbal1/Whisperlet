#include "audiorecorder.h"

#include <QAudioSource>
#include <QMediaDevices>
#include <QSettings>
#include <QtEndian>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kTargetRate = 16000;

// 4th-order Butterworth low-pass (two cascaded biquads), applied in place.
// Downsampling without this aliases everything above the new Nyquist back
// into the speech band — the classic "underwater/garbled" recording.
void lowPass(std::vector<float> &samples, double srcRate, double cutoffHz)
{
    const double w0 = 2.0 * M_PI * cutoffHz / srcRate;
    const double cosw = std::cos(w0), sinw = std::sin(w0);

    // Q values for a 4th-order Butterworth split into two biquads.
    for (const double q : {0.54119610, 1.30656296}) {
        const double alpha = sinw / (2.0 * q);
        const double b0 = (1.0 - cosw) / 2.0, b1 = 1.0 - cosw, b2 = b0;
        const double a0 = 1.0 + alpha, a1 = -2.0 * cosw, a2 = 1.0 - alpha;

        double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        for (float &s : samples) {
            const double x = s;
            const double y = (b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            s = float(y);
        }
    }
}

// Second-order Butterworth HIGH-pass — strips DC offset, rumble, desk thumps
// and mains hum below the speech band.
void highPass(std::vector<float> &samples, double rate, double cutoffHz)
{
    const double w0 = 2.0 * M_PI * cutoffHz / rate;
    const double cosw = std::cos(w0), sinw = std::sin(w0);
    const double alpha = sinw / (2.0 * 0.70710678); // Q = 1/sqrt(2)
    const double b0 = (1.0 + cosw) / 2.0, b1 = -(1.0 + cosw), b2 = b0;
    const double a0 = 1.0 + alpha, a1 = -2.0 * cosw, a2 = 1.0 - alpha;

    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (float &s : samples) {
        const double x = s;
        const double y = (b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        s = float(y);
    }
}

// Automatic conditioning so ANY mic at ANY system gain feeds the model (and
// the saved clip) a healthy signal:
//   1. high-pass at 80Hz (DC / rumble / hum)
//   2. measure loudness of SPEECH frames only — frame-RMS statistics, with
//      the quietest frames treated as the noise floor. Gaining on raw peak
//      would let one click set the level, and gaining on overall RMS would
//      be dragged down by silence.
//   3. gain speech to about -20 dBFS RMS, capped so a hot mic isn't blown up
//   4. hard-limit stray peaks
void conditionAudio(std::vector<float> &samples, int rate)
{
    if (samples.size() < size_t(rate / 4))
        return; // too short to measure meaningfully

    highPass(samples, rate, 80.0);

    // 20ms frame RMS values
    const size_t frame = size_t(rate) / 50;
    std::vector<double> rmsValues;
    rmsValues.reserve(samples.size() / frame + 1);
    for (size_t start = 0; start + frame <= samples.size(); start += frame) {
        double sum = 0.0;
        for (size_t i = start; i < start + frame; ++i)
            sum += double(samples[i]) * double(samples[i]);
        rmsValues.push_back(std::sqrt(sum / frame));
    }
    if (rmsValues.size() < 4)
        return;

    std::vector<double> sorted = rmsValues;
    std::sort(sorted.begin(), sorted.end());
    const double noiseFloor = sorted[sorted.size() / 10]; // 10th percentile

    // Speech frames: meaningfully above the noise floor.
    double speechSum = 0.0;
    int speechCount = 0;
    for (double r : rmsValues) {
        if (r > std::max(noiseFloor * 3.0, 1e-5)) {
            speechSum += r;
            ++speechCount;
        }
    }
    // Nothing rose above the floor — probably silence; leave it alone.
    if (speechCount == 0)
        return;

    const double speechRms = speechSum / speechCount;
    constexpr double kTargetRms = 0.1; // ~ -20 dBFS
    const double gain = std::clamp(kTargetRms / speechRms, 0.25, 40.0);

    for (float &s : samples)
        s = std::clamp(float(s * gain), -0.98f, 0.98f);
}

// One-shot resampler — used only when the input device won't give us 16kHz
// natively. Low-pass first (anti-aliasing), then linear interpolation. Runs
// once over the whole buffer at stop(), not per-chunk.
std::vector<float> resampleLinear(std::vector<float> in, double srcRate, double dstRate)
{
    if (in.empty() || srcRate <= 0.0)
        return {};
    if (qFuzzyCompare(srcRate, dstRate))
        return in;

    if (srcRate > dstRate)
        lowPass(in, srcRate, 0.45 * dstRate); // keep speech, kill aliases

    const double ratio = srcRate / dstRate;
    const size_t outCount = static_cast<size_t>(in.size() / ratio);
    std::vector<float> out(outCount);
    for (size_t i = 0; i < outCount; ++i) {
        const double srcPos = i * ratio;
        const size_t i0 = static_cast<size_t>(srcPos);
        const size_t i1 = std::min(i0 + 1, in.size() - 1);
        const double frac = srcPos - static_cast<double>(i0);
        out[i] = static_cast<float>(in[i0] * (1.0 - frac) + in[i1] * frac);
    }
    return out;
}

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

    QAudioFormat desired;
    desired.setSampleRate(kTargetRate);
    desired.setChannelCount(1);
    desired.setSampleFormat(QAudioFormat::Int16);

    m_format = device.isFormatSupported(desired) ? desired : device.preferredFormat();

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

    std::vector<float> result = m_format.sampleRate() == kTargetRate
        ? std::move(m_samples)
        : resampleLinear(std::move(m_samples), m_format.sampleRate(), kTargetRate);

    // Adapt to whatever mic/system gain we were given: high-pass + speech-
    // level AGC + limiter, so the saved clip and the model both get a
    // consistent, healthy signal with no manual OS settings.
    conditionAudio(result, kTargetRate);

    m_samples.clear();
    m_pending.clear();
    return result;
}
