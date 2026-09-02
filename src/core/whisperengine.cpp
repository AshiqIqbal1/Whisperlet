#include "whisperengine.h"

#include <whisper.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

WhisperEngine::~WhisperEngine()
{
    if (m_ctx)
        whisper_free(m_ctx);
}

bool WhisperEngine::loadModel(const QString &path)
{
    QElapsedTimer clock;
    clock.start();

    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
        m_loadedPath.clear();
    }

    whisper_context_params cparams = whisper_context_default_params();
    m_ctx = whisper_init_from_file_with_params(path.toUtf8().constData(), cparams);

    if (!m_ctx) {
        m_lastError = QStringLiteral("whisper_init_from_file_with_params failed for %1").arg(path);
        return false;
    }

    m_loadedPath = path;
    m_lastLoadMs = clock.elapsed();
    qInfo() << "[perf] model load" << path.section('/', -1) << m_lastLoadMs << "ms";
    return true;
}

QString WhisperEngine::transcribe(const std::vector<float> &samples, const QString &language)
{
    if (!m_ctx) {
        m_lastError = QStringLiteral("no model loaded");
        return QString();
    }
    if (samples.empty()) {
        m_lastError = QStringLiteral("no audio samples");
        return QString();
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;
    params.print_special    = false;
    params.translate        = false;
    params.single_segment   = false;
    params.no_context       = true;

    const QByteArray langUtf8 = language.toUtf8();
    params.language = language.isEmpty() ? "auto" : langUtf8.constData();
    params.detect_language = language.isEmpty();

    // Leave one core free for the UI thread; whisper.cpp saturates the rest.
    const int threads = qBound(1, QThread::idealThreadCount() - 1, 8);
    params.n_threads = threads;

    QElapsedTimer clock;
    clock.start();

    if (whisper_full(m_ctx, params, samples.data(), static_cast<int>(samples.size())) != 0) {
        m_lastError = QStringLiteral("whisper_full failed");
        return QString();
    }

    m_lastTranscribeMs = clock.elapsed();
    qInfo() << "[perf] transcribe" << (samples.size() / 16000.0) << "s of audio in"
            << m_lastTranscribeMs << "ms on" << threads << "threads";

    QString text;
    const int segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < segments; ++i) {
        text += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
    }
    return text.trimmed();
}
