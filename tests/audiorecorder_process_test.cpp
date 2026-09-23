#include "audiorecorder.h"
#include "audioutil.h"

#include <QCoreApplication>
#include <QtConcurrent>
#include <QFuture>

#include <cmath>
#include <cstdio>
#include <random>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::vector<float> makeTone(int rate, double seconds)
{
    std::vector<float> samples(static_cast<size_t>(rate * seconds));
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> noise(-0.05f, 0.05f);
    for (size_t i = 0; i < samples.size(); ++i) {
        const double t = double(i) / rate;
        samples[i] = 0.4f * float(std::sin(2.0 * 3.14159265358979323846 * 220.0 * t)) + noise(rng);
    }
    return samples;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const int captureRate = 48000;
    const std::vector<float> original = makeTone(captureRate, 1.5);

    // Reference: run the exact same DSP pipeline synchronously, the way
    // AudioRecorder::stop() used to do it inline on the UI thread, plus the
    // edge fade process() now applies after conditioning.
    std::vector<float> reference = original;
    AudioUtil::denoise(reference, captureRate);
    AudioUtil::condition(reference, captureRate);
    AudioUtil::fadeEdges(reference, captureRate);
    std::vector<float> referenceNative = reference;
    std::vector<float> referenceTranscribe =
        AudioUtil::resample(std::move(reference), captureRate, AudioUtil::kWhisperRate);

    // AudioRecorder::process() is what now runs off the UI thread via
    // QtConcurrent::run(); it must produce byte-identical output for the
    // same input, since this is a threading fix, not a DSP rewrite.
    AudioRecorder::RawRecording raw;
    raw.samples = original;
    raw.captureRate = captureRate;

    QFuture<AudioRecorder::ProcessedRecording> future =
        QtConcurrent::run(&AudioRecorder::process, raw, /*suppressNoise=*/true);
    future.waitForFinished();
    const AudioRecorder::ProcessedRecording processed = future.result();

    check(processed.nativeRate == captureRate, "process() keeps the native capture rate");
    check(processed.nativeAudio.size() == referenceNative.size(),
          "process() native audio has the same length as the sync pipeline");
    check(processed.transcribeSamples.size() == referenceTranscribe.size(),
          "process() transcribe audio has the same length as the sync pipeline");

    bool nativeIdentical = processed.nativeAudio.size() == referenceNative.size();
    for (size_t i = 0; nativeIdentical && i < referenceNative.size(); ++i)
        nativeIdentical = processed.nativeAudio[i] == referenceNative[i];
    check(nativeIdentical, "process() run off-thread produces identical native audio to the sync pipeline");

    bool transcribeIdentical = processed.transcribeSamples.size() == referenceTranscribe.size();
    for (size_t i = 0; transcribeIdentical && i < referenceTranscribe.size(); ++i)
        transcribeIdentical = processed.transcribeSamples[i] == referenceTranscribe[i];
    check(transcribeIdentical,
          "process() run off-thread produces identical transcribe audio to the sync pipeline");

    // suppressNoise=false must skip denoise() exactly like the old
    // QSettings-gated branch in stop() did.
    std::vector<float> noDenoiseRef = original;
    AudioUtil::condition(noDenoiseRef, captureRate);
    AudioUtil::fadeEdges(noDenoiseRef, captureRate);

    AudioRecorder::RawRecording raw2;
    raw2.samples = original;
    raw2.captureRate = captureRate;
    const AudioRecorder::ProcessedRecording processedNoDenoise =
        AudioRecorder::process(std::move(raw2), /*suppressNoise=*/false);

    bool noDenoiseIdentical = processedNoDenoise.nativeAudio.size() == noDenoiseRef.size();
    for (size_t i = 0; noDenoiseIdentical && i < noDenoiseRef.size(); ++i)
        noDenoiseIdentical = processedNoDenoise.nativeAudio[i] == noDenoiseRef[i];
    check(noDenoiseIdentical,
          "process() with suppressNoise=false skips denoise() just like stop() used to");

    if (failures > 0) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("All checks passed\n");
    return 0;
}
