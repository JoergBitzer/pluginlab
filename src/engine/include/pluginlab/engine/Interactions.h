#pragma once

#include <functional>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/ParameterSurvey.h"

namespace pluginlab::engine
{
// Interactions between bands (W8.7, docs/eqmodel/interactions.md): do the bands of a plugin add in dB? For each pair of mapped bands, both
// set to peaks (through the mapping) at several distances and with equal and opposite gains; the response of both together against the sum
// of the single bands: dB(A+B) - dB(A) - dB(B) + dB(neither). A series of independent bands adds exactly; parallel bands (analogue designs),
// proportional-Q bands that share a stage, or an output stage that reacts to the level do not.

struct InteractionSettings
{
    double sampleRate = 48000.0;
    double centreHz = 1000.0;                              // band A
    std::vector<double> distancesOctaves = {0.0, 1.0 / 3.0, 1.0, 3.0}; // band B above A
    double gainDb = 6.0;                                   // both +gain, then +gain and -gain
    double q = 1.0;
    double toleranceDb = 0.05;                             // largest deviation from the sum below this: the bands add
    double levelDbfs = -12.0;
    double sweepSeconds = 1.0;
};

struct InteractionCase
{
    int bandA = 0;
    int bandB = 0;
    double frequencyA = 0.0;
    double frequencyB = 0.0;
    double gainA = 0.0;
    double gainB = 0.0;
    double largestDeviationDb = 0.0;   // signed: dB(A+B) minus the sum of the single bands, where its magnitude is largest
    double atHz = 0.0;
    bool interacts = false;
    juce::String note;                 // why a case could not be measured
};

struct InteractionResult
{
    juce::String pluginName;
    InteractionSettings settings;
    int bands = 0;
    std::vector<InteractionCase> cases;
    bool measured = false;             // at least two bands with frequency and gain
    bool additive = true;
    juce::String conclusion;
    int renders = 0;
    double seconds = 0.0;
};

using InteractionProgress = std::function<void(double fraction, const juce::String& step)>;

InteractionResult measureInteractions(const SurveySubject& subject, const Mapping& mapping, const InteractionSettings& settings = {},
                                      const InteractionProgress& progress = {});

// Markdown: the cases (bands, distance, gains, largest deviation and where) and the conclusion
juce::String createInteractionReport(const InteractionResult& result);
}
