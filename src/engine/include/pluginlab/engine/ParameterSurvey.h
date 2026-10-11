#pragma once

#include <complex>
#include <functional>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/SystemClass.h"

namespace pluginlab::engine
{
// Parameter survey (W8.4, docs/eqmodel/parameter-survey.md): every parameter alone over its range, measured and classified. One render per
// point: silence (the idle output: noise, hum, DC), then a synchronized sweep on the left input only (the direct path left -> left, the cross
// path left -> right, the delay, the harmonics). A parameter that changes nothing at the default setting is searched for a context (one or two
// other parameters moved) in which it acts: it is reported as coupled ("acts only when ..."), not guessed. The system class (W8.1) at the ends of
// each acting parameter tells a drive knob that makes an EQ non-linear.

// What is surveyed: the parameters (normalised 0 ... 1) and a device for a setting
struct SurveyParameter
{
    juce::String name;
    juce::String label;                 // the unit the plugin shows
    int numSteps = 0;                   // 0 or more than kContinuousSteps: continuous
    bool isBoolean = false;
    float defaultValue = 0.0f;
    std::function<juce::String(float)> getText; // the text the plugin shows for a normalised value
};

struct SurveySubject
{
    std::vector<SurveyParameter> parameters;
    std::function<measure::Device(const std::vector<float>& setting)> makeDevice;
    int channels = 2;                   // of a render (1: no cross path)
};

// A hosted plugin as a survey subject (the careful delivery of makePluginDevice)
SurveySubject makePluginSubject(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate);

enum class ParameterRole
{
    Nothing,                            // no change in any measurement, in any context tried
    Level,                              // the direct path changes by the same dB at every frequency
    Frequency,                          // moves the frequency of a band
    Gain,                               // changes the gain of a band
    Width,                              // changes the Q (bandwidth, shelf slope) of a band
    Slope,                              // changes the order of a band
    Latency,                            // only the delay changes
    Polarity,                           // only the sign changes
    Stereo,                             // the cross path changes (crosstalk, width)
    Distortion,                         // the harmonics change
    IdleOutput,                         // the output without input changes (noise, hum, DC)
    Modulation,                         // the system is time-variant and the parameter changes it
    Switch,                             // a discrete parameter that changes the structure (type, design, band on/off, curve)
    Other                               // changes the response, but no single band quantity explains it
};

juce::String getRoleName(ParameterRole role);

// One measured point of a parameter
struct SurveyPoint
{
    float value = 0.0f;                 // normalised
    juce::String text;                  // as the plugin shows it
    std::vector<std::complex<double>> direct;   // left -> left on SurveySettings::frequencies (with the delay)
    std::vector<std::complex<double>> cross;    // left -> right (empty for one channel)
    int delaySamples = 0;               // peak of the impulse response
    double harmonicDb = -300.0;         // largest |H_n / H_1|, n = 2 ... 5, in the band of W8.1
    double idleDbfs = -300.0;           // rms of the stationary output without input (both channels; second half of the silence)
    double settlingDbfs = -300.0;       // a decaying output in the silence instead (the setting still settles: a resonance rings)
    std::optional<eqmodel::FitResult> fit;      // when the response had to be described
};

// The survey of one parameter in one context
struct ContextSurvey
{
    std::vector<int> contextParameters; // the other parameters moved (empty: the default setting)
    std::vector<float> contextValues;
    juce::String contextText;           // "Gain = +21.6 dB, Enabled = On"
    std::vector<SurveyPoint> points;
    ParameterRole role = ParameterRole::Nothing;
    juce::String detail;                // what the evidence says (band quantities, ranges, what a switch changes)
    juce::StringArray alsoChanges;      // secondary effects (a gain knob that also changes the width: proportional Q)
    std::vector<measure::SystemClass> classes;  // at the first and the last point (if classified)
    bool changesSystemClass = false;
};

struct ParameterSurvey
{
    int index = 0;
    SurveyParameter parameter;
    ContextSurvey atDefault;
    std::optional<ContextSurvey> inContext; // where a parameter that does nothing at the default acts, or a switch with an active band
    ParameterRole role = ParameterRole::Nothing; // the role in the most telling context
    bool coupled = false;               // acts only in another context
    juce::String summary;               // one line
};

struct SurveySettings
{
    double sampleRate = 48000.0;
    double levelDbfs = -12.0;           // peak of the sweep
    double sweepSeconds = 1.0;
    double idleSeconds = 0.5;           // silence before the sweep (after the delivery's own settling)
    std::vector<double> frequencies;    // empty: 12 per octave, 20 Hz ... 20 kHz
    int continuousPoints = 5;           // normalised 0.05 ... 0.95 (the ends of a range are often outside what a band model describes)
    int maximumSteps = 16;              // a parameter with up to this many steps: every step
    double nothingDb = 0.01;            // a change below this (magnitude, cross path) is no change
    double flatDb = 0.02;               // a change of the direct path flat within this: a level
    double harmonicChangeDb = 1.0;      // harmonics above harmonicFloorDb that change by more
    double harmonicFloorDb = -100.0;
    double idleChangeDb = 3.0;          // the idle output above idleFloorDbfs that changes by more
    double idleFloorDbfs = -140.0;
    double crossFloorDb = -100.0;       // the cross path counts above this (re the direct path)
    double frequencyChangeOctaves = 0.02;
    double gainChangeDb = 0.05;
    double widthChange = 0.03;          // relative Q change
    bool classify = true;               // the system class at the ends of each acting parameter
    bool searchPairs = true;            // contexts with two other parameters moved
    int maximumSearchRenders = 200;     // per parameter
};

struct SurveyResult
{
    SurveySettings settings;
    std::vector<double> frequencies;
    SurveyPoint defaultPoint;           // the default setting
    std::optional<measure::SystemClass> defaultClass;
    std::vector<ParameterSurvey> parameters;
    int renders = 0;
    double renderSeconds = 0.0;         // where the time went
    double classifySeconds = 0.0;
    double fitSeconds = 0.0;
};

using SurveyProgress = std::function<void(int parameterIndex, int parameterCount)>;

SurveyResult surveyParameters(const SurveySubject& subject, const SurveySettings& settings, const SurveyProgress& progress = {});

// The normalised values a parameter is surveyed at
std::vector<float> getSurveyValues(const SurveyParameter& parameter, const SurveySettings& settings);

// Markdown: one table row per parameter, then the evidence
juce::String createSurveyReport(const SurveyResult& result, const juce::String& title);
}
