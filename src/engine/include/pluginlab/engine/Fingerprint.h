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
    int channel = 0;          // the output channel with the largest difference (all channels are compared)
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
    Difference change;            // the larger of the changes (both test signals)
    Difference changeSame;        // with the same noise on all channels (L = R)
    Difference changeDifferent;   // with different noise on the channels (L != R; only for plugins with more than one channel)
    juce::String measuredWith;    // "defaults" (first pass) or "the others at <high>" (second pass)
};

// A bus of the plugin as it is created (before the host chooses a layout)
struct BusFingerprint
{
    bool isInput = true;
    int index = 0;
    juce::String name;
    juce::String defaultLayout;   // the channel set, "disabled" if the bus is off
};

// Whether a main-bus layout is accepted (in = out, or mono in / stereo out); other buses switched off if the plugin allows it
struct LayoutFingerprint
{
    juce::String name;
    bool accepted = false;
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

// One parameter of the settings A (defaults) and B, in normalised units and in the plugin's own text (only those that differ are listed)
struct SettingEntry
{
    int index = 0;
    juce::String name;
    float valueA = 0.0f;
    juce::String textA;
    float valueB = 0.0f;
    juce::String textB;
};

// Real-time behaviour (docs/design/W5c-real-time-behaviour.md): what a fast render with the message thread blocked can hide
struct RealTimeFingerprint
{
    bool measured = false;
    bool ownSettingB = false;               // the scan found no reacting parameter: B = every continuous parameter at the high position
    Difference baseline;                    // two fast renders of B (fresh instances): the noise floor of the comparisons
    Difference offlineDifference;           // offline flag on against off, setting B
    bool sameOffline = true;
    bool pacedMeasured = false;             // realTimeTests in the settings
    Difference pacedDifference;             // real-time pace with the message loop running against fast, setting B
    bool sameWhenPaced = true;
    bool changeJudged = false;              // false for a plugin whose output differs between two renders
    double changeReachedFastMs = -1.0;      // after A -> B in the middle of a render: until the output equals that of B; -1 = not in the render
    double changeReachedPacedMs = -1.0;
    bool changeTimingAlike = true;          // false: one of the two reaches B, the other not (or much later)
    bool longMeasured = false;              // longRealTimeSeconds > 0
    std::vector<double> longDifferentSeconds; // the starts of the 1 s segments of the long run that differ
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
    int measuredChannels = 0;                 // the main-bus channels the measurement runs with (2 if the plugin takes stereo, else 1)
    std::vector<BusFingerprint> buses;
    std::vector<LayoutFingerprint> layouts;
    bool hasSideChain = false;                // more than one input bus
    bool acceptsMidi = false;
    bool producesMidi = false;
    bool isInstrument = false;
    // Channel coupling (only with two channels): one input channel driven, the other silent: the output of the silent one relative to the
    // output of the driven one, at the setting B
    bool couplingMeasured = false;
    Difference couplingLeftToRight;
    Difference couplingRightToLeft;
    bool channelsIndependent = true;
    std::vector<ParameterFingerprint> parameters;
    int numberOfParameters = 0;               // all of the plugin (only the first settings.maximumParameters are examined)
    std::vector<int> reactingParameters;      // indices of the parameters that change the audio
    juce::String scanBase;                    // what the parameter scan started from (the defaults, or the switches flipped, or a pair)
    std::vector<SettingEntry> settingB;       // the parameters where B differs from A
    std::vector<RateFingerprint> rates;       // 44.1, 48 and 96 kHz
    std::vector<DeliveryResult> delivery;
    juce::String recommendedDelivery;         // the most careful way that works (empty if none does)
    std::vector<BlockSizeResult> blockSizes;
    bool blockSizeIndependent = true;
    bool deterministic = false;
    bool timeInvariant = true;                // the same noise twice through one instance (with silence between) gives the same output
    Difference timeInvarianceDifference;
    bool settlesInTime = true;                // after settleSeconds the output equals the output after longSettleSeconds (no slow smoothing)
    Difference settleDifference;
    bool streamReacts = true;                 // the stream way of the delivery test changes its output when the parameters change
    bool outputStaysFinite = true;
    bool recoversFromJumps = true;
    bool recoversContinuous = true;           // after jumps of the continuous parameters
    bool recoversDiscrete = true;             // after jumps of the switches and choices
    bool discreteJumped = false;              // there were switches or choices to jump
    bool silenceStaysSilent = true;
    double idleLevelDb = -200.0;
    RealTimeFingerprint realTime;
    std::vector<juce::String> findings;       // what is noteworthy, in words, with the numbers
};

// Measures the fingerprint of a plugin with the measurements of docs/reference/fingerprint-report-explained.md. Loads the plugin many times.
// A plugin that crashes ends the process: run it in a process of its own (PluginLabHost --fingerprint).
PluginFingerprint measureFingerprint(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description,
                                     const FingerprintSettings& settings = FingerprintSettings());

// One line of the summary: a single result of the fingerprint.
struct SummaryItem
{
    juce::String key;     // stable name for programs (the JSON file, the columns of the Developer page)
    juce::String test;    // what was tested, for people
    juce::String result;  // "yes", "no", a way, "not measured", ...
    juce::String detail;  // the number behind it
    bool good = true;     // false: worth a look (shown as a finding)
};

// The single results of the fingerprint, in the order of the summary table.
std::vector<SummaryItem> summarize(const PluginFingerprint& fingerprint);

// The summary as JSON (plugin name, identifier, the date, and the items): written next to the report, read by the Developer page.
juce::String createSummaryJson(const PluginFingerprint& fingerprint);

// Reads the items back from such JSON (an empty list if the text is not one).
std::vector<SummaryItem> parseSummaryJson(const juce::String& json);

// The fingerprint as a text report (Markdown): the summary table first, then the findings and the details.
juce::String createReport(const PluginFingerprint& fingerprint);

// "continuous" for a continuous parameter, "switch" for two steps, else the number of steps
juce::String describeSteps(int numSteps);
}
