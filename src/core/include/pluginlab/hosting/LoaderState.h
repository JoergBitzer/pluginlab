#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::hosting
{
// The saved state of the loader plugin: which plugin is loaded (its description) and the state of that plugin, as one block
// (XML, the plugin state base64 encoded). Used by the loader plugin and by the tests.
juce::MemoryBlock createLoaderState(const juce::PluginDescription& description, const juce::MemoryBlock& hostedPluginState);

// false if the data is not a loader state
bool parseLoaderState(const void* data, int sizeInBytes, juce::PluginDescription& description, juce::MemoryBlock& hostedPluginState);
}
