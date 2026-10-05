#pragma once

#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "pluginlab/engine/FingerprintSettings.h"

namespace pluginlab::engine
{
// The difference of a signal from a reference: relative to the level of the reference ("null depth", dB re reference) and absolute (dBFS).
// For a silent reference only the absolute value means something; the decisions then use it.
struct Difference
{
    double relativeDb = -200.0;
    double absoluteDbfs = -200.0;
    bool referenceSilent = false;
};

struct ParameterFingerprint
{
    int index = 0;
    juce::String name;
    juce::String textAtMinimum;
    juce::String textAtDefault;
    juce::String textAtMaximum;
    int numSteps = 0;
    bool automatable = false;
    bool changesTheAudio = false; // moving it to the low or the high test position changes the output of noise
    Difference change;            // the larger of the two changes
    juce::String measuredWith;    // "defaults" (first pass) or "the others at <high>" (second pass)
};

// The latency at one sample rate
struct RateFingerprint
{
    double sampleRate = 0.0;
    int reportedAfterPrepare = 0;
    int reportedAfterAudio = 0;
    bool outputFound = false;                // false: nothing came out for the impulse
    int measuredLatency = 0;
    double outputBeforePeakDb = -200.0;      // largest output between the impulse and the peak, relative to the peak
    double outputBeforeImpulseDbfs = -200.0; // largest output before the impulse arrived
};

// The A, A, B, A test (LESSONS_LEARNED §2) for one way of giving the plugin its parameters
struct DeliveryResult
{
    juce::String name;
    bool repeatable = false;  // the same settings give the same output every time (A, A, A)
    bool reacts = false;      // other settings (B) give another output
    bool correct = false;     // the output is that of the same settings reached by a change after prepare (the first delivery was not lost)
    bool passed = false;
    juce::String comment;
    Difference secondA;       // A2 against A1
    Difference thirdA;        // A3 against A1
    Difference reaction;      // B against A1
    Difference toReferenceA;  // A1 against the reference A
    Difference toReferenceB;  // B against the reference B
};

struct BlockSizeResult
{
    int blockSize = 0;
    Difference steadyState;   // the end of the render (after the plugin had time to settle): decides
    Difference whole;         // the whole render, for information (smoothing of parameters per block shows here)
};

struct PluginFingerprint
{
    juce::PluginDescription description;
    FingerprintSettings settings;             // the values the measurement used
    juce::String settingsWarning;
    bool loaded = false;
    juce::String message;
    bool supportsMono = false;
    bool supportsStereo = false;
    std::vector<ParameterFingerprint> parameters;
    int numberOfParameters = 0;               // all of the plugin (only the first settings.maximumParameters are examined)
    std::vector<int> reactingParameters;      // indices of the parameters that change the audio
    std::vector<RateFingerprint> rates;       // 44.1, 48 and 96 kHz
    std::vector<DeliveryResult> delivery;
    juce::String recommendedDelivery;         // the most careful way that works (empty if none does)
    std::vector<BlockSizeResult> blockSizes;
    bool blockSizeIndependent = true;
    bool deterministic = false;
    bool outputStaysFinite = true;
    bool recoversFromJumps = true;
    bool silenceStaysSilent = true;
    double idleLevelDb = -200.0;
    std::vector<juce::String> findings;       // what is noteworthy, in words, with the numbers
};

// Measures the fingerprint of a plugin with the measurements of docs/reference/fingerprint-report-explained.md. Loads the plugin many times.
// A plugin that crashes ends the process: run it in a process of its own (PluginLabHost --fingerprint).
PluginFingerprint measureFingerprint(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                                     const FingerprintSettings& settings = FingerprintSettings());

// The fingerprint as a text report (Markdown).
juce::String createReport(const PluginFingerprint& fingerprint);

// "continuous" for a continuous parameter, "switch" for two steps, else the number of steps
juce::String describeSteps(int numSteps);
}
