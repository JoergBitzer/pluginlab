#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::engine
{
// Lets a plugin run with another channel count than the audio around it: a mono plugin in a stereo engine gets the mean of the channels
// and its output goes to all channels; a stereo plugin in a mono engine gets the signal on both channels and its left output is the
// result. Allocation free after prepare(), so it can be used in an audio callback.
class ChannelAdapter
{
public:
    // The channel count the plugin should run with in an engine with this many channels: the engine's own if the plugin supports it,
    // else stereo, else mono. Sets that layout on the plugin. Returns 0 if the plugin supports none of them.
    static int chooseLayout(juce::AudioPluginInstance& instance, int outerChannels);

    void prepare(int outerChannels, int pluginChannels, int maxBlockSize);

    // true if the plugin has the channels of the engine: enter() and leave() are then not needed
    bool isTransparent() const;

    // The block for the plugin (valid until the next call), made from the audio of the engine.
    juce::AudioBuffer<float>& enter(const juce::AudioBuffer<float>& outer);

    // The plugin's result back into the audio of the engine.
    void leave(juce::AudioBuffer<float>& outer);

private:
    int m_outerChannels = 0;
    int m_pluginChannels = 0;
    juce::AudioBuffer<float> m_storage;
    juce::AudioBuffer<float> m_block;
};
}
