#pragma once

#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/engine/KnobLaw.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/eqmodel/EqModel.h"

namespace pluginlab::engine
{
// The mapping of a plugin's parameters to the band model (W8.5, docs/eqmodel/mapping.md): which parameter is which band quantity, its knob law (as
// the plugin shows it and as measured), what it needs to act (another parameter's value), and what the steps of a switch are. Proposed from the
// survey (W8.4), confirmed by a person, stored as JSON: our own plugins in the repository (mappings/), all others in the user's settings folder.

// Which band quantity (or other effect) a parameter controls
enum class MappedQuantity
{
    None,
    Frequency,     // Hz
    Gain,          // dB
    Q,
    Order,
    Type,          // a switch of band types
    Family,        // a switch of design families
    BandOnOff,     // a switch that turns a band on and off
    OutputLevel,   // dB
    Latency,       // samples
    Other          // surveyed, not a band quantity (stereo, distortion, noise, ...): the role says what
};

const char* getMappedQuantityName(MappedQuantity quantity);
juce::String getMappedQuantityUnit(MappedQuantity quantity);

struct Requirement
{
    int parameter = 0;
    juce::String name;
    float value = 0.0f;           // normalised
    juce::String text;
};

struct SwitchStep
{
    float value = 0.0f;           // normalised
    juce::String text;            // as the plugin shows it
    juce::String description;     // what the measurement found ("peak 1000 Hz +21.6 dB Q 0.71 (RBJ cookbook)", "flat")
    std::optional<reference::FilterType> type;     // of the dominant band
    std::optional<eqmodel::DesignFamily> family;
    bool bandPresent = false;
};

struct MappedParameter
{
    int index = 0;
    juce::String name;
    juce::String role;            // getRoleName of the survey
    MappedQuantity quantity = MappedQuantity::None;
    int band = -1;                // band number (0 ...) for band quantities and band switches
    std::vector<Requirement> requirements; // what it needs to act (the survey's context)
    std::optional<KnobLaw> displayedLaw;   // from the texts the plugin shows
    std::optional<KnobLaw> measuredLaw;    // from the survey's measurements
    std::vector<SwitchStep> steps;         // switches
    float defaultValue = 0.0f;
};

struct Mapping
{
    static constexpr int kSchema = 1;
    juce::String pluginName;
    juce::String manufacturer;
    juce::String version;
    juce::String format;
    juce::String identifier;      // see PluginIdentity
    std::vector<double> sampleRates;
    juce::String date;            // ISO date of the proposal
    juce::String proposedBy;      // program and version
    juce::String confirmedBy;     // empty: a proposal nobody has checked
    juce::String notes;
    std::vector<MappedParameter> parameters;
    int bands = 0;

    juce::var toVar() const;
    static std::optional<Mapping> fromVar(const juce::var& value, juce::String& error);
    bool write(const juce::File& file) const;
    static std::optional<Mapping> read(const juce::File& file, juce::String& error);

    const MappedParameter* find(int band, MappedQuantity quantity) const;
};

struct PluginIdentity
{
    juce::String name;
    juce::String manufacturer;
    juce::String version;
    juce::String format;
    juce::String identifier;      // format, name and unique id ("VST3:PluginLab Reference EQ:1a2b3c4d"): the same on every machine
};

PluginIdentity makePluginIdentity(const juce::PluginDescription& description);

// The proposal from a survey; the subject gives the texts of the knobs (sampled at kDisplaySamples values)
Mapping proposeMapping(const SurveySubject& subject, const SurveyResult& survey, const PluginIdentity& identity, const juce::String& proposedBy);

// A setting (normalised values) that makes the plugin realise the band (as measured: the measured laws, else the displayed ones): the switches for
// type and family, the band's quantities, the requirements. Starts from base; `missing` lists what the mapping cannot set.
std::vector<float> makeSettingForBand(const Mapping& mapping, const std::vector<float>& base, int band, const eqmodel::Band& wanted, juce::StringArray& missing);

// Markdown: one row per parameter
juce::String createMappingReport(const Mapping& mapping);

// A file name for a plugin's mapping ("PluginLab Reference EQ" -> "pluginlab_reference_eq.json")
juce::String getMappingFileName(const juce::String& pluginName);

// How a proposal differs from another mapping (e.g. the confirmed file): one line per difference; empty if they agree within the tolerances
juce::StringArray compareMappings(const Mapping& proposal, const Mapping& confirmed);
}
