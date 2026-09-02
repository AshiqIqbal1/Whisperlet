#ifndef AUDIORECORDER_H
#define AUDIORECORDER_H

#include <QAudioFormat>
#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <vector>

class QAudioSource;
class QIODevice;

// Captures the default microphone straight into the format whisper.cpp
// wants — mono, 16kHz, float32 — so no resampling step is needed later.
// Emits a 0..1 level roughly every ~50ms for the RecordButton's ring.
class AudioRecorder : public QObject
{
    Q_OBJECT

public:
    explicit AudioRecorder(QObject *parent = nullptr);
    ~AudioRecorder() override;

    bool isRecording() const { return m_source != nullptr; }

    // Starts capturing. Returns false (and sets lastError()) if no input
    // device is available or the format isn't supported.
    bool start();

    // Stops capturing and returns everything captured so far as mono
    // float32 samples in [-1, 1], ready to hand to WhisperEngine::transcribe.
    std::vector<float> stop();

    // Always 16000 — stop() resamples if the device captured at a
    // different native rate, so callers never need to branch on this.
    int sampleRate() const { return 16000; }

    // Full-quality (native rate) copy of the last recording, conditioned but
    // not downsampled — this is what gets stored for playback so clips don't
    // sound like a phone call. Valid after stop(); moves out of the recorder.
    std::vector<float> takeNativeAudio();
    int nativeRate() const { return m_nativeRate; }

    QString lastError() const { return m_lastError; }

signals:
    void levelChanged(qreal level); // 0..1, roughly RMS of the last chunk

private slots:
    void onReadyRead();

private:
    QAudioFormat m_format;
    QAudioSource *m_source = nullptr;
    QIODevice *m_device = nullptr;
    QByteArray m_pending;   // leftover partial sample from the last chunk
    std::vector<float> m_samples;
    std::vector<float> m_nativeAudio; // conditioned, still at capture rate
    int m_nativeRate = 16000;
    QString m_lastError;

    // Level smoothing: raw per-chunk RMS strobes badly at chunk rate, so we
    // EMA it and emit at most every ~50ms.
    qreal m_smoothedLevel = 0.0;
    QElapsedTimer m_levelClock;
};

#endif // AUDIORECORDER_H
