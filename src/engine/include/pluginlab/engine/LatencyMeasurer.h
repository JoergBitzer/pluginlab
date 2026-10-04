#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::engine
{
struct LatencyResult
{
    bool found = false;     // false: no signal came out (the plugin is silent for an impulse)
    int measuredSamples = 0; // position of the peak of the impulse response
    int reportedSamples = 0; // what the plugin says (getLatencySamples)
};

// Measures the latency of a plugin with an impulse: the position of the largest sample of the response. (A plugin that reports its
// latency wrongly is a finding, not a mistake of the measurement: LESSONS_LEARNED §2.) The instance is prepared and processed here and
// is left with the response in its state: the caller prepares it again before using it. The plugin runs with the given channel count.
LatencyResult measureLatency(juce::AudioPluginInstance& instance, double sampleRate, int blockSize, int channels);
}
