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
// PLUGINLAB_TEST_PLUGIN_FIR_TAPS=N (odd) filters with a symmetric (linear phase) FIR of N taps: pre-ringing before the peak.
// PLUGINLAB_TEST_PLUGIN_BLOCK_MODE=1 smooths the gain once per block (half the way to the target per block: the transient depends on the
// block size, the steady state does not); =2 a one-pole low-pass whose state is reset at every block (a fault: depends on the block size).
// PLUGINLAB_TEST_PLUGIN_STEREO_MODE=1 cross feed (R += L / 2); =2 the gain is a width control (acts on L - R only).
// PLUGINLAB_TEST_PLUGIN_SIDECHAIN=1 has a stereo side-chain input bus that is on by default.
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
    static BusesProperties makeBuses();

    // the delay line of the audio (only used if the plugin delays the audio)
    std::vector<std::vector<float>> m_delayLines;
    int m_delayPosition = 0;
    float m_smoothedGain = 1.0f;
    std::vector<float> m_firCoefficients;
    std::vector<std::vector<float>> m_firHistory;
    int m_firPosition = 0;

    juce::AudioParameterFloat* m_gain = nullptr;
    juce::AudioParameterFloat* m_frequency = nullptr;
    juce::AudioParameterChoice* m_mode = nullptr;
    juce::AudioParameterBool* m_bypass = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TestPluginProcessor)
};
