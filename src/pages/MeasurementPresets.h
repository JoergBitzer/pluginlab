#pragma once

#include <optional>
#include <vector>

#include <juce_core/juce_core.h>

#include "HostSettings.h"

namespace pluginlab::host
{
// Measurement presets (W10.6, docs/design/W10-plan.md 3.5): what the page Expert measures, saved under a name with a note, as JSON in the
// settings folder (measurement_presets/), so that the application and the plugin share them. The shipped presets for the teaching cases are
// written into the folder when they are missing (a preset the user changed is never overwritten).
struct MeasurementPreset
{
    juce::String name;
    juce::String note;                  // what it is for, how to set up the slots
    juce::StringArray units;            // the keys of engine::getMeasurementUnitKeys(); empty: only the system class
    bool includeDry = false;            // the dry slot is measured too
    juce::String nullMaster = "none";   // "none", "dry", "first plugin" (the slots are chosen by role, not by plugin)
    bool shipped = false;               // one of the teaching presets

    juce::var toJson() const;
    static std::optional<MeasurementPreset> fromJson(const juce::var& json);
};

// The presets in the folder, sorted by name; the shipped ones are written first if they are missing
std::vector<MeasurementPreset> loadMeasurementPresets(const HostSettings& settings);

// Saves (or replaces) the preset of that name; false if the file cannot be written
bool saveMeasurementPreset(const HostSettings& settings, const MeasurementPreset& preset);

// The teaching presets: "Is it LTI?", "Full AES17 set", "Null test of two EQs", "Orfanidis against RBJ"
std::vector<MeasurementPreset> getShippedMeasurementPresets();
}
