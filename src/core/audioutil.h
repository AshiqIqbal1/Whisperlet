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

// Spectral subtraction: estimates the steady background (fans, air
// conditioning, room hum, street noise) from the quietest frames and
// subtracts it from every frame. Run BEFORE condition(), so the AGC
// measures speech against a cleaned floor rather than the noise.
void denoise(std::vector<float> &samples, int rate);

// Drops leading and trailing silence and shortens long internal pauses.
// Whisper is charged per second of audio, so this is the cheapest real
// speedup available: a recording that is half pauses transcribes in half
// the time, with no effect on the words.
std::vector<float> trimSilence(const std::vector<float> &samples, int rate);

// Ramps the first and last few milliseconds to zero. The mic stream starts
// and stops mid-waveform, so the raw buffer has a step discontinuity at
// each edge that plays back as a click/pop; a short linear fade removes it
// without being long enough to audibly clip real speech at the boundary.
void fadeEdges(std::vector<float> &samples, int rate);

} // namespace AudioUtil

#endif // AUDIOUTIL_H
