#pragma once

#include <optional>
#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "pluginlab/engine/LivePluginDevice.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/eqmodel/EqModel.h"
#include "pluginlab/match/EqDomain.h"
#include "pluginlab/match/Match.h"

namespace pluginlab::engine
{
// Matching plugins (W9.4, docs/matching/plugin-matching.md): the master (a plugin at a setting, or a band-model curve) and one or more targets,
// each matched one after another through the fast render path (W9.3) within its budget. Only the parameters the user released are moved;
// switches stay as the user set them. The main case (the author): the user's own EQ as the master, several new EQs as targets.

struct MatchMaster
{
    std::optional<juce::PluginDescription> plugin;   // a plugin at `setting`, or
    std::vector<float> setting;
    std::optional<eqmodel::EqModel> curve;           // a band-model curve (no render)
    juce::String describe() const;
};

struct MatchTarget
{
    juce::PluginDescription plugin;
    std::vector<float> start;                        // the whole setting as the user left it (switches set); empty: the defaults
    std::vector<int> released;                       // the parameters to optimize (indices), each over its whole range
    std::optional<Mapping> mapping;                  // for the model start point (the target's mapping, W8.5)
    bool modelStart = false;                         // start from the master's bands through the mapping instead of the user's values
};

struct PluginMatchSettings
{
    match::MagnitudeProbeSettings probe;             // impulse stimulus by default (the fast path)
    match::MatchTolerance tolerance = match::MatchTolerance::Strict;
    match::Budget budget{400, 15.0};
    LiveDeviceOptions live;
    PluginMatchSettings();
};

struct ReleasedValue
{
    int index = 0;
    juce::String name;
    float value = 0.0f;
    juce::String text;                               // as the plugin shows it
    bool atBound = false;
};

struct TargetMatchResult
{
    juce::String name;
    bool refused = false;
    juce::String reason;                             // why it was refused
    match::OptimizerResult optimizer;
    std::vector<float> setting;                      // the found setting (the whole setting)
    std::vector<ReleasedValue> values;
    std::vector<double> targetFeatures;              // the target's magnitude at the found setting
    bool startedFromModel = false;
    double seconds = 0.0;                            // with the checks
};

struct PluginMatchResult
{
    juce::String master;
    std::vector<double> frequencies;
    std::vector<double> masterFeatures;
    match::MatchTolerance tolerance = match::MatchTolerance::Strict;
    std::vector<TargetMatchResult> targets;
    juce::String error;                              // the master could not be measured
};

using PluginMatchProgress = std::function<void(double fraction, const juce::String& step)>;

PluginMatchResult matchPlugins(juce::AudioPluginFormatManager& formatManager, const MatchMaster& master, const std::vector<MatchTarget>& targets,
                               const PluginMatchSettings& settings = {}, const PluginMatchProgress& progress = {});

// The start from the model: the master's bands (sorted by frequency) set through the target's mapping on the target's mapped bands, from base
std::vector<float> makeModelStart(const Mapping& mapping, const std::vector<float>& base, const eqmodel::EqModel& masterBands);

// Markdown: per target the result, the deviation, the found setting in the plugin's texts, renders and time; refusals with their reasons
juce::String createPluginMatchReport(const PluginMatchResult& result);
}
