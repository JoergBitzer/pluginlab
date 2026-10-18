#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "HostSettings.h"
#include "pluginlab/engine/MeasurementEngine.h"

// Jade Benchmark, the plugin product (W10.2, docs/design/W10-plan.md): the pages of the stand-alone application (Plugins, Compare, Matcher;
// the Developer page stays in the application) inside a DAW. The DAW's track goes through the engine: every slot (the dry reference and up
// to four plugins) processes it, latency-compensated, and the audible slot comes out; switching is a crossfade. The slots and their states
// are kept in the DAW project. The jobs of the pages (a match) run in the PluginLabHost that the plugin carries in its bundle.
class BenchmarkProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaximumPluginSlots = 4;

    BenchmarkProcessor();
    ~BenchmarkProcessor() override;

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

    // The slots (each with its plugin's description and state) and the audible slot
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    pluginlab::engine::MeasurementEngine& getEngine() { return m_engine; }
    pluginlab::host::HostSettings& getSettings() { return m_settings; }
    juce::AudioPluginFormatManager& getFormatManager() { return m_formatManager; }

    // The slots changed (loaded, unloaded, restored): the latency the DAW is told follows; the editor's pages are told
    void slotsChanged();
    std::function<void()> onSlotsRestored;

    // The PluginLabHost next to the plugin binary (in the bundle), for the jobs of the pages
    static juce::File getBundledHelper();

private:
    juce::MemoryBlock createState();
    void restoreState(const juce::MemoryBlock& state);

    pluginlab::host::HostSettings m_settings;
    juce::AudioPluginFormatManager m_formatManager;
    pluginlab::engine::MeasurementEngine m_engine;
    double m_sampleRate = 0.0;
    int m_blockSize = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE(BenchmarkProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BenchmarkProcessor)
};
