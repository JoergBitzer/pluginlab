#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "HostSettings.h"
#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::host
{
// What the pages Matcher (W9.6) and Easy (W10.4) share about a match: the job file for PluginLabHost --match, the child process that runs it
// (a crashing plugin does not take the host or the DAW down), the result it writes next to the report, applying the found values to the loaded
// plugins, the parameters a mapping releases, and the matched settings as presets.

struct MatchJobTarget
{
    juce::String plugin;          // PluginDescription::fileOrIdentifier
    std::vector<float> values;    // the current setting (the start)
    std::vector<int> released;    // the parameters the match may move
    bool modelStart = true;
};

struct MatchJobSpec
{
    juce::String masterPlugin;    // a loaded plugin as the master (with masterValues), or
    std::vector<float> masterValues;
    juce::var masterCurve;        // a band curve (the Matcher page), used when masterPlugin is empty
    std::vector<MatchJobTarget> targets;
    juce::String tolerance = "strict";
    double seconds = 15.0;
    double lowestHz = 20.0;
    double highestHz = 20000.0;
};

// Writes the job; empty if it was written, else what is wrong
juce::String writeMatchJob(const MatchJobSpec& spec, const juce::File& jobFile);

// One target of the result (<report>.json)
struct MatchOutcome
{
    juce::String plugin;          // PluginDescription::createIdentifierString
    juce::String name;
    bool refused = false;
    juce::String reason;          // why it was refused
    bool matched = false;         // within the tolerance
    double largestDb = 0.0;       // the largest deviation from the master
    double worstHz = 0.0;         // where
    juce::String cause;           // the main reason when not within the tolerance (W9.5), in words
    juce::String evidence;
    double levelDb = 0.0;         // the matched target's level against the master (mean dB difference)
    std::vector<float> setting;   // the whole found setting
    std::vector<int> released;
};

std::vector<MatchOutcome> readMatchResult(const juce::File& reportFile);

// Sets the released values of each outcome on the loaded plugin with the same identifier among `slots`; returns how many plugins got them
int applyMatchOutcomes(engine::MeasurementEngine& engine, const std::vector<MatchOutcome>& outcomes, const std::vector<int>& slots);

// The band parameters (frequency, gain, Q) the plugin's mapping (W8.5) releases; empty without a mapping
std::vector<int> getMappedParameters(const HostSettings& settings, const juce::PluginDescription& description);

// Without a mapping: every continuous, automatable parameter that is not a bypass (the musician cannot map a plugin inside a DAW)
std::vector<int> getContinuousParameters(engine::MeasurementEngine& engine, int slot);

// The state of each plugin of `slots` as a preset "matched to <master> <date>" in the settings folder (shared by both products); returns how many
int saveMatchedPresets(engine::MeasurementEngine& engine, const HostSettings& settings, const std::vector<int>& slots, const juce::String& masterName);

// Runs PluginLabHost --match <job> <report> (the helper of PageServices; another mode such as --expert by `option`) in a child process on a
// thread of its own; tells its progress and its end on the message thread. Destroying it stops the child.
class MatchJob
{
public:
    using Progress = std::function<void(double fraction, const juce::String& step)>;
    using Finished = std::function<void(bool success, const juce::String& message)>;

    MatchJob(juce::File job, juce::File report, Progress progress, Finished finished, juce::String option = "--match");
    ~MatchJob();

    const juce::File& getReport() const { return m_report; }

private:
    class Runner;
    juce::File m_report;
    std::shared_ptr<std::atomic<bool>> m_alive;
    std::unique_ptr<Runner> m_runner;
};
}
