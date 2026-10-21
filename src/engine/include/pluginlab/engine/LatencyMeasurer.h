#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::engine
{
struct LatencyResult
{
    bool found = false;               // false: no signal came out (the plugin is silent for an impulse)
    int measuredSamples = 0;          // position of the peak of the response, counted from the impulse
    int reportedSamples = 0;          // what the plugin says (getLatencySamples) right after prepareToPlay
    int reportedAfterAudio = 0;       // the same after the whole observation (a plugin may report it late)
    double outputBeforePeakDb = -200.0;      // largest output between the impulse and the peak, relative to the peak (pre-ringing, look-ahead)
    double outputBeforeImpulseDbfs = -200.0; // largest output before the impulse arrived (the plugin makes signal of its own)
    // A delay (W11.1b): the largest sample is an echo, not the latency; measuredSamples is then the reported latency
    bool peakIsEcho = false;
    int echoSamples = 0;              // the position of that echo, counted from the impulse
    juce::String echoReason;          // why it counts as an echo
};

struct LatencyOptions
{
    int preDelaySamples = 0;          // silence before the impulse (so that output before the peak is inside the observation)
    double observeSeconds = 1.0;
    double minimumPeak = 1.0e-4;      // below this nothing came out
    // The largest sample this far after the reported latency or later may be a delay's echo (W11.1b): it is one if the dry path arrives at the
    // reported latency (at most dryBelowDb below the peak), if a second echo follows at the same distance, or if it is later than any plausible
    // latency (implausibleLatencySeconds)
    double minimumEchoSeconds = 0.005;
    double dryBelowDb = 40.0;
    double secondEchoBelowDb = 60.0;
    double implausibleLatencySeconds = 0.05;
};

// Measures the latency of a plugin with an impulse: the position of the largest sample of the response, unless that is a delay's echo
// (see LatencyOptions; a 100 % wet delay or reverb without feedback whose echo comes earlier than implausibleLatencySeconds stays a latency). (A plugin that reports its
// latency wrongly is a finding, not a mistake of the measurement: LESSONS_LEARNED §2.) The instance is prepared and processed here and
// is left with the response in its state: the caller prepares it again before using it. The plugin runs with the given channel count.
LatencyResult measureLatency(juce::AudioPluginInstance& instance, double sampleRate, int blockSize, int channels,
                             const LatencyOptions& options = LatencyOptions());
}
