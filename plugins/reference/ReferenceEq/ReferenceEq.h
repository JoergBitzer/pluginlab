#pragma once

#include <complex>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginSettings.h"
#include "ReferenceControls.h"
#include "tools/ParameterSpec.h"
#include "tools/SynchronBlockProcessor.h"

#include "pluginlab/reference/Analog.h"
#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/StateVariableFilter.h"

class ReferenceEqAudioProcessor;

// PluginLab Reference EQ: one filter band from pluginlab_reference, chosen by algorithm and type (docs/design/W6-plan.md, W6.4).
// The parameters are defined once here (tools/ParameterSpec.h, ReferenceControls.h).
const struct
{
    const std::string ID = "frequency";
    const std::string name = "Frequency";
    const std::string unitName = "Hz";
    const float minValue = 20.f;
    const float maxValue = 20000.f;
    const float defaultValue = 1000.f;
    const int numDecimalPlaces = 0;
    const bool logFrequency = true;
    const std::string help = "Corner or centre frequency (limited to 0.45 of the sample rate).";
} g_paramFrequency;

const struct
{
    const std::string ID = "gain";
    const std::string name = "Gain";
    const std::string unitName = "dB";
    const float minValue = -24.f;
    const float maxValue = 24.f;
    const float defaultValue = 0.f;
    const int numDecimalPlaces = 1;
    const bool logFrequency = false;
    const std::string help = "Gain of peak and shelf filters (the other types ignore it).";
} g_paramGain;

const struct
{
    const std::string ID = "q";
    const std::string name = "Q";
    const std::string unitName = "";
    const float minValue = 0.1f;
    const float maxValue = 20.f;
    const float defaultValue = 0.71f;
    const int numDecimalPlaces = 2;
    const bool logFrequency = false;
    const std::string help = "Quality factor (RBJ definitions; shelves: 0.71 = no overshoot). Butterworth and Linkwitz-Riley ignore it.";
} g_paramQ;

const struct
{
    const std::string ID = "order";
    const std::string name = "Order";
    const std::string unitName = "";
    const float minValue = 1.f;
    const float maxValue = 8.f;
    const float defaultValue = 2.f;
    const int numDecimalPlaces = 0;
    const bool logFrequency = false;
    const std::string help = "Butterworth: order 1 ... 8. Linkwitz-Riley: 2, 4 or 8 (rounded up). Zoelzer shelves: 1st or 2nd order.";
} g_paramOrder;

const pluginlab::refplugins::ChoiceSpec g_paramAlgorithm{
    "algorithm", "Algorithm", {"RBJ cookbook", "Orfanidis", "Zoelzer (DAFX)", "State variable (TPT)", "Butterworth", "Linkwitz-Riley"}, 0,
    "The design. Orfanidis: peak only; Zoelzer: shelves and peak; Butterworth and Linkwitz-Riley: low-pass and high-pass; RBJ and state variable: all types."};

const pluginlab::refplugins::ChoiceSpec g_paramType{
    "type", "Type", {"Low-pass", "High-pass", "Band-pass", "Band-pass 0 dB", "Notch", "All-pass", "Peak", "Low shelf", "High shelf"}, 6,
    "The filter type. Types the algorithm does not offer are greyed out (set by automation, they pass the audio unchanged)."};

enum class EqAlgorithm
{
    Rbj,
    Orfanidis,
    Zoelzer,
    StateVariable,
    Butterworth,
    LinkwitzRiley
};

// One setting of the EQ, in physical units
struct EqSetting
{
    EqAlgorithm algorithm = EqAlgorithm::Rbj;
    pluginlab::reference::FilterType type = pluginlab::reference::FilterType::Peak;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 0.71;
    int order = 2;

    bool operator==(const EqSetting& other) const;
};

// What a setting becomes: biquad sections, or the state-variable structure, or nothing (combination not offered)
struct EqDesign
{
    bool supported = true;
    bool stateVariable = false;
    double frequencyHz = 1000.0;  // after the limit to 0.45 fs
    std::vector<pluginlab::reference::BiquadCoefficients> sections;
    juce::String description;
};

EqSetting makeEqSetting(int algorithmIndex, int typeIndex, double frequencyHz, double gainDb, double q, double order);

// Whether an algorithm offers a filter type (RBJ and state variable: all; Orfanidis: peak; Zoelzer: shelves and peak; Butterworth, Linkwitz-Riley:
// low-pass and high-pass), and the type the GUI switches to when the chosen one is not offered
bool isTypeAvailable(EqAlgorithm algorithm, pluginlab::reference::FilterType type);
pluginlab::reference::FilterType getDefaultType(EqAlgorithm algorithm);
EqDesign designEq(const EqSetting& setting, double sampleRate);
// The exact response of the design (1 for a combination that is not offered)
std::complex<double> getEqResponse(const EqDesign& design, const EqSetting& setting, double frequencyHz, double sampleRate);

class ReferenceEqAudio : public SynchronBlockProcessor
{
public:
    explicit ReferenceEqAudio(ReferenceEqAudioProcessor* processor);
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
    EqSetting readSetting() const;
    void applySetting(const EqSetting& setting);

    ReferenceEqAudioProcessor* m_processor;
    int m_Latency = 0;
    double m_sampleRate = 48000.0;
    std::atomic<float>* m_algorithm = nullptr;
    std::atomic<float>* m_type = nullptr;
    std::atomic<float>* m_frequency = nullptr;
    std::atomic<float>* m_gain = nullptr;
    std::atomic<float>* m_q = nullptr;
    std::atomic<float>* m_order = nullptr;
    EqSetting m_setting;
    bool m_settingValid = false;
    EqDesign m_design;
    std::unique_ptr<pluginlab::reference::BiquadCascade> m_cascade;
    std::unique_ptr<pluginlab::reference::StateVariableFilter> m_stateVariable;
};

class ReferenceEqGUI : public juce::Component, private juce::Timer
{
public:
    ReferenceEqGUI(ReferenceEqAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    EqSetting readSetting() const;
    // Greys out the types the algorithm does not offer and moves an unavailable type to the algorithm's default
    void updateTypeChoice(const EqSetting& setting);

    ReferenceEqAudioProcessor& m_processor;
    juce::AudioProcessorValueTreeState& m_apvts;
    pluginlab::refplugins::ReferenceControls m_controls;
    juce::Rectangle<int> m_plotArea;
    EqSetting m_shownSetting;
    bool m_typesShown = false;
    EqAlgorithm m_typesAlgorithm = EqAlgorithm::Rbj;
};
