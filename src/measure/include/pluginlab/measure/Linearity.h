#pragma once

#include <vector>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// Maximum input and output level, overload behaviour and gain non-linearity (docs/measurements/maximum-level-and-linearity.md): AES17-2015 6.2.1,
// 6.2.6, 6.6.8, 6.3.7.

enum class MaximumLevelMethod
{
    ThdN,          // AES17 6.2.1 a: the input level at which THD+N reaches -40 dB (recommended for hard clipping)
    Compression    // AES17 6.2.1 b: the input level at which the output has risen 0.3 dB less than the input (recommended for soft clipping)
};

struct MaximumLevelSettings
{
    double sampleRate = 48000.0;
    double frequencyHz = kStandardFrequencyHz;   // made coherent with the analysis window
    MaximumLevelMethod method = MaximumLevelMethod::ThdN;
    double thdnLimitDb = -40.0;
    double compressionLimitDb = 0.3;
    double referenceLevelDbfs = -40.0;           // compression: the gain here is the linear gain
    double startDbfs = -20.0;                    // the search starts here (the usual operating level) and goes up; down only if this already exceeds
    double lowestDbfs = -60.0;                   // the search range; a float plugin can be linear above 0 dBFS
    double highestDbfs = 24.0;
    double resolutionDb = 0.01;
    double settleSeconds = 0.2;
    double measureSeconds = 0.25;                // at least; the window is the next power of two
    int channels = 1;                            // channel 0 decides
};

struct MaximumLevelResult
{
    MaximumLevelSettings settings;
    double frequencyHz = 0.0;                    // the coherent test frequency
    int windowSamples = 0;
    bool found = false;                          // false: the criterion is not reached up to highestDbfs, or it is exceeded at every level down to lowestDbfs
    bool exceededEverywhere = false;             // the second case (e.g. THD+N above -40 dB from noise alone)
    double maximumInputDbfs = 0.0;               // 6.2.1
    double maximumOutputDbfs = 0.0;              // 6.2.6: the output level (broadband, standard low-pass) at that input level
    double thdnAtMaximumDb = 0.0;
    double gainAtMaximumDb = 0.0;                // output - input level there
    double overloadThdnDb = 0.0;                 // 6.6.8: THD+N at +3 dB above the maximum input level
    bool rollover = false;                       // 6.6.8: overload THD+N above -14 dB (20 %)
    int evaluations = 0;
};

MaximumLevelResult measureMaximumLevel(const Device& device, const MaximumLevelSettings& settings);

struct LinearitySettings
{
    double sampleRate = 48000.0;
    double frequencyHz = kStandardFrequencyHz;
    double maximumInputDbfs = 0.0;               // 6.2.1 (from measureMaximumLevel, or full scale)
    double stepDb = 5.0;                         // AES17: steps of at most 5 dB
    double lowestDbfs = -140.0;                  // stop here at the latest (an undithered device has no idle noise to stop at)
    double bandwidthHz = 500.0;                  // AES17: frequency-domain band-pass no wider than 500 Hz
    double settleSeconds = 0.2;
    double measureSeconds = 1.0;
    int channels = 1;
};

// AES17 6.3.7: the idle channel noise level (6.4.2, dBFS CCIR-RMS) first; the reference at -5 dB re the maximum input level; the level falls in steps
// until the band-passed output is within 5 dB of the idle channel noise level; the deviation of the output from the ideal (linear) output at each step
struct LinearityResult
{
    LinearitySettings settings;
    double frequencyHz = 0.0;
    int windowSamples = 0;
    double idleNoiseDbfs = 0.0;                  // 6.4.2, dBFS CCIR-RMS (the stop criterion)
    std::vector<double> inputDbfs;
    std::vector<double> outputDbfs;              // band-passed output
    std::vector<double> deviationDb;             // output - (reference output + input - reference input)
};

LinearityResult measureGainLinearity(const Device& device, const LinearitySettings& settings);
}
