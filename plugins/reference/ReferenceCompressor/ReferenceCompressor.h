#pragma once

#include <array>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginSettings.h"
#include "ReferenceControls.h"
#include "tools/ParameterSpec.h"
#include "tools/SynchronBlockProcessor.h"

#include "pluginlab/reference/Compressor.h"

class ReferenceCompressorAudioProcessor;

// PluginLab Reference Compressor (W11.3f, docs/design/W11-dynamics-plan.md 7): the feed-forward compressor of pluginlab_reference
// (reference::Compressor, Giannoulis et al. 2012) with its model variants of W11.3c, the detector, hold, look-ahead, stereo link and side-chain
// high-pass. A new setting rebuilds the compressor (its state starts at no reduction: the reference is the exact algorithm, not a smooth one).
// The latency is the look-ahead. The parameters are defined once here.
const struct
{
    const std::string ID = "threshold";
    const std::string name = "Threshold";
    const std::string unitName = "dBFS";
    const float minValue = -60.0f;
    const float maxValue = 0.0f;
    const float defaultValue = -20.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "The detector level above which the gain is reduced (the middle of the knee).";
} g_paramThreshold;

const struct
{
    const std::string ID = "ratio";
    const std::string name = "Ratio";
    const std::string unitName = ": 1";
    const float minValue = 1.0f;
    const float maxValue = 20.0f;
    const float defaultValue = 4.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Above the threshold the output rises 1 dB for every <ratio> dB of the input.";
} g_paramRatio;

const struct
{
    const std::string ID = "knee";
    const std::string name = "Knee";
    const std::string unitName = "dB";
    const float minValue = 0.0f;
    const float maxValue = 24.0f;
    const float defaultValue = 6.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "The width of the soft knee around the threshold (quadratic, Giannoulis et al.); 0 = hard knee.";
} g_paramKnee;

const struct
{
    const std::string ID = "makeup";
    const std::string name = "Makeup";
    const std::string unitName = "dB";
    const float minValue = -12.0f;
    const float maxValue = 24.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "A fixed gain after the compression.";
} g_paramMakeup;

const struct
{
    const std::string ID = "attack";
    const std::string name = "Attack";
    const std::string unitName = "ms";
    const float minValue = 0.1f;
    const float maxValue = 200.0f;
    const float defaultValue = 5.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "The time constant while the gain reduction grows (63 %: a = exp(-1 / (tau fs))).";
} g_paramAttack;

const struct
{
    const std::string ID = "release";
    const std::string name = "Release";
    const std::string unitName = "ms";
    const float minValue = 5.0f;
    const float maxValue = 2000.0f;
    const float defaultValue = 50.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "The time constant while the gain reduction falls.";
} g_paramRelease;

const struct
{
    const std::string ID = "hold";
    const std::string name = "Hold";
    const std::string unitName = "ms";
    const float minValue = 0.0f;
    const float maxValue = 200.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "After the target reduction falls, the reduction stays this long before the release starts.";
} g_paramHold;

const struct
{
    const std::string ID = "rmswindow";
    const std::string name = "RMS window";
    const std::string unitName = "ms";
    const float minValue = 1.0f;
    const float maxValue = 100.0f;
    const float defaultValue = 10.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "The length of the moving mean square of the RMS detector.";
} g_paramRmsWindow;

const struct
{
    const std::string ID = "lookahead";
    const std::string name = "Look-ahead";
    const std::string unitName = "ms";
    const float minValue = 0.0f;
    const float maxValue = 10.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "The audio is delayed by this time, the detector is not (reported as the plugin's latency).";
} g_paramLookahead;

const struct
{
    const std::string ID = "link";
    const std::string name = "Stereo link";
    const std::string unitName = "%";
    const float minValue = 0.0f;
    const float maxValue = 100.0f;
    const float defaultValue = 100.0f;
    const int numDecimalPlaces = 0;
    const bool logFrequency = false;
    const std::string help = "0 %: each channel its own gain; 100 %: one gain from the louder channel.";
} g_paramLink;

const struct
{
    const std::string ID = "schighpass";
    const std::string name = "SC high-pass";
    const std::string unitName = "Hz";
    const float minValue = 0.0f;
    const float maxValue = 1000.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 0;
    const bool logFrequency = false;
    const std::string help = "A second-order Butterworth high-pass before the detector; 0 = none.";
} g_paramSidechainHighPass;

const pluginlab::refplugins::ChoiceSpec g_paramDetector{
    "detector", "Detector", {"Peak", "RMS"}, 0, "Peak: 20 lg |x| per sample; RMS: the moving mean square over the RMS window."};

const pluginlab::refplugins::ChoiceSpec g_paramSmoothing{
    "smoothing", "Smoothing", {"Gain (dB)", "Level (dB)", "Two stages"}, 0,
    "Where attack and release act: on the gain reduction in dB (Giannoulis' recommendation), on the detector level in dB (the gain computer "
    "after it), or the detector's power first (2 / 20 ms) and then the gain reduction in dB (Simmer, Schmidt, Bitzer 2006)."};

class ReferenceCompressorAudio : public SynchronBlockProcessor
{
public:
    explicit ReferenceCompressorAudio(ReferenceCompressorAudioProcessor* processor);
    void prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels);
    int processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock) override;

    void addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector);
    void prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts);

    // the latency: the look-ahead in samples at the current rate (the processor sets it when the look-ahead or the rate changes)
    int getLatency() const;

    double getTailSeconds() const;

private:
    enum Index
    {
        Threshold,
        Ratio,
        Knee,
        Makeup,
        Attack,
        Release,
        Hold,
        RmsWindow,
        Lookahead,
        Link,
        SidechainHighPass,
        DetectorChoice,
        SmoothingChoice,
        Count
    };

    using Setting = std::array<float, Count>;

    Setting readSetting() const;

    ReferenceCompressorAudioProcessor* m_processor;
    double m_sampleRate = 48000.0;
    std::array<std::atomic<float>*, Count> m_params{};
    Setting m_setting{};
    bool m_settingValid = false;
    std::unique_ptr<pluginlab::reference::Compressor> m_compressor;
};

class ReferenceCompressorGUI : public juce::Component, private juce::Timer
{
public:
    ReferenceCompressorGUI(ReferenceCompressorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    ReferenceCompressorAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;
    pluginlab::refplugins::ReferenceControls m_controls;
    std::vector<pluginlab::refplugins::FlowGroup> m_flow;
    float m_scale = 1.0f;
};
