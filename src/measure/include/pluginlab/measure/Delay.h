#pragma once

#include <vector>

#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/SweptImpulse.h"

namespace pluginlab::measure
{
// Delay (latency) and polarity (docs/measurements/delay-and-polarity.md): AES17-2015 6.8.2 (delay through the EUT: impulse response method and
// cross-correlation method) and 6.2.8 (polarity: impulse response method and asymmetric continuous signal).
struct DelaySettings
{
    double sampleRate = 48000.0;
    double levelDbfs = -20.0;              // AES17 6.8.2 / 6.2.8: -20 dB relative to the maximum input level
    double noiseSeconds = 1.0;             // the noise of the cross-correlation method
    double maximumDelaySeconds = 0.5;      // the cross-correlation looks for delays from 0 to this
    double phaseDelayHz = 100.0;           // the frequency of the phase delay (low: the phase delay there equals the delay of a pure delay)
    double twoToneSeconds = 0.5;           // the asymmetric signal of the polarity test (the last half is analysed)
    int channels = 2;
};

struct ChannelDelay
{
    // AES17 6.8.2 a: the impulse response (synchronized sweep), the time of its absolute peak
    int impulsePeakSamples = 0;
    double impulsePeakInterpolated = 0.0;  // parabolic interpolation through the peak and its neighbours
    double phaseDelaySamples = 0.0;        // -arg H(f) / (2 pi f / fs) at phaseDelayHz, unwrapped around the peak
    // AES17 6.8.2 b: the cross-correlation of input and output (white noise), the time of its absolute peak
    int correlationPeakSamples = 0;
    double correlationPeakInterpolated = 0.0;
    // AES17 6.2.8
    bool invertingByImpulse = false;       // c: the peak of the impulse response is negative
    bool invertingByTwoTone = false;       // b: the dominant peak of the response to 997 Hz + 1994 Hz (-90 degrees) is negative
    double twoToneRatio = 0.0;             // |largest positive| / |largest negative| of that response (> 1: non-inverting; about 1.8 for a pure delay)
};

struct DelayResult
{
    DelaySettings settings;
    std::vector<ChannelDelay> channels;
};

DelayResult measureDelay(const Device& device, const DelaySettings& settings);
}
