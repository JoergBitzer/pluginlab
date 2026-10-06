#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginSettings.h"
#include "ReferenceControls.h"
#include "tools/ParameterSpec.h"
#include "tools/SynchronBlockProcessor.h"

#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

class ReferenceNonlinearAudioProcessor;

// PluginLab Reference Nonlinear: drive -> curve (polynomial, hard clip, tanh) -> output -> hum -> noise -> quantizer, from pluginlab_reference
// (docs/design/W6-plan.md, W6.4). Every stage has its known answer (harmonics, SNR, levels).
// The parameters are defined once here (tools/ParameterSpec.h, ReferenceControls.h).
const struct
{
    const std::string ID = "drive";
    const std::string name = "Drive";
    const std::string unitName = "dB";
    const float minValue = -24.0f;
    const float maxValue = 24.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Gain before the curve (for tanh the drive of the curve).";
} g_paramDrive;

const struct
{
    const std::string ID = "a2";
    const std::string name = "a2";
    const std::string unitName = "";
    const float minValue = -1.0f;
    const float maxValue = 1.0f;
    const float defaultValue = 0.1f;
    const int numDecimalPlaces = 3;
    const bool logFrequency = false;
    const std::string help = "Polynomial: coefficient of x^2 (y = x + a2 x^2 + a3 x^3).";
} g_paramA2;

const struct
{
    const std::string ID = "a3";
    const std::string name = "a3";
    const std::string unitName = "";
    const float minValue = -1.0f;
    const float maxValue = 1.0f;
    const float defaultValue = 0.05f;
    const int numDecimalPlaces = 3;
    const bool logFrequency = false;
    const std::string help = "Polynomial: coefficient of x^3 (y = x + a2 x^2 + a3 x^3).";
} g_paramA3;

const struct
{
    const std::string ID = "threshold";
    const std::string name = "Threshold";
    const std::string unitName = "dBFS";
    const float minValue = -40.0f;
    const float maxValue = 0.0f;
    const float defaultValue = -6.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Hard clipper: the clipping level.";
} g_paramThreshold;

const struct
{
    const std::string ID = "output";
    const std::string name = "Output";
    const std::string unitName = "dB";
    const float minValue = -24.0f;
    const float maxValue = 24.0f;
    const float defaultValue = 0.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Gain after the curve.";
} g_paramOutput;

const struct
{
    const std::string ID = "bits";
    const std::string name = "Bits";
    const std::string unitName = "";
    const float minValue = 2.0f;
    const float maxValue = 24.0f;
    const float defaultValue = 16.0f;
    const int numDecimalPlaces = 0;
    const bool logFrequency = false;
    const std::string help = "Quantizer: word length (full scale +-1, mid-tread).";
} g_paramBits;

const struct
{
    const std::string ID = "noiselevel";
    const std::string name = "Noise level";
    const std::string unitName = "dBFS";
    const float minValue = -140.0f;
    const float maxValue = 0.0f;
    const float defaultValue = -60.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Added noise, RMS per channel (channels uncorrelated).";
} g_paramNoiseLevel;

const struct
{
    const std::string ID = "humlevel";
    const std::string name = "Hum level";
    const std::string unitName = "dBFS";
    const float minValue = -140.0f;
    const float maxValue = 0.0f;
    const float defaultValue = -60.0f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Added hum, peak level of the sine.";
} g_paramHumLevel;

const pluginlab::refplugins::ChoiceSpec g_paramShaper{
    "shaper", "Curve", {"Off", "Polynomial", "Hard clip", "Soft clip (tanh)"}, 0,
    "The memoryless curve between drive and output. Off: drive and output only."};
const pluginlab::refplugins::ChoiceSpec g_paramQuantizer{
    "quantizer", "Quantizer", {"Off", "On", "On, TPDF dither"}, 0, "Rounds to the word length at the end of the chain, optionally with TPDF dither (+-1 step)."};
const pluginlab::refplugins::ChoiceSpec g_paramNoise{"noise", "Noise", {"Off", "White", "Pink"}, 0, "Adds seeded noise (Gaussian white or Voss-McCartney pink)."};
const pluginlab::refplugins::ChoiceSpec g_paramHum{"hum", "Hum", {"Off", "50 Hz", "60 Hz"}, 0, "Adds a mains hum sine."};

class ReferenceNonlinearAudio : public SynchronBlockProcessor
{
public:
    explicit ReferenceNonlinearAudio(ReferenceNonlinearAudioProcessor* processor);
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
        int shaper = 0;
        float drive = 0.0f;
        float a2 = 0.0f;
        float a3 = 0.0f;
        float threshold = 0.0f;
        float output = 0.0f;
        int quantizer = 0;
        int bits = 16;
        int noise = 0;
        float noiseLevel = 0.0f;
        int hum = 0;
        float humLevel = 0.0f;

        bool operator==(const Setting& other) const;
    };

    Setting readSetting() const;
    void applySetting(const Setting& setting);

    ReferenceNonlinearAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_sampleRate = 48000.0;
    std::atomic<float>* m_shaperParam = nullptr;
    std::atomic<float>* m_driveParam = nullptr;
    std::atomic<float>* m_a2Param = nullptr;
    std::atomic<float>* m_a3Param = nullptr;
    std::atomic<float>* m_thresholdParam = nullptr;
    std::atomic<float>* m_outputParam = nullptr;
    std::atomic<float>* m_quantizerParam = nullptr;
    std::atomic<float>* m_bitsParam = nullptr;
    std::atomic<float>* m_noiseParam = nullptr;
    std::atomic<float>* m_noiseLevelParam = nullptr;
    std::atomic<float>* m_humParam = nullptr;
    std::atomic<float>* m_humLevelParam = nullptr;
    Setting m_setting;
    bool m_settingValid = false;
    double m_driveGain = 1.0;
    double m_outputGain = 1.0;
    std::unique_ptr<pluginlab::reference::Waveshaper> m_shaper;
    std::unique_ptr<pluginlab::reference::Quantizer> m_quantizer;
    std::unique_ptr<pluginlab::reference::NoiseAdder> m_noise;
    std::unique_ptr<pluginlab::reference::HumAdder> m_hum;
};

class ReferenceNonlinearGUI : public juce::Component
{
public:
    ReferenceNonlinearGUI(ReferenceNonlinearAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ReferenceNonlinearAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;
    pluginlab::refplugins::ReferenceControls m_controls;
};
