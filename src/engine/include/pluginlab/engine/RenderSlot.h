#pragma once

#include <memory>
#include <vector>

#include "pluginlab/engine/ChannelAdapter.h"
#include "pluginlab/engine/LatencyMeasurer.h"
#include "pluginlab/hosting/HostedPlugin.h"

namespace pluginlab::engine
{
// One of the things the engine compares: a hosted plugin, or no plugin (the dry reference). It processes the audio of the engine in
// its own copy, adapts the channels, and delays its output so that all slots of the engine are time aligned (latency compensation).
class RenderSlot
{
public:
    // plugin: nullptr makes the dry reference slot
    RenderSlot(std::unique_ptr<hosting::HostedPlugin> plugin, const juce::String& name);

    const juce::String& getName() const;
    hosting::HostedPlugin* getPlugin();

    // Prepares the plugin, measures its latency (the impulse run) and prepares it again, then lets it run silence for a number of blocks
    // (the warm-up of the parameter delivery protocol). Returns false and sets error if the plugin supports no channel layout.
    bool prepare(double sampleRate, int maxBlockSize, int engineChannels, juce::String& error);

    // The same again without measuring: back to the state after prepare (the plugin forgets what it processed).
    void restart();

    LatencyResult getLatency() const;

    // Delay of the output in samples (the engine sets it so that all slots have the latency of the slowest one).
    void setCompensation(int samples);
    int getCompensation() const;

    // Processes the block: output = this slot's version of input (same size). Audio thread safe after prepare().
    void process(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output);

private:
    void warmUp();
    void delayOutput(juce::AudioBuffer<float>& output);

    std::unique_ptr<hosting::HostedPlugin> m_plugin;
    juce::String m_name;
    ChannelAdapter m_adapter;
    LatencyResult m_latency;
    juce::MidiBuffer m_midi;
    double m_sampleRate = 0.0;
    int m_maxBlockSize = 0;
    int m_engineChannels = 0;
    int m_pluginChannels = 0;

    int m_compensation = 0;
    int m_delayPosition = 0;
    std::vector<std::vector<float>> m_delayLines;
};
}
