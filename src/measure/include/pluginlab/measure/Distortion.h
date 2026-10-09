#pragma once

#include <vector>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/SweptImpulse.h"

namespace pluginlab::measure
{
// THD+N and THD (docs/measurements/thd-and-thdn.md): AES17-2015 6.3.1 (THD+N ratio), 6.3.2 (vs frequency), 6.3.3 (vs input level), annex A.3.6
// (frequency-domain TD+N), annex A.4.7 (THD vs frequency from the sweep); THD and harmonic levels as defined in IEC 60268-3.
struct DistortionSettings
{
    double sampleRate = 48000.0;
    double frequencyHz = kStandardFrequencyHz;   // snapped to a bin of the analysis window (997 Hz -> 996.83 Hz at 48 kHz)
    double levelDbfs = -1.0;                     // AES17 6.3.2: -1 dB (and -20 dB) relative to the maximum input level
    double settleSeconds = 0.5;
    double measureSeconds = 1.0;                 // at least; the window is the next power of two (65536 samples at 48 kHz)
    int maximumHarmonic = 10;                    // harmonics up to this order (and up to the band edge) count in THD
    double notchQ = 2.0;                         // AES17 5.2.8: the standard notch, Q between 1.2 and 3
    int channels = 2;
};

struct ChannelDistortion
{
    double fundamentalDbfs = 0.0;
    double outputDbfs = 0.0;                     // the output through the standard low-pass filter, true rms (AES17's level: everything)
    std::vector<int> harmonicOrders;             // 2, 3, ... (those below the upper band edge)
    std::vector<double> harmonicDb;              // level of each harmonic relative to the fundamental
    double thdDb = 0.0;                          // IEC 60268-3: sqrt(sum H_n^2) / H_1
    double thdPercent = 0.0;
    double thdnDb = 0.0;                         // AES17 annex A.3.6: all bins 20 Hz ... 20 kHz except the fundamental, relative to all of them
    double thdnNotchDb = 0.0;                    // AES17 6.3.1: residual after the standard notch relative to the total (both through the standard low-pass)
};

struct DistortionResult
{
    DistortionSettings settings;
    double frequencyHz = 0.0;                    // the coherent test frequency
    int windowSamples = 0;
    std::vector<ChannelDistortion> channels;
};

DistortionResult measureDistortion(const Device& device, const DistortionSettings& settings);

// AES17 6.3.3: THD+N against the input level (one measurement per level)
std::vector<DistortionResult> measureDistortionVsLevel(const Device& device, const DistortionSettings& settings, const std::vector<double>& levelsDbfs);

// AES17 6.3.2: THD+N against the frequency (one measurement per frequency)
std::vector<DistortionResult> measureDistortionVsFrequency(const Device& device, const DistortionSettings& settings, const std::vector<double>& frequencies);

// AES17 annex A.4.7 with the synchronized sweep (Novak et al. 2015): the harmonic impulse responses at -L ln(n) give the level of the n-th harmonic
// for every input frequency f (their spectrum at n f, relative to the linear response at f)
struct HarmonicResponse
{
    std::vector<double> frequencyHz;             // input frequencies
    int maximumHarmonic = 5;
    double validToHz = kUpperBandEdgeHz;         // the harmonic n f must lie below this
    struct Channel
    {
        std::vector<std::vector<double>> harmonicDb; // [n - 2][frequency]: level of the n-th harmonic relative to the fundamental (NaN if n f is out of band)
        std::vector<double> thdDb;                   // from the harmonics in band
    };
    std::vector<Channel> channels;
};

HarmonicResponse measureSweptHarmonics(const Device& device, const SweepResponseSettings& settings, int maximumHarmonic, const std::vector<double>& frequencies);
}
