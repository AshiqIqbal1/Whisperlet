#ifndef WHISPERENGINE_H
#define WHISPERENGINE_H

#include <QString>
#include <vector>

struct whisper_context;

// Thin RAII wrapper around the whisper.cpp C API. Deliberately synchronous
// and dumb — loadModel() and transcribe() both block the calling thread, so
// the caller (MainWindow) is responsible for running this off the UI thread
// (see QtConcurrent::run in mainwindow.cpp). Not copyable: it owns exactly
// one whisper_context.
class WhisperEngine
{
public:
    WhisperEngine() = default;
    ~WhisperEngine();

    WhisperEngine(const WhisperEngine &) = delete;
    WhisperEngine &operator=(const WhisperEngine &) = delete;

    // Loads (or reloads) a ggml model file. Blocking — can take a few
    // seconds for the larger models. Returns false and sets lastError() on
    // failure.
    bool loadModel(const QString &path);
    bool isLoaded() const { return m_ctx != nullptr; }
    QString loadedPath() const { return m_loadedPath; }

    // samples: mono, 16kHz, float32 in [-1, 1] — see AudioRecorder.
    // Blocking. Returns the concatenated text of every segment whisper.cpp
    // produced, trimmed. Empty string on failure (check lastError()).
    QString transcribe(const std::vector<float> &samples, const QString &language = QStringLiteral("en"));

    QString lastError() const { return m_lastError; }

    // Perf instrumentation: how long the most recent operations took.
    qint64 lastLoadMs() const { return m_lastLoadMs; }
    qint64 lastTranscribeMs() const { return m_lastTranscribeMs; }

private:
    whisper_context *m_ctx = nullptr;
    QString m_loadedPath;
    QString m_lastError;
    qint64 m_lastLoadMs = 0;
    qint64 m_lastTranscribeMs = 0;
};

#endif // WHISPERENGINE_H
