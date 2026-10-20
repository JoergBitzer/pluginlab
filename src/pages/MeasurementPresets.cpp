#include "MeasurementPresets.h"

#include <algorithm>

#include "pluginlab/engine/OfflineRenderer.h"
#include "pluginlab/engine/PluginMeasurement.h"

namespace pluginlab::host
{
namespace
{
constexpr int kFormatVersion = 1;
const juce::String kExtension = ".json";

juce::File getPresetFile(const HostSettings& settings, const juce::String& name)
{
    return settings.getMeasurementPresetFolder().getChildFile(engine::makeFileNamePart(name) + kExtension);
}
}

juce::var MeasurementPreset::toJson() const
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("format", "pluginlab measurement preset");
    root->setProperty("version", kFormatVersion);
    root->setProperty("name", name);
    root->setProperty("note", note);
    juce::Array<juce::var> list;
    for (const juce::String& unit : units)
    {
        list.add(unit);
    }
    root->setProperty("units", list);
    root->setProperty("includeDry", includeDry);
    root->setProperty("nullMaster", nullMaster);
    root->setProperty("shipped", shipped);
    return juce::var(root.release());
}

std::optional<MeasurementPreset> MeasurementPreset::fromJson(const juce::var& json)
{
    if (json.getProperty("format", "").toString() != "pluginlab measurement preset" || json.getProperty("name", "").toString().isEmpty())
    {
        return std::nullopt;
    }
    MeasurementPreset preset;
    preset.name = json.getProperty("name", "").toString();
    preset.note = json.getProperty("note", "").toString();
    if (const juce::Array<juce::var>* list = json.getProperty("units", {}).getArray())
    {
        for (const juce::var& unit : *list)
        {
            if (engine::getMeasurementUnitKeys().contains(unit.toString()))
            {
                preset.units.add(unit.toString()); // an unknown unit (a newer version) is left out
            }
        }
    }
    preset.includeDry = json.getProperty("includeDry", false);
    preset.nullMaster = json.getProperty("nullMaster", "none").toString();
    preset.shipped = json.getProperty("shipped", false);
    return preset;
}

std::vector<MeasurementPreset> getShippedMeasurementPresets()
{
    std::vector<MeasurementPreset> presets;
    MeasurementPreset lti;
    lti.name = "Is it LTI?";
    lti.note = "Only the system class (W8.1): linear and time-invariant, time-variant, or non-linear with a first guess of the kind. Decides which "
               "of the other measurements mean what.";
    presets.push_back(lti);

    MeasurementPreset full;
    full.name = "Full AES17 set";
    full.note = "Every measurement unit of W7 with its AES17 defaults, the slow gain linearity included.";
    full.units = engine::getMeasurementUnitKeys();
    presets.push_back(full);

    MeasurementPreset nullTest;
    nullTest.name = "Null test of two EQs";
    nullTest.note = "Load two EQs set alike; the second is null-tested against the first (aligned in delay and gain); frequency response and phase "
                    "of both. A magnitude match is not a null: the phase (minimum against linear phase) shows here.";
    nullTest.units = {"response", "phase"};
    nullTest.nullMaster = "first plugin";
    presets.push_back(nullTest);

    MeasurementPreset designs;
    designs.name = "Orfanidis against RBJ";
    designs.note = "Load the Reference EQ twice: the first with the RBJ cookbook, the second with the Orfanidis design, both the same peak near "
                   "Nyquist (e.g. 16 kHz, +12 dB, Q 2). The responses differ where RBJ cramps towards Nyquist; the null test shows how much "
                   "(docs/teaching/matching-orfanidis-rbj.md).";
    designs.units = {"response", "phase"};
    designs.nullMaster = "first plugin";
    presets.push_back(designs);
    for (MeasurementPreset& preset : presets)
    {
        preset.shipped = true;
    }
    return presets;
}

bool saveMeasurementPreset(const HostSettings& settings, const MeasurementPreset& preset)
{
    settings.getMeasurementPresetFolder().createDirectory();
    return getPresetFile(settings, preset.name).replaceWithText(juce::JSON::toString(preset.toJson()));
}

std::vector<MeasurementPreset> loadMeasurementPresets(const HostSettings& settings)
{
    for (const MeasurementPreset& shipped : getShippedMeasurementPresets())
    {
        if (! getPresetFile(settings, shipped.name).existsAsFile())
        {
            saveMeasurementPreset(settings, shipped);
        }
    }
    std::vector<MeasurementPreset> presets;
    for (const juce::File& file : settings.getMeasurementPresetFolder().findChildFiles(juce::File::findFiles, false, "*" + kExtension))
    {
        if (const std::optional<MeasurementPreset> preset = MeasurementPreset::fromJson(juce::JSON::parse(file.loadFileAsString())))
        {
            presets.push_back(*preset);
        }
    }
    std::sort(presets.begin(), presets.end(), [](const MeasurementPreset& a, const MeasurementPreset& b) { return a.name.compareNatural(b.name) < 0; });
    return presets;
}
}
