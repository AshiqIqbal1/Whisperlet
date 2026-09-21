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

    // Everything captured so far, still raw (unprocessed) at the device's
    // native rate. Cheap: no DSP runs here, so this is safe to call on the
    // UI thread.
    struct RawRecording {
        std::vector<float> samples;
        int captureRate = 16000;
    };

    // Result of process()ing a RawRecording: denoised + conditioned audio,
    // both as the model's 16kHz mono float32 and as a full-quality native-rate
    // copy for playback.
    struct ProcessedRecording {
        std::vector<float> transcribeSamples; // 16kHz, ready for WhisperEngine::transcribe
        std::vector<float> nativeAudio;       // native rate, for AudioClipStore/playback
        int nativeRate = 16000;
    };

    // Stops capturing and returns the raw samples. Does no DSP, so this
    // returns immediately and is safe to call from the UI thread.
    RawRecording stop();

    // Runs the (potentially slow) denoise + condition + resample pipeline.
    // Pure function of its arguments, so it's meant to be run off the UI
    // thread, e.g. via QtConcurrent::run.
    static ProcessedRecording process(RawRecording raw, bool suppressNoise);

    // Always 16000 — process() resamples if the device captured at a
    // different native rate, so callers never need to branch on this.
    static int sampleRate() { return 16000; }

    const QString &lastError() const { return m_lastError; }

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
    QString m_lastError;

    // Level smoothing: raw per-chunk RMS strobes badly at chunk rate, so we
    // EMA it and emit at most every ~50ms.
    qreal m_smoothedLevel = 0.0;
    QElapsedTimer m_levelClock;
};

#endif // AUDIORECORDER_H
