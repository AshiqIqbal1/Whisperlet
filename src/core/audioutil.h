#ifndef AUDIOUTIL_H
#define AUDIOUTIL_H

#include <vector>

// Shared DSP used by the recorder and by re-transcription of stored clips.
namespace AudioUtil {

// Rate whisper.cpp requires.
constexpr int kWhisperRate = 16000;

// Anti-aliased sample-rate conversion (low-passes before decimating).
std::vector<float> resample(std::vector<float> in, double srcRate, double dstRate);

// Mic-independent front end: 80Hz high-pass, speech-level AGC to -20 dBFS,
// limiter. Makes any mic at any system gain sound (and transcribe) the same.
void condition(std::vector<float> &samples, int rate);

} // namespace AudioUtil

#endif // AUDIOUTIL_H
