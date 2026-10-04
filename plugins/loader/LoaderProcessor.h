#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/engine/ChannelAdapter.h"
#include "pluginlab/hosting/HostedPlugin.h"

// The loader plugin: hosts one other plugin (VST3 or VST2) and passes the audio through it. W3: loading, parameters, editor, state;
// no latency compensation yet and effects only (W10 refines it).
class LoaderProcessor : public juce::AudioProcessor
{
public:
    LoaderProcessor();
    ~LoaderProcessor() override;

    const juce::String getName() const override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Loads a plugin and replaces the one that is loaded. Message thread only. On failure the old plugin stays and errorMessage is set.
    bool loadPlugin(const juce::PluginDescription& description, juce::String& errorMessage);
    void unloadPlugin();

    // The loaded plugin (message thread only), nullptr if there is none.
    pluginlab::hosting::HostedPlugin* getHostedPlugin();

    // The editor registers here: before the loaded plugin is replaced or unloaded it must close the window of that plugin, afterwards it
    // can show the new one's parameters (both on the message thread). A plugin's own editor is never opened by loading: only on request.
    std::function<void()> onBeforeHostedPluginChanged;
    std::function<void()> onHostedPluginChanged;

private:
    void configureHostedPlugin();
    void restoreState(const juce::MemoryBlock& state);

    JUCE_DECLARE_WEAK_REFERENCEABLE(LoaderProcessor)

    std::atomic<bool> m_firstBlockLogged{false};

    juce::AudioPluginFormatManager m_formatManager;
    std::unique_ptr<pluginlab::hosting::HostedPlugin> m_hosted;
    juce::CriticalSection m_hostedLock; // the audio thread only try-locks: while the plugin is replaced the audio passes through

    // the channels the loaded plugin runs with (1 for a mono plugin in a stereo loader) and the buffer to adapt the audio to them
    int m_hostedChannels = 0;
    pluginlab::engine::ChannelAdapter m_adapter;

    double m_sampleRate;
    int m_blockSize;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoaderProcessor)
};
