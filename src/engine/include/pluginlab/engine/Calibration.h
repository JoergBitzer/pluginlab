#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/ParameterSurvey.h"

namespace pluginlab::engine
{
// Calibration, "myth and reality" (W8.6, docs/eqmodel/calibration.md): for each band parameter of a mapping (frequency, gain, Q, order) the value
// the plugin shows against the value the band model measures (W8.3), at several knob positions and at 44.1, 48 and 96 kHz. Only the deviations
// are reported, with their pattern (a constant factor, a factor that follows the sample rate, an offset, a scale, a Q knob that is a bandwidth).
// No corrected knob law is handed on: the matcher (W9) needs only a few examples.

struct CalibrationSettings
{
    std::vector<double> sampleRates = {44100.0, 48000.0, 96000.0};
    int continuousPoints = 7;            // knob positions 0.05 ... 0.95 of a continuous parameter (a discrete one: every step, up to maximumSteps)
    int maximumSteps = 16;
    double levelDbfs = -12.0;            // peak of the sweep
    double sweepSeconds = 1.0;
    double frequencyTolerance = 0.01;    // relative: a band within 1 % of the frequency shown is "as shown"
    double gainToleranceDb = 0.1;
    double qTolerance = 0.03;            // relative
    double silentGainDb = 0.05;          // bands of the fit with less gain are left out
};

// One knob position at one sample rate
struct CalibrationPoint
{
    float value = 0.0f;                  // normalised
    juce::String text;                   // as the plugin shows it
    double shown = 0.0;                  // the number in the text
    double resolution = 0.0;             // half a unit of the text's last digit ("28 Hz": 0.5 Hz)
    std::optional<double> measured;      // from the band fit; empty if it cannot be measured (no band, a band outside 20 Hz ... 20 kHz)
    juce::String note;                   // why it was not measured
    bool deviates = false;
};

// One parameter at one sample rate
struct CalibrationSeries
{
    double sampleRate = 0.0;
    std::vector<CalibrationPoint> points;
    int measuredPoints = 0;
    bool deviates = false;
    double factor = 1.0;                 // frequency, Q: the mean ratio measured / shown; gain: the slope a of measured = a shown + b
    double offset = 0.0;                 // gain: b
    bool constant = false;               // the ratio (frequency, Q) or the straight line (gain) describes every point within the tolerance
    juce::String pattern;                // "x 1.088", "offset +0.50 dB", "a bandwidth in octaves", "varies: x 0.97 ... x 1.12"
};

struct ParameterCalibration
{
    int index = 0;
    juce::String name;
    MappedQuantity quantity = MappedQuantity::None;
    int band = -1;
    juce::String context;                // the other parameters set for it to act ("Gain = 21.6 dB")
    std::vector<CalibrationSeries> rates;
    bool deviates = false;               // at any rate
    juce::String conclusion;             // over the rates: "follows the sample rate: designed for 44.1 kHz", "the same at every rate", ...
};

struct CalibrationResult
{
    juce::String pluginName;
    CalibrationSettings settings;
    std::vector<ParameterCalibration> parameters;
    int renders = 0;
    double seconds = 0.0;
};

using CalibrationProgress = std::function<void(double fraction, const juce::String& step)>;

// The calibration of the mapped band parameters (the subject gives the texts and a device per setting; the device runs at any sample rate)
CalibrationResult calibrate(const SurveySubject& subject, const Mapping& mapping, const CalibrationSettings& settings = {},
                            const CalibrationProgress& progress = {});

// Markdown: the deviations, their patterns, the points of the parameters that deviate; what is as shown in one line
juce::String createCalibrationReport(const CalibrationResult& result);

// Markdown: the end of a calibration report (what, why, the tolerances)
juce::String createCalibrationReportFooter(const CalibrationSettings& settings);

// Half a unit of the last digit of the first number in a text ("1.25 kHz": 5 Hz, "28 Hz": 0.5 Hz); 0 if there is no number
double getDisplayedResolution(const juce::String& text);
}
