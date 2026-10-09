#pragma once

#include <vector>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/Weighting.h"

namespace pluginlab::measure
{
// Noise (docs/measurements/noise.md): AES17-2015 6.4.2 idle channel noise, 6.4.1 dynamic range, 6.5.1 power line (mains) related products; the
// weightings of 5.2.7 (CCIR-RMS) and IEC 61672-1 (A).

// A level with every weighting, in dBFS (AES17 3.12: rms relative to a full-scale sine)
struct WeightedLevels
{
    double unweightedDbfs = 0.0;
    double bandDbfs = 0.0;                       // 20 Hz ... 20 kHz
    double ccirRmsDbfs = 0.0;                    // "dBFS CCIR-RMS"
    double aWeightedDbfs = 0.0;                  // "dBFS(A)"

    double get(Weighting weighting) const;
};

struct NoiseSettings
{
    double sampleRate = 48000.0;
    double settleSeconds = 0.5;
    double measureSeconds = 1.0;                 // at least; the window is the next power of two
    double testFrequencyHz = kStandardFrequencyHz; // dynamic range: 997 Hz (made coherent)
    double testLevelDbfs = -60.0;                // AES17 6.4.1: -60 dB re the maximum input level
    double maximumOutputDbfs = 0.0;              // AES17 6.2.6, the reference of the dynamic range (a plugin: full scale unless W7.9 finds less)
    double notchQ = 2.0;                         // AES17 5.2.8
    int channels = 2;
};

// AES17 6.4.2: the input is digital zero
struct IdleNoiseResult
{
    NoiseSettings settings;
    int windowSamples = 0;
    std::vector<WeightedLevels> channels;
};

IdleNoiseResult measureIdleNoise(const Device& device, const NoiseSettings& settings);

// AES17 6.4.1: a 997 Hz sine at -60 dBFS; the output through the standard low-pass and the standard notch, then weighted; the dynamic range is the
// maximum output level minus that level (reported "dB CCIR-RMS"; here also unweighted, band and A)
struct DynamicRangeResult
{
    NoiseSettings settings;
    double frequencyHz = 0.0;                    // coherent
    int windowSamples = 0;
    struct Channel
    {
        WeightedLevels residual;                 // the notched output
        WeightedLevels dynamicRange;             // maximumOutputDbfs - residual (dB)
    };
    std::vector<Channel> channels;
};

DynamicRangeResult measureDynamicRange(const Device& device, const NoiseSettings& settings);

// AES17 6.5.1: digital zero in; frequency-domain band-pass filters (no wider than half the mains frequency) at M times the mains frequency,
// M = 1 ... 5; the power line level is their rms sum. The window is a whole number of seconds: every multiple of 1 Hz lies on a bin.
struct MainsSettings
{
    double sampleRate = 48000.0;
    double mainsHz = 50.0;
    int harmonics = 5;
    double bandwidthHz = 0.0;                    // 0: half the mains frequency (the widest AES17 allows)
    double settleSeconds = 0.5;
    int measureSeconds = 1;
    int channels = 2;
};

struct MainsResult
{
    MainsSettings settings;
    int windowSamples = 0;
    struct Channel
    {
        std::vector<double> lineDbfs;            // [M - 1]: the band level at M x mains
        double totalDbfs = 0.0;                  // the rms sum
    };
    std::vector<Channel> channels;
};

MainsResult measureMainsProducts(const Device& device, const MainsSettings& settings);

// The level of a spectrum (bins 0 ... N/2 of a window of N samples, as getSpectrum gives them) with a weighting, optionally also through a filter
// response given per frequency (e.g. the notch)
double getWeightedRms(const std::vector<std::complex<double>>& spectrum, int length, double sampleRate, Weighting weighting,
                      const std::vector<double>& extraGain = {});
}
