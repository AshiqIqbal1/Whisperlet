#include "audioutil.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
int failures = 0;

void check(bool condition, const char *what)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// A buffer that starts and ends mid-waveform, the way a raw mic capture
// does: full-amplitude square-ish edges rather than settling near zero.
std::vector<float> stepBuffer(size_t count, float value)
{
    return std::vector<float>(count, value);
}
} // namespace

int main()
{
    constexpr int kRate = 16000;

    // Edges of a buffer that never approaches zero must land exactly on
    // zero after the fade, removing the click a hard onset/offset causes.
    std::vector<float> samples = stepBuffer(kRate / 2, 0.8f);
    AudioUtil::fadeEdges(samples, kRate);
    check(samples.front() == 0.0f, "fadeEdges zeroes the first sample");
    check(samples.back() == 0.0f, "fadeEdges zeroes the last sample");

    // No introduced discontinuity anywhere across the fade ramp: each
    // consecutive sample-to-sample step must stay small relative to the
    // original amplitude.
    const size_t fadeLen = kRate * 5 / 1000;
    float maxStep = 0.0f;
    for (size_t i = 1; i < fadeLen; ++i)
        maxStep = std::max(maxStep, std::fabs(samples[i] - samples[i - 1]));
    check(maxStep < 0.8f / float(fadeLen) + 1e-4f, "fade ramp has no large sample-to-sample jump");

    // The body of the clip, well away from the edges, is untouched so real
    // speech content is not perceptibly clipped.
    check(samples[kRate / 4] == 0.8f, "fadeEdges leaves samples away from the edges unchanged");

    // A buffer shorter than the fade window must not crash or corrupt data,
    // and still fades smoothly to zero at both ends.
    std::vector<float> tiny = stepBuffer(4, 0.5f);
    AudioUtil::fadeEdges(tiny, kRate);
    check(tiny.front() == 0.0f, "fadeEdges handles a buffer shorter than the fade window (start)");
    check(tiny.back() == 0.0f, "fadeEdges handles a buffer shorter than the fade window (end)");

    // Empty input and a non-positive rate must be safe no-ops.
    std::vector<float> empty;
    AudioUtil::fadeEdges(empty, kRate);
    check(empty.empty(), "fadeEdges is a no-op on an empty buffer");

    std::vector<float> untouched = stepBuffer(100, 0.3f);
    AudioUtil::fadeEdges(untouched, 0);
    check(untouched.front() == 0.3f, "fadeEdges is a no-op for a non-positive rate");

    if (failures == 0)
        std::printf("All audio util fade tests passed.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
