#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// Null test with alignment (docs/measurements/null-test.md): two devices get the same stimulus; the output of B is aligned to the output of A
// (delay, also fractional, and gain, also inverted) and subtracted. The depth of the null is the residual relative to A in the band
// 20 Hz ... 20 kHz. No standard describes it; the alignment is a time-delay estimate (cross-correlation, Knapp and Carter 1976) refined by the phase
// slope of the cross-spectrum and a least-squares gain.
struct NullTestSettings
{
    double sampleRate = 48000.0;
    double levelDbfs = -20.0;                    // white Gaussian noise, AES17 dBFS (rms re a full-scale sine)
    int seed = 17;
    juce::AudioBuffer<float> stimulus;           // instead of the noise (e.g. music); empty: noise. Must hold settle + window + largest delay samples.
    double settleSeconds = 0.5;
    int windowSamples = 1 << 17;                 // the analysis window (a power of two; 2.7 s at 48 kHz)
    double maximumDelaySeconds = 0.25;           // delays of B against A looked for: -this ... +this
    bool alignDelay = true;
    bool alignGain = true;
    int channels = 2;
};

struct ChannelNull
{
    double delaySamples = 0.0;                   // B is delayed by this to match A (A(n) = g B(n - delay)); fractional
    double gainDb = 0.0;                         // |g|
    bool inverted = false;                       // g < 0
    double nullDepthDb = 0.0;                    // residual / A in the band, dB (the lower, the better the null)
    double unalignedNullDb = 0.0;                // A - B without any alignment, for comparison
    double residualDbfs = 0.0;                   // the residual's level in the band, AES17 dBFS
    std::vector<double> thirdOctaveHz;           // standard third-octave centres in the band
    std::vector<double> thirdOctaveResidualDb;   // residual / A per band, dB
};

struct NullTestResult
{
    NullTestSettings settings;
    std::vector<ChannelNull> channels;
};

NullTestResult measureNull(const Device& a, const Device& b, const NullTestSettings& settings);
}
