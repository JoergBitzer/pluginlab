#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginSettings.h"
#include "ReferenceControls.h"
#include "tools/ParameterSpec.h"
#include "tools/SynchronBlockProcessor.h"

#include "pluginlab/reference/DirectFormFilter.h"
#include "pluginlab/reference/Utility.h"

class ReferenceUtilityAudioProcessor;

// PluginLab Reference Utility: gain/polarity -> delay (integer, Thiran or Lagrange) -> channel matrix (width, crosstalk) -> tremolo -> DC,
// from pluginlab_reference (docs/design/W6-plan.md, W6.4). Every stage is exact.
// The parameters are defined once here (tools/ParameterSpec.h, ReferenceControls.h).
const struct
{
    const std::string ID = "gain";
    const std::string name = "Gain";
    const std::string unitName = "dB";
    const float minValue = -60.0f;
    const float maxValue = 24.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Gain of all channels.";
} g_paramGain;

const struct
{
    const std::string ID = "delay";
    const std::string name = "Delay";
    const std::string unitName = "samples";
    const float minValue = 0.0f;
    const float maxValue = 4800.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Delay in samples; a fraction uses the chosen interpolator. Reported latency stays 0 (it is an effect).";
} g_paramDelay;

const struct
{
    const std::string ID = "width";
    const std::string name = "Width";
    const std::string unitName = "";
    const float minValue = 0.0f;
    const float maxValue = 2.0f;
    const float defaultValue = 1.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Stereo width (M/S): 0 mono, 1 unchanged, 2 double side.";
} g_paramWidth;

const struct
{
    const std::string ID = "crosstalk";
    const std::string name = "Crosstalk";
    const std::string unitName = "dB";
    const float minValue = -120.0f;
    const float maxValue = 0.0f;
    const float defaultValue = -120.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Each channel leaks into the other at this level; -120 dB = off.";
} g_paramCrosstalk;

const struct
{
    const std::string ID = "dc";
    const std::string name = "DC offset";
    const std::string unitName = "";
    const float minValue = -0.5f;
    const float maxValue = 0.5f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 3;
    const bool logFrequency = false;
    const std::string help = "Adds a constant to all channels.";
} g_paramDc;

const struct
{
    const std::string ID = "rate";
    const std::string name = "Tremolo rate";
    const std::string unitName = "Hz";
    const float minValue = 0.1f;
    const float maxValue = 20.0f;
    const float defaultValue = 5.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Tremolo frequency.";
} g_paramRate;

const struct
{
    const std::string ID = "depth";
    const std::string name = "Tremolo depth";
    const std::string unitName = "";
    const float minValue = 0.0f;
    const float maxValue = 1.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Tremolo depth: the gain moves between 1 - depth and 1; 0 = off.";
} g_paramDepth;

const pluginlab::refplugins::ChoiceSpec g_paramPolarity{"polarity", "Polarity", {"Normal", "Inverted"}, 0, "Inverts the polarity of all channels."};
const pluginlab::refplugins::ChoiceSpec g_paramInterpolation{
    "interpolation", "Interpolation", {"Thiran (all-pass)", "Lagrange (FIR)"}, 0,
    "Fractional delays: Thiran all-pass (|H| = 1, flat group delay at DC) or Lagrange FIR (flat at DC, falls towards Nyquist); order 3."};

class ReferenceUtilityAudio : public SynchronBlockProcessor
{
public:
    explicit ReferenceUtilityAudio(ReferenceUtilityAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    int processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock) override;

    // parameter handling
    void addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts);

    // some necessary info for the host
    int getLatency()
    {
        return m_Latency;
    }

private:
    struct Setting
    {
        float gain = 0.0f;
        int polarity = 0;
        float delay = 0.0f;
        int interpolation = 0;
        float width = 1.0f;
        float crosstalk = -120.0f;
        float dc = 0.0f;
        float rate = 5.0f;
        float depth = 0.0f;

        bool operator==(const Setting& other) const;
    };

    Setting readSetting() const;
    void applySetting(const Setting& setting);

    ReferenceUtilityAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_sampleRate = 48000.0;
    int m_channels = 2;
    std::atomic<float>* m_gainParam = nullptr;
    std::atomic<float>* m_polarityParam = nullptr;
    std::atomic<float>* m_delayParam = nullptr;
    std::atomic<float>* m_interpolationParam = nullptr;
    std::atomic<float>* m_widthParam = nullptr;
    std::atomic<float>* m_crosstalkParam = nullptr;
    std::atomic<float>* m_dcParam = nullptr;
    std::atomic<float>* m_rateParam = nullptr;
    std::atomic<float>* m_depthParam = nullptr;
    Setting m_setting;
    bool m_settingValid = false;
    std::unique_ptr<pluginlab::reference::Gain> m_gain;
    std::unique_ptr<pluginlab::reference::DirectFormFilter> m_delay;
    std::unique_ptr<pluginlab::reference::ChannelMatrix> m_matrix;
    std::unique_ptr<pluginlab::reference::Tremolo> m_tremolo;
    std::unique_ptr<pluginlab::reference::DcOffset> m_dc;
};

class ReferenceUtilityGUI : public juce::Component
{
public:
    ReferenceUtilityGUI(ReferenceUtilityAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ReferenceUtilityAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;
    pluginlab::refplugins::ReferenceControls m_controls;
};
