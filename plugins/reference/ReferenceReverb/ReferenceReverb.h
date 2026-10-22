#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginSettings.h"
#include "ReferenceControls.h"
#include "tools/ParameterSpec.h"
#include "tools/SynchronBlockProcessor.h"

#include "pluginlab/reference/FdnReverb.h"

class ReferenceReverbAudioProcessor;

// PluginLab Reference Reverb (W11.2e, docs/design/W11-reverb-plan.md 7): the FDN of pluginlab_reference (reference::FdnReverb: 8 coprime lines, a
// Householder matrix, gains for an exact T60), a pre-delay, an LFO on the lines (time-variant on demand) and the mix. A new setting rebuilds the
// FDN (it starts empty: the reference is the exact algorithm, not a smooth one). The parameters are defined once here.
const struct
{
    const std::string ID = "t60";
    const std::string name = "T60";
    const std::string unitName = "s";
    const float minValue = 0.2f;
    const float maxValue = 10.0f;
    const float defaultValue = 1.5f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "The decay time: every circulation through the network loses 60 dB in this time (at every frequency without LFO).";
} g_paramT60;

const struct
{
    const std::string ID = "predelay";
    const std::string name = "Pre-delay";
    const std::string unitName = "ms";
    const float minValue = 0.0f;
    const float maxValue = 200.0f;
    const float defaultValue = 10.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Delay before the network (its first output comes one shortest line, 21.5 ms at 48 kHz, later).";
} g_paramPreDelay;

const struct
{
    const std::string ID = "modrate";
    const std::string name = "Mod rate";
    const std::string unitName = "Hz";
    const float minValue = 0.05f;
    const float maxValue = 5.0f;
    const float defaultValue = 1.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Rate of the LFOs on the lines (spread by +-15 % between the lines).";
} g_paramModRate;

const struct
{
    const std::string ID = "moddepth";
    const std::string name = "Mod depth";
    const std::string unitName = "samples";
    const float minValue = 0.0f;
    const float maxValue = 32.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Depth of the LFOs on the line lengths; 0 = time-invariant.";
} g_paramModDepth;

const struct
{
    const std::string ID = "mix";
    const std::string name = "Mix";
    const std::string unitName = "%";
    const float minValue = 0.0f;
    const float maxValue = 100.0f;
    const float defaultValue = 30.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "y = (1 - mix) dry + mix wet (100 %: the reverb alone, as the measurements want it).";
} g_paramMix;

class ReferenceReverbAudio : public SynchronBlockProcessor
{
public:
    explicit ReferenceReverbAudio(ReferenceReverbAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    int processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock) override;

    void addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts);

    int getLatency()
    {
        return m_Latency;
    }

    double getTailSeconds() const;

private:
    struct Setting
    {
        float t60 = 0.0f;
        float preDelay = 0.0f;
        float modRate = 0.0f;
        float modDepth = 0.0f;

        bool operator==(const Setting& other) const;
    };

    Setting readSetting() const;

    ReferenceReverbAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_sampleRate = 48000.0;
    int m_channels = 2;
    std::atomic<float>* m_t60Param = nullptr;
    std::atomic<float>* m_preDelayParam = nullptr;
    std::atomic<float>* m_modRateParam = nullptr;
    std::atomic<float>* m_modDepthParam = nullptr;
    std::atomic<float>* m_mixParam = nullptr;
    Setting m_setting;
    bool m_settingValid = false;
    std::unique_ptr<pluginlab::reference::FdnReverb> m_fdn;
    juce::AudioBuffer<float> m_wet;
    std::atomic<double> m_tailSeconds{0.0};
};

class ReferenceReverbGUI : public juce::Component, private juce::Timer
{
public:
    ReferenceReverbGUI(ReferenceReverbAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    ReferenceReverbAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;
    pluginlab::refplugins::ReferenceControls m_controls;
    std::vector<pluginlab::refplugins::FlowGroup> m_flow;
    float m_scale = 1.0f;
};
