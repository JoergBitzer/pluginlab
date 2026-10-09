#pragma once

#include <vector>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// Inter-channel crosstalk (docs/measurements/crosstalk.md): AES17-2015 6.5.2 (and A.3.8). One channel is driven with a sine at -20 dBFS, the others
// get digital zero; the level on each undriven channel relative to the level on the driven channel, from 20 Hz to the band edge in steps of at most
// an octave. Gain matching between channels (6.2.4) is part of the gain unit (level-and-gain.md).

// 20, 40, 80, ... 10240 Hz and 20 kHz: octave steps over the AES17 passband
std::vector<double> getCrosstalkFrequencies();

struct CrosstalkSettings
{
    double sampleRate = 48000.0;
    double levelDbfs = -20.0;                    // AES17 6.5.2: -20 dB re the maximum input level
    std::vector<double> frequencies;             // empty: getCrosstalkFrequencies()
    double settleSeconds = 0.2;
    double measureSeconds = 0.5;                 // at least; whole periods of each frequency
    int channels = 2;
};

struct CrosstalkResult
{
    CrosstalkSettings settings;
    std::vector<double> frequencyHz;
    // [driven][receiving][frequency], dB re the driven channel's output; the driven channel itself is 0 dB
    std::vector<std::vector<std::vector<double>>> selectiveDb;   // the test tone alone (one-bin frequency-domain band-pass, as A.3.8)
    std::vector<std::vector<std::vector<double>>> broadbandDb;   // everything on the channel through the standard low-pass (AES17's level)
    double worstSelectiveDb = -1000.0;           // the largest crosstalk over all pairs and frequencies
};

CrosstalkResult measureCrosstalk(const Device& device, const CrosstalkSettings& settings);
}
