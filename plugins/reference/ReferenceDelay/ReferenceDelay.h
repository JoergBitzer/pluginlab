#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginSettings.h"
#include "ReferenceControls.h"
#include "tools/ParameterSpec.h"
#include "tools/SynchronBlockProcessor.h"

#include "pluginlab/reference/FeedbackDelay.h"

class ReferenceDelayAudioProcessor;

// PluginLab Reference Delay (W11.1, docs/design/W11-delay-plan.md 7): the reference delay of pluginlab_reference (reference::FeedbackDelay, the
// author's jade::BasicDelayEffect ported with the corrections of the plan): time in ms or a note value of the host's tempo, feedback, mix, a
// first-order low and high cut in the loop, the interpolation of the fractional delay and the loop order. Every echo has an exact answer.
// A change of a parameter keeps the echoes in the line (a jump, the author's "digital" switch). The parameters are defined once here.
const struct
{
    const std::string ID = "time";
    const std::string name = "Time";
    const std::string unitName = "ms";
    const float minValue = 1.0f;
    const float maxValue = 2000.0f;
    const float defaultValue = 250.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Delay time in ms (fractional samples use the interpolation); not used when Sync is a note value.";
} g_paramTime;

const struct
{
    const std::string ID = "feedback";
    const std::string name = "Feedback";
    const std::string unitName = "%";
    const float minValue = -95.0f;
    const float maxValue = 95.0f;
    const float defaultValue = 40.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Gain of one trip around the loop; negative inverts every repeat.";
} g_paramFeedback;

const struct
{
    const std::string ID = "mix";
    const std::string name = "Mix";
    const std::string unitName = "%";
    const float minValue = 0.0f;
    const float maxValue = 100.0f;
    const float defaultValue = 50.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "y = (1 - mix) dry + mix wet.";
} g_paramMix;

const struct
{
    const std::string ID = "lowcut";
    const std::string name = "Low cut";
    const std::string unitName = "Hz";
    const float minValue = 20.0f;
    const float maxValue = 2000.0f;
    const float defaultValue = 100.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = true;
    const std::string help = "First-order Butterworth high-pass in the loop (with Loop filter on): every repeat loses more lows.";
} g_paramLowCut;

const struct
{
    const std::string ID = "highcut";
    const std::string name = "High cut";
    const std::string unitName = "Hz";
    const float minValue = 500.0f;
    const float maxValue = 20000.0f;
    const float defaultValue = 5000.0f;
    const int numDecimalPlaces = 0;
    const bool logFrequency = true;
    const std::string help = "First-order Butterworth low-pass in the loop (with Loop filter on): every repeat gets darker.";
} g_paramHighCut;

const pluginlab::refplugins::ChoiceSpec g_paramSync{
    "sync", "Sync", {"Off (ms)", "1/4", "1/8", "1/8 dotted", "1/8 triplet", "1/16"}, 0,
    "The delay as a note value of the host's tempo (120 BPM without a host tempo), or the time in ms."};
const pluginlab::refplugins::ChoiceSpec g_paramLoopFilter{"loopfilter", "Loop filter", {"Off", "On"}, 0,
                                                          "The low and high cut in the loop on or off."};
const pluginlab::refplugins::ChoiceSpec g_paramInterpolation{
    "interpolation", "Interpolation", {"Lagrange (FIR)", "Thiran (all-pass)"}, 0,
    "Fractional delays: Lagrange of order 3 (the author's tape glide; falls towards Nyquist, darkens every repeat a little) or a first-order "
    "Thiran all-pass (|H| = 1)."};
const pluginlab::refplugins::ChoiceSpec g_paramLoopOrder{
    "looporder", "Loop order", {"Exact", "Author's original"}, 0,
    "Exact: one trip takes the delay time. Author's original (jade::BasicDelayEffect): the feedback adds the previous sample's output, one "
    "trip takes one sample more (echoes at D, 2D + 1, 3D + 2, ...)."};

class ReferenceDelayAudio : public SynchronBlockProcessor
{
public:
    explicit ReferenceDelayAudio(ReferenceDelayAudioProcessor* processor);
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

    // until the echoes are 120 dB down (at most the longest render of the measurements)
    double getTailSeconds() const;

private:
    pluginlab::reference::FeedbackDelaySettings readSetting() const;
    double getTempo() const;

    ReferenceDelayAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_sampleRate = 48000.0;
    std::atomic<float>* m_timeParam = nullptr;
    std::atomic<float>* m_syncParam = nullptr;
    std::atomic<float>* m_feedbackParam = nullptr;
    std::atomic<float>* m_mixParam = nullptr;
    std::atomic<float>* m_loopFilterParam = nullptr;
    std::atomic<float>* m_lowCutParam = nullptr;
    std::atomic<float>* m_highCutParam = nullptr;
    std::atomic<float>* m_interpolationParam = nullptr;
    std::atomic<float>* m_loopOrderParam = nullptr;
    std::unique_ptr<pluginlab::reference::FeedbackDelay> m_delay;
    std::atomic<double> m_tailSeconds{0.0};
};

// The controls in the order of the processing (the delay with its time, the loop with feedback and filters, the mix), grouped in boxes and
// joined by arrows; the time is greyed out when synced, the cut frequencies when the loop filter is off
class ReferenceDelayGUI : public juce::Component, private juce::Timer
{
public:
    ReferenceDelayGUI(ReferenceDelayAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateControls();

    ReferenceDelayAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;
    pluginlab::refplugins::ReferenceControls m_controls;
    std::vector<pluginlab::refplugins::FlowGroup> m_flow;
    float m_scale = 1.0f;
};
