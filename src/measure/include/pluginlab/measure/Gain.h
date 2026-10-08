#pragma once

#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// Level and gain (docs/measurements/level-and-gain.md): AES17-2015 6.2.2 (gain) and 6.2.4 (gain matching between channels).
struct GainSettings
{
    double sampleRate = 48000.0;
    double frequencyHz = kStandardFrequencyHz;
    double levelDbfs = -20.0;        // AES17 6.2.2: -20 dB relative to the maximum input level (0 dBFS for a plugin, see W7.9)
    double settleSeconds = 0.5;      // discarded before the measurement window (transients, latency, smoothing)
    double measureSeconds = 1.0;     // at least; a whole number of periods (AES17 5.2.3)
    int channels = 2;                // the test signal on all channels at once (AES17 6.2.4)
};

struct ChannelGain
{
    double inputLevelDbfs = 0.0;     // the test signal as generated, true rms
    double outputLevelDbfs = 0.0;    // the output through the standard low-pass filter, true rms (everything: tone, harmonics, noise)
    double gainDb = 0.0;             // AES17 6.2.2: output level - input level (broadband; both through the standard low-pass filter)
    double selectiveGainDb = 0.0;    // the tone alone (one-bin frequency-domain band-pass, AES17 5.2.9 / 5.2.10 "if the EUT is very noisy")
    double phaseDegrees = 0.0;       // phase of the output tone relative to the input tone (latency included), -180 ... 180
};

struct GainResult
{
    GainSettings settings;
    int measureStart = 0;            // first sample of the window
    int measureLength = 0;           // samples in the window (whole periods)
    std::vector<ChannelGain> channels;
    double matchingDb = 0.0;         // AES17 6.2.4: the largest difference of the (broadband) gains between the channels
};

GainResult measureGain(const Device& device, const GainSettings& settings);
}
