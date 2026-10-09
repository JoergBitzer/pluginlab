#pragma once

#include <vector>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// Intermodulation distortion (docs/measurements/intermodulation.md): AES17-2015 6.3.5 (difference-frequency distortion, two close tones near the
// upper band edge) and 6.3.6 (modulation distortion, 41 Hz and 7993 Hz, 4:1). Both tones lie on bins of a power-of-two window; the frequency-domain
// band-pass filters of AES17 5.2.10 (fixed width in Hz, annex B.5) are sums of the bins within the band.

struct DifferenceFrequencySettings
{
    double sampleRate = 48000.0;
    double upperToneHz = kUpperBandEdgeHz;       // AES17: 20 kHz (or the upper band edge if lower)
    double spacingHz = 2000.0;                   // the lower tone 2 kHz below
    double levelDbfs = 0.0;                      // peak of the sum relative to full scale (AES17: the peak of a sine at the maximum input level; each tone 0.5)
    double bandwidthHz = 500.0;                  // AES17 6.3.5: 500 Hz
    double settleSeconds = 0.5;
    double measureSeconds = 1.0;                 // at least; the window is the next power of two
    int channels = 2;
};

struct ChannelDifferenceFrequency
{
    double lowerFundamentalDbfs = 0.0;           // rms level in the band around the lower tone, dBFS (AES17 3.12)
    double secondOrderDb = 0.0;                  // f2 - f1, relative to the lower fundamental
    double lowerThirdOrderDb = 0.0;              // 2 f1 - f2
    double upperThirdOrderDb = 0.0;              // 2 f2 - f1 (NaN above Nyquist)
    double ratioDb = 0.0;                        // AES17 6.3.5: rms sum of the three products relative to the lower fundamental
    double ratioPercent = 0.0;
};

struct DifferenceFrequencyResult
{
    DifferenceFrequencySettings settings;
    double lowerHz = 0.0;                        // the coherent tones
    double upperHz = 0.0;
    int windowStart = 0;                         // first sample of the analysis window
    int windowSamples = 0;
    std::vector<ChannelDifferenceFrequency> channels;
};

DifferenceFrequencyResult measureDifferenceFrequency(const Device& device, const DifferenceFrequencySettings& settings);

struct ModulationSettings
{
    double sampleRate = 48000.0;
    double lowToneHz = 41.0;                     // AES17 6.3.6
    double highToneHz = 7993.0;
    double amplitudeRatio = 4.0;                 // low tone / high tone
    double levelDbfs = 0.0;                      // peak of the sum (AES17: 0.8 and 0.2 of the maximum input level)
    double bandwidthHz = 40.0;                   // AES17 6.3.6: no more than 40 Hz
    int sidebandOrders = 3;                      // sidebands f2 +- k f1 reported for k = 1 ... this (AES17 uses k = 1)
    double settleSeconds = 0.5;
    double measureSeconds = 1.0;
    int channels = 2;
};

struct ChannelModulation
{
    double upperFundamentalDbfs = 0.0;
    std::vector<double> lowerSidebandDb;         // [k - 1]: f2 - k f1 relative to the upper fundamental
    std::vector<double> upperSidebandDb;         // [k - 1]: f2 + k f1
    double ratioDb = 0.0;                        // AES17 6.3.6: rms sum of the first two sidebands relative to the upper fundamental
    double ratioPercent = 0.0;
    double allSidebandsDb = 0.0;                 // the same with all reported sidebands (odd orders show up at k = 2)
};

struct ModulationResult
{
    ModulationSettings settings;
    double lowHz = 0.0;
    double highHz = 0.0;
    int windowStart = 0;
    int windowSamples = 0;
    std::vector<ChannelModulation> channels;
};

ModulationResult measureModulation(const Device& device, const ModulationSettings& settings);
}
