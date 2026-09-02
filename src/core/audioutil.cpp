#include "audioutil.h"

#include <algorithm>
#include <cmath>

namespace {

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

} // namespace

std::vector<float> AudioUtil::resample(std::vector<float> in, double srcRate, double dstRate)
{
    if (in.empty() || srcRate <= 0.0 || dstRate <= 0.0)
        return {};
    if (std::abs(srcRate - dstRate) < 0.5)
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

// Automatic conditioning so ANY mic at ANY system gain produces a healthy
// signal:
//   1. high-pass at 80Hz (DC / rumble / hum)
//   2. measure loudness of SPEECH frames only — frame-RMS statistics, with
//      the quietest frames treated as the noise floor. Gaining on raw peak
//      would let one click set the level, and gaining on overall RMS would
//      be dragged down by silence.
//   3. gain speech to about -20 dBFS RMS, capped so a hot mic isn't blown up
//   4. hard-limit stray peaks
void AudioUtil::condition(std::vector<float> &samples, int rate)
{
    if (rate <= 0 || samples.size() < size_t(rate / 4))
        return; // too short to measure meaningfully

    highPass(samples, rate, 80.0);

    // 20ms frame RMS values
    const size_t frame = size_t(rate) / 50;
    if (frame == 0)
        return;

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
