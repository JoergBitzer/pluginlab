#pragma once

#include <vector>

#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// What kind of system a device is (W8.1, docs/measurements/system-class.md): LTI, time-variant, memoryless non-linear, Hammerstein (curve, then one
// filter), Wiener (one filter, then a curve), generalized Hammerstein (parallel branches x^n -> G_n), or other (e.g. dynamics). From synchronized
// sweeps at several levels and their harmonic responses H_n(f) (Novak et al. 2015; generalized Hammerstein after Novak et al. 2010, leading order).
enum class SystemClass
{
    Lti,
    TimeVariant,
    Memoryless,
    Hammerstein,
    Wiener,
    GeneralizedHammerstein,
    Other
};

juce::String getSystemClassName(SystemClass systemClass);

// What the class means for the other measurements (the first line of a report)
juce::String getSystemClassMeaning(SystemClass systemClass);

struct SystemClassSettings
{
    double sampleRate = 48000.0;
    std::vector<double> levelsDbfs = {-40.0, -20.0, -6.0}; // peak of the sweeps, rising
    int maximumHarmonic = 5;
    double offsetSeconds = 0.37;                // the second sweep at the middle level starts this much later (time variance)
    double harmonicLimitDb = -80.0;             // harmonics below this (re H_1) do not count
    double linearToleranceDb = 0.05;            // H_1 equal across levels / flat within this
    double flatToleranceDb = 0.2;               // a harmonic response counts as flat / a ratio as constant within this (spread)
    double wienerToleranceDb = 0.3;
    double shapeToleranceDb = 1.0;              // generalized Hammerstein: the shape of each H_n the same at two levels (near the noise floor: 0.8 dB)
    double timeVarianceDb = 0.1;                // H_1 of the offset sweep differs by more: time-variant
    double lowestHz = 50.0;
    double highestHz = 16000.0;
};

// The evidence behind the class (all in dB)
struct SystemClassEvidence
{
    double timeVarianceDb = 0.0;                // largest |H_1| difference between the two sweeps at the middle level
    double largestHarmonicDb = -1000.0;         // largest |H_n / H_1| at any level
    double linearLevelSpreadDb = 0.0;           // largest |H_1| difference between the levels
    double levelChangeBoundDb = 0.0;            // what a polynomial's odd harmonics allow for that difference
    double linearFlatnessDb = 0.0;              // spread of |H_1| over frequency (highest level)
    double harmonicFlatnessDb = 0.0;            // largest spread of a significant |H_n| over frequency
    double hammersteinSpreadDb = 0.0;           // largest spread of |H_n(f)| / |H_1(n f)|
    double wienerSpreadDb = 0.0;                // largest spread of |H_n(f)| / |H_1(f)|^n (lowest level with significant harmonics)
    double shapeSpreadDb = 0.0;                 // largest change of the shape of a significant H_n between the two highest levels
    std::vector<int> significantOrders;         // harmonic orders above the limit at the highest level
};

struct SystemClassResult
{
    SystemClassSettings settings;
    SystemClass systemClass = SystemClass::Lti;
    SystemClassEvidence evidence;
    juce::String reason;                        // one sentence: which test decided
};

// Channel 0 decides
SystemClassResult classifySystem(const Device& device, const SystemClassSettings& settings);
}
