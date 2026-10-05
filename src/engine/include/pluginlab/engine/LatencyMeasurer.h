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
};

struct LatencyOptions
{
    int preDelaySamples = 0;          // silence before the impulse (so that output before the peak is inside the observation)
    double observeSeconds = 1.0;
    double minimumPeak = 1.0e-4;      // below this nothing came out
};

// Measures the latency of a plugin with an impulse: the position of the largest sample of the response. (A plugin that reports its
// latency wrongly is a finding, not a mistake of the measurement: LESSONS_LEARNED §2.) The instance is prepared and processed here and
// is left with the response in its state: the caller prepares it again before using it. The plugin runs with the given channel count.
LatencyResult measureLatency(juce::AudioPluginInstance& instance, double sampleRate, int blockSize, int channels,
                             const LatencyOptions& options = LatencyOptions());
}
