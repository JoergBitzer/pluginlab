#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

// A plugin with known behavior for the tests of the host:
//   parameters (in this order): Gain (-24 ... 24 dB, default 0), Frequency (20 ... 20000 Hz, default 1000),
//                               Mode (choice A, B, C), Bypass (switch)
//   processing: multiplies the audio with the gain, unless bypassed
// Built with PLUGINLAB_TEST_PLUGIN_CRASHES=1 it crashes as soon as the plugin object is created (a plugin that cannot be scanned);
// with PLUGINLAB_TEST_PLUGIN_CRASHES_IN_PROCESS=1 it crashes in processBlock (scanning and loading work).
// PLUGINLAB_TEST_PLUGIN_DELAY_SAMPLES=N delays the audio by N samples; PLUGINLAB_TEST_PLUGIN_REPORTED_LATENCY=M is the latency it tells the
// host (a plugin whose reported latency is wrong: the engine must measure it).
class TestPluginProcessor : public juce::AudioProcessor
{
public:
    TestPluginProcessor();

    const juce::String getName() const override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // the plugin's own Bypass is the bypass parameter: without this the VST3 wrapper adds a second, hidden one
    juce::AudioProcessorParameter* getBypassParameter() const override;

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

private:
    // the delay line of the audio (only used if the plugin delays the audio)
    std::vector<std::vector<float>> m_delayLines;
    int m_delayPosition = 0;

    juce::AudioParameterFloat* m_gain = nullptr;
    juce::AudioParameterFloat* m_frequency = nullptr;
    juce::AudioParameterChoice* m_mode = nullptr;
    juce::AudioParameterBool* m_bypass = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestPluginProcessor)
};
