#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/measure/Device.h"

namespace pluginlab::engine
{
// A hosted plugin as a measurement device (docs/measurements/plugins-and-host.md): every render makes a fresh instance and delivers the setting the
// careful way of the fingerprint (prepare; every parameter first to another value and one block of noise; then the setting; settle in silence; then
// the input). The output has the plugin's main-bus channels (getPluginChannels), not necessarily as many as the input.
struct PluginDeviceOptions
{
    int blockSize = 512;
    double settleSeconds = 0.25;                 // silence after the setting, before the input
};

measure::Device makePluginDevice(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, std::vector<float> setting,
                                 PluginDeviceOptions options = {});

// The channels a render gives (the main-bus layout chosen for 2 channels; 0 if the plugin cannot be created)
int getPluginChannels(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate);

// The normalised values of all parameters of an instance (a setting for makePluginDevice); the defaults of a fresh instance if nothing was changed
std::vector<float> getSetting(juce::AudioPluginInstance& instance);
std::vector<float> getDefaultSetting(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate);
}
