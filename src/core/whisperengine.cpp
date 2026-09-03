#include "whisperengine.h"

#include <whisper.h>

#include <QDebug>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QThread>

#include <algorithm>
#include <cmath>

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
    // Flash attention is a large speedup on the bigger models for no
    // meaningful quality cost, and it lowers memory traffic on CPU too.
    cparams.flash_attn = true;
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

    // Peak-normalize quiet input: laptop mics often record well below full
    // scale and whisper's accuracy drops with low-level audio. Leave
    // already-loud audio untouched.
    std::vector<float> audio = samples;
    float peak = 0.0f;
    for (float s : audio)
        peak = std::max(peak, std::abs(s));
    if (peak > 0.001f && peak < 0.5f) {
        const float gain = 0.95f / peak;
        for (float &s : audio)
            s *= gain;
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
    // Leave one core for the UI, but do not cap so low that a many-core
    // laptop transcribes at the speed of a four-core one.
    const int threads = qBound(1, QThread::idealThreadCount() - 1, 12);
    params.n_threads = threads;

    QElapsedTimer clock;
    clock.start();

    if (whisper_full(m_ctx, params, audio.data(), static_cast<int>(audio.size())) != 0) {
        m_lastError = QStringLiteral("whisper_full failed");
        return QString();
    }

    m_lastTranscribeMs = clock.elapsed();
    qInfo() << "[perf] transcribe" << (audio.size() / 16000.0) << "s of audio in"
            << m_lastTranscribeMs << "ms on" << threads << "threads";

    QString text;
    const int segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < segments; ++i) {
        text += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
    }
    text = text.trimmed();

    // Whisper narrates non-speech audio with bracketed tags such as
    // [MUSIC PLAYING], (upbeat music) or [BLANK_AUDIO]. They are never
    // something the user said, so strip them; if that is all there was,
    // report nothing rather than typing a stage direction into their work.
    static const QRegularExpression nonSpeech(
        QStringLiteral(R"(\s*[\[(][^\])]*[\])]\s*)"));
    text.remove(nonSpeech);

    return text.trimmed();
}
