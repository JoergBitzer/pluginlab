#pragma once

#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::engine
{
struct ParameterFingerprint
{
    int index = 0;
    juce::String name;
    juce::String textAtMinimum;
    juce::String textAtDefault;
    juce::String textAtMaximum;
    int numSteps = 0;
    bool automatable = false;
    bool changesTheAudio = false; // moving it to 0.25 or 0.75 changes the output of noise
    double changeDb = -200.0;     // the size of the largest change relative to the level of the output (dB)
};

struct RateFingerprint
{
    double sampleRate = 0.0;
    int reportedLatency = 0;
    int measuredLatency = 0;
    bool hasFeature = false;          // the response deviates from 0 dB somewhere (a bell, a shelf): where
    double featureFrequencyHz = 0.0;
    double featureGainDb = 0.0;
    std::vector<double> responseDb;   // magnitude on the common axis of the fingerprint (see axisFrequenciesHz)
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
    juce::String numbers;     // the differences that the verdict is made of (dB)
};

struct BlockSizeResult
{
    int blockSize = 0;
    double differenceDb = -200.0; // largest difference to the reference block size relative to the level of the output
};

struct PluginFingerprint
{
    juce::PluginDescription description;
    bool loaded = false;
    juce::String message;
    bool supportsMono = false;
    bool supportsStereo = false;
    std::vector<ParameterFingerprint> parameters;
    std::vector<int> reactingParameters;      // indices of the parameters that change the audio
    std::vector<RateFingerprint> rates;       // 44.1, 48 and 96 kHz
    std::vector<double> axisFrequenciesHz;    // the frequencies of RateFingerprint::responseDb
    double largestRateDifferenceDb = 0.0;     // between the responses at the three rates, on the common axis
    bool responseFollowsSampleRate = false;   // the feature (a bell) moves with the sample rate: designed for another rate
    double featureRatio = 0.0;                // feature frequency at the highest rate over that at the lowest
    double rateRatio = 0.0;
    std::vector<DeliveryResult> delivery;
    juce::String recommendedDelivery;          // the most conservative way that works (empty if none does)
    std::vector<BlockSizeResult> blockSizes;
    bool blockSizeIndependent = true;
    bool deterministic = false;
    bool outputStaysFinite = true;
    bool recoversFromJumps = true;
    bool silenceStaysSilent = true;
    double idleLevelDb = -200.0;
    std::vector<juce::String> findings;       // what is noteworthy, in words
};

// Measures the fingerprint of a plugin with the measurements of docs/design/W5-fingerprint.md. Loads the plugin many times. A plugin that
// crashes ends the process: run it in a process of its own (PluginLabHost --fingerprint).
PluginFingerprint measureFingerprint(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description);

// The fingerprint as a text report (Markdown).
juce::String createReport(const PluginFingerprint& fingerprint);
}
