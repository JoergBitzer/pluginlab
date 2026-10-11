#include "pluginlab/engine/Mapping.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace pluginlab::engine
{
namespace
{
constexpr int kDisplaySamples = 101;          // texts of a continuous knob sampled at this many values (no render needed)
constexpr double kSameBandOctaves = 1.0 / 3.0; // parameters whose bands lie this close belong to the same band
constexpr double kPassBandWeight = 20.0;      // a pass filter counts like a 20 dB band when the dominant band is chosen
constexpr double kLevelReferenceHz = 1000.0;
constexpr double kLawRangeTolerance = 0.005;  // compareMappings: law ends within 0.5 % of the range
constexpr double kSkewTolerance = 0.02;

bool hasGain(reference::FilterType type)
{
    return type == reference::FilterType::Peak || type == reference::FilterType::LowShelf || type == reference::FilterType::HighShelf;
}

bool isDiscrete(const SurveyParameter& parameter, const SurveySettings& settings)
{
    return parameter.isBoolean || (parameter.numSteps >= 2 && parameter.numSteps <= settings.maximumSteps);
}

// The band that describes a point: the largest gain, a pass filter counted as kPassBandWeight dB
const eqmodel::Band* getDominantBand(const SurveyPoint& point)
{
    if (! point.fit || ! point.fit->goodDescription)
    {
        return nullptr;
    }
    const eqmodel::Band* best = nullptr;
    double bestSize = 0.0;
    for (const eqmodel::Band& band : point.fit->model.bands)
    {
        const double size = hasGain(band.type) ? std::abs(band.gainDb) : kPassBandWeight;
        if (size > bestSize)
        {
            bestSize = size;
            best = &band;
        }
    }
    return best;
}

juce::String describeBand(const eqmodel::Band& band)
{
    juce::String text = juce::String(reference::getFilterTypeName(band.type)) + " " + juce::String(band.frequencyHz, 0) + " Hz";
    if (hasGain(band.type))
    {
        text << " " << (band.gainDb >= 0.0 ? "+" : "") << juce::String(band.gainDb, 1) << " dB";
    }
    if (band.family == eqmodel::DesignFamily::Butterworth || band.family == eqmodel::DesignFamily::LinkwitzRiley)
    {
        text << " order " << band.order;
    }
    else
    {
        text << " Q " << juce::String(band.q, 2);
    }
    return text + " (" + eqmodel::getFamilyName(band.family) + ")";
}

MappedQuantity getQuantity(ParameterRole role)
{
    switch (role)
    {
        case ParameterRole::Frequency: return MappedQuantity::Frequency;
        case ParameterRole::Gain: return MappedQuantity::Gain;
        case ParameterRole::Width: return MappedQuantity::Q;
        case ParameterRole::Slope: return MappedQuantity::Order;
        case ParameterRole::Level: return MappedQuantity::OutputLevel;
        case ParameterRole::Latency: return MappedQuantity::Latency;
        case ParameterRole::Nothing: return MappedQuantity::None;
        case ParameterRole::Switch: return MappedQuantity::Other; // decided from the steps
        default: return MappedQuantity::Other;
    }
}

bool isBandQuantity(MappedQuantity quantity)
{
    return quantity == MappedQuantity::Frequency || quantity == MappedQuantity::Gain || quantity == MappedQuantity::Q || quantity == MappedQuantity::Order
           || quantity == MappedQuantity::Type || quantity == MappedQuantity::Family || quantity == MappedQuantity::BandOnOff;
}

// The measured value of a point for a quantity
std::optional<double> getMeasuredValue(const SurveyPoint& point, MappedQuantity quantity, const std::vector<double>& frequencies)
{
    const eqmodel::Band* band = getDominantBand(point);
    switch (quantity)
    {
        case MappedQuantity::Frequency:
            return band != nullptr ? std::optional<double>(band->frequencyHz) : std::nullopt;
        case MappedQuantity::Gain:
            if (band == nullptr)
            {
                return point.fit && point.fit->goodDescription ? std::optional<double>(0.0) : std::nullopt; // the band vanished: gain 0
            }
            return hasGain(band->type) ? std::optional<double>(band->gainDb) : std::nullopt;
        case MappedQuantity::Q:
            return band != nullptr ? std::optional<double>(band->q) : std::nullopt;
        case MappedQuantity::Order:
            return band != nullptr ? std::optional<double>(band->order) : std::nullopt;
        case MappedQuantity::OutputLevel:
        {
            if (point.direct.empty())
            {
                return std::nullopt;
            }
            size_t nearest = 0;
            for (size_t index = 1; index < frequencies.size(); ++index)
            {
                if (std::abs(std::log(frequencies[index] / kLevelReferenceHz)) < std::abs(std::log(frequencies[nearest] / kLevelReferenceHz)))
                {
                    nearest = index;
                }
            }
            return 20.0 * std::log10(std::max(std::abs(point.direct[nearest]), 1.0e-15));
        }
        case MappedQuantity::Latency:
            return static_cast<double>(point.delaySamples);
        default:
            return std::nullopt;
    }
}

// The frequency where a parameter's band sits (to group parameters into bands)
std::optional<double> getBandLocation(const MappedParameter& parameter, const ContextSurvey& context)
{
    if (parameter.quantity == MappedQuantity::Frequency && parameter.measuredLaw)
    {
        return parameter.measuredLaw->toValue(parameter.defaultValue);
    }
    std::vector<double> locations;
    for (const SurveyPoint& point : context.points)
    {
        if (const eqmodel::Band* band = getDominantBand(point))
        {
            locations.push_back(band->frequencyHz);
        }
    }
    if (locations.empty())
    {
        return std::nullopt;
    }
    std::sort(locations.begin(), locations.end());
    return locations[locations.size() / 2];
}

juce::var requirementToVar(const Requirement& requirement)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("parameter", requirement.parameter);
    object->setProperty("name", requirement.name);
    object->setProperty("value", requirement.value);
    object->setProperty("text", requirement.text);
    return juce::var(object);
}

std::optional<reference::FilterType> parseType(const juce::String& name)
{
    for (const reference::FilterType type :
         {reference::FilterType::LowPass, reference::FilterType::HighPass, reference::FilterType::BandPass, reference::FilterType::BandPassUnity,
          reference::FilterType::Notch, reference::FilterType::AllPass, reference::FilterType::Peak, reference::FilterType::LowShelf,
          reference::FilterType::HighShelf})
    {
        if (name == reference::getFilterTypeName(type))
        {
            return type;
        }
    }
    return std::nullopt;
}

std::optional<eqmodel::DesignFamily> parseFamily(const juce::String& name)
{
    for (const eqmodel::DesignFamily family : eqmodel::getAllFamilies())
    {
        if (name == eqmodel::getFamilyName(family))
        {
            return family;
        }
    }
    return std::nullopt;
}

std::optional<MappedQuantity> parseQuantity(const juce::String& name)
{
    for (const MappedQuantity quantity :
         {MappedQuantity::None, MappedQuantity::Frequency, MappedQuantity::Gain, MappedQuantity::Q, MappedQuantity::Order, MappedQuantity::Type,
          MappedQuantity::Family, MappedQuantity::BandOnOff, MappedQuantity::OutputLevel, MappedQuantity::Latency, MappedQuantity::Other})
    {
        if (name == getMappedQuantityName(quantity))
        {
            return quantity;
        }
    }
    return std::nullopt;
}

bool sameFamilyForSwitch(eqmodel::DesignFamily a, eqmodel::DesignFamily b)
{
    return a == b;
}
}

const char* getMappedQuantityName(MappedQuantity quantity)
{
    switch (quantity)
    {
        case MappedQuantity::None: return "none";
        case MappedQuantity::Frequency: return "frequency";
        case MappedQuantity::Gain: return "gain";
        case MappedQuantity::Q: return "q";
        case MappedQuantity::Order: return "order";
        case MappedQuantity::Type: return "type";
        case MappedQuantity::Family: return "family";
        case MappedQuantity::BandOnOff: return "band on/off";
        case MappedQuantity::OutputLevel: return "output level";
        case MappedQuantity::Latency: return "latency";
        case MappedQuantity::Other: return "other";
    }
    return "";
}

juce::String getMappedQuantityUnit(MappedQuantity quantity)
{
    switch (quantity)
    {
        case MappedQuantity::Frequency: return "Hz";
        case MappedQuantity::Gain:
        case MappedQuantity::OutputLevel: return "dB";
        case MappedQuantity::Latency: return "samples";
        default: return {};
    }
}

PluginIdentity makePluginIdentity(const juce::PluginDescription& description)
{
    PluginIdentity identity;
    identity.name = description.name;
    identity.manufacturer = description.manufacturerName;
    identity.version = description.version;
    identity.format = description.pluginFormatName;
    identity.identifier = description.pluginFormatName + ":" + description.name + ":" + juce::String::toHexString(description.uniqueId);
    return identity;
}

Mapping proposeMapping(const SurveySubject& subject, const SurveyResult& survey, const PluginIdentity& identity, const juce::String& proposedBy)
{
    Mapping mapping;
    mapping.pluginName = identity.name;
    mapping.manufacturer = identity.manufacturer;
    mapping.version = identity.version;
    mapping.format = identity.format;
    mapping.identifier = identity.identifier;
    mapping.sampleRates = {survey.settings.sampleRate};
    mapping.date = juce::Time::getCurrentTime().formatted("%Y-%m-%d");
    mapping.proposedBy = proposedBy;

    std::vector<const ContextSurvey*> contexts;
    for (const ParameterSurvey& parameterSurvey : survey.parameters)
    {
        MappedParameter parameter;
        parameter.index = parameterSurvey.index;
        parameter.name = parameterSurvey.parameter.name;
        parameter.role = getRoleName(parameterSurvey.role);
        parameter.defaultValue = parameterSurvey.parameter.defaultValue;
        parameter.quantity = getQuantity(parameterSurvey.role);
        const bool discrete = isDiscrete(parameterSurvey.parameter, survey.settings);
        // the context that tells most: where a coupled parameter acts; for a switch the extra context if it found bands
        const ContextSurvey* context = &parameterSurvey.atDefault;
        if (parameterSurvey.coupled || (parameterSurvey.inContext && parameterSurvey.inContext->role != ParameterRole::Nothing))
        {
            context = &*parameterSurvey.inContext;
        }
        contexts.push_back(context);
        if (parameterSurvey.coupled)
        {
            for (size_t index = 0; index < context->contextParameters.size(); ++index)
            {
                Requirement requirement;
                requirement.parameter = context->contextParameters[index];
                requirement.value = context->contextValues[index];
                const SurveyParameter& other = subject.parameters[static_cast<size_t>(requirement.parameter)];
                requirement.name = other.name;
                requirement.text = getValueText(other, requirement.value);
                parameter.requirements.push_back(requirement);
            }
        }
        // the displayed law: the knob's texts (every step of a discrete knob, else kDisplaySamples values)
        const SurveyParameter& surveyParameter = subject.parameters[static_cast<size_t>(parameter.index)];
        if (surveyParameter.getText)
        {
            std::vector<LawPoint> points;
            bool allNumbers = true;
            const int count = discrete ? (surveyParameter.isBoolean ? 2 : surveyParameter.numSteps) : kDisplaySamples;
            for (int index = 0; index < count; ++index)
            {
                const double v = static_cast<double>(index) / static_cast<double>(count - 1);
                const std::optional<double> number = parseDisplayedNumber(surveyParameter.getText(static_cast<float>(v)));
                if (! number)
                {
                    allNumbers = false;
                    break;
                }
                points.push_back({v, *number});
            }
            if (allNumbers && ! points.empty())
            {
                parameter.displayedLaw = discrete ? std::optional<KnobLaw>(makeTableLaw(points)) : fitKnobLaw(points);
            }
        }
        // a switch: its steps, and what kind of switch it is
        if (parameterSurvey.role == ParameterRole::Switch || (discrete && parameterSurvey.role != ParameterRole::Nothing))
        {
            std::map<juce::String, int> types;
            std::map<juce::String, int> families;
            int present = 0;
            for (const SurveyPoint& point : context->points)
            {
                SwitchStep step;
                step.value = point.value;
                step.text = point.text;
                if (const eqmodel::Band* band = getDominantBand(point))
                {
                    step.description = describeBand(*band);
                    step.type = band->type;
                    step.family = band->family;
                    step.bandPresent = true;
                    ++types[reference::getFilterTypeName(band->type)];
                    ++families[eqmodel::getFamilyName(band->family)];
                    ++present;
                }
                else if (point.fit && point.fit->goodDescription)
                {
                    step.description = "flat";
                }
                parameter.steps.push_back(step);
            }
            if (parameterSurvey.role == ParameterRole::Switch)
            {
                if (types.size() > 1)
                {
                    parameter.quantity = MappedQuantity::Type;
                }
                else if (families.size() > 1)
                {
                    parameter.quantity = MappedQuantity::Family;
                }
                else if (present > 0 && present < static_cast<int>(context->points.size()))
                {
                    parameter.quantity = MappedQuantity::BandOnOff;
                }
            }
        }
        // the measured law
        if (parameter.quantity != MappedQuantity::None && parameter.quantity != MappedQuantity::Other && parameter.quantity != MappedQuantity::Type
            && parameter.quantity != MappedQuantity::Family && parameter.quantity != MappedQuantity::BandOnOff)
        {
            // band quantities: only the points whose fit has the usual number of bands (at the edge of the band a fit may trade the band's
            // frequency against a small extra band), if three or more are left
            std::map<size_t, int> bandCounts;
            for (const SurveyPoint& point : context->points)
            {
                if (point.fit && point.fit->goodDescription && ! point.fit->model.bands.empty())
                {
                    ++bandCounts[point.fit->model.bands.size()];
                }
            }
            size_t usualCount = 0;
            int usualFrequency = 0;
            for (const auto& [count, frequency] : bandCounts)
            {
                if (frequency > usualFrequency)
                {
                    usualCount = count;
                    usualFrequency = frequency;
                }
            }
            std::vector<LawPoint> points;
            std::vector<LawPoint> usualPoints;
            for (const SurveyPoint& point : context->points)
            {
                if (const std::optional<double> value = getMeasuredValue(point, parameter.quantity, survey.frequencies))
                {
                    points.push_back({point.value, *value});
                    const bool usual = ! point.fit || point.fit->model.bands.empty() || point.fit->model.bands.size() == usualCount;
                    if (usual)
                    {
                        usualPoints.push_back({point.value, *value});
                    }
                }
            }
            if (! discrete && usualPoints.size() >= 3 && usualPoints.size() < points.size())
            {
                points = usualPoints;
            }
            parameter.measuredLaw = discrete ? std::optional<KnobLaw>(makeTableLaw(points)) : fitKnobLaw(points);
        }
        mapping.parameters.push_back(parameter);
    }

    // bands: band parameters whose bands lie within a third of an octave belong together; numbered by frequency
    std::vector<std::pair<double, std::vector<size_t>>> groups;
    for (size_t index = 0; index < mapping.parameters.size(); ++index)
    {
        MappedParameter& parameter = mapping.parameters[index];
        if (! isBandQuantity(parameter.quantity))
        {
            continue;
        }
        const std::optional<double> location = getBandLocation(parameter, *contexts[index]);
        if (! location)
        {
            continue;
        }
        bool placed = false;
        for (auto& [frequency, members] : groups)
        {
            if (std::abs(std::log2(*location / frequency)) <= kSameBandOctaves)
            {
                members.push_back(index);
                placed = true;
                break;
            }
        }
        if (! placed)
        {
            groups.push_back({*location, {index}});
        }
    }
    // a group without a frequency, gain, Q or order knob is a side effect (a fractional delay that falls like a shelf), not an EQ band
    groups.erase(std::remove_if(groups.begin(), groups.end(),
                                [&](const auto& group) {
                                    const bool hasKnob = std::any_of(group.second.begin(), group.second.end(), [&](size_t index) {
                                        const MappedQuantity quantity = mapping.parameters[index].quantity;
                                        return quantity == MappedQuantity::Frequency || quantity == MappedQuantity::Gain || quantity == MappedQuantity::Q
                                               || quantity == MappedQuantity::Order;
                                    });
                                    if (! hasKnob)
                                    {
                                        for (const size_t index : group.second)
                                        {
                                            mapping.parameters[index].quantity = MappedQuantity::Other;
                                        }
                                    }
                                    return ! hasKnob;
                                }),
                 groups.end());
    std::sort(groups.begin(), groups.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (size_t band = 0; band < groups.size(); ++band)
    {
        for (const size_t index : groups[band].second)
        {
            mapping.parameters[index].band = static_cast<int>(band);
        }
    }
    mapping.bands = static_cast<int>(groups.size());
    return mapping;
}

const MappedParameter* Mapping::find(int band, MappedQuantity quantity) const
{
    for (const MappedParameter& parameter : parameters)
    {
        if (parameter.band == band && parameter.quantity == quantity)
        {
            return &parameter;
        }
    }
    return nullptr;
}

std::vector<float> makeSettingForBand(const Mapping& mapping, const std::vector<float>& base, int band, const eqmodel::Band& wanted, juce::StringArray& missing)
{
    std::vector<float> setting = base;
    std::vector<bool> set(setting.size(), false);
    std::vector<const MappedParameter*> used;
    auto assign = [&](const MappedParameter& parameter, double normalised) {
        if (parameter.index >= 0 && static_cast<size_t>(parameter.index) < setting.size())
        {
            setting[static_cast<size_t>(parameter.index)] = static_cast<float>(normalised);
            set[static_cast<size_t>(parameter.index)] = true;
            used.push_back(&parameter);
        }
    };
    auto setLaw = [&](MappedQuantity quantity, double value, bool needed) {
        const MappedParameter* parameter = mapping.find(band, quantity);
        if (parameter == nullptr)
        {
            if (needed)
            {
                missing.add(juce::String("no parameter for the ") + getMappedQuantityName(quantity));
            }
            return;
        }
        const std::optional<KnobLaw>& law = parameter->measuredLaw ? parameter->measuredLaw : parameter->displayedLaw;
        if (! law)
        {
            missing.add(parameter->name + ": no law");
            return;
        }
        assign(*parameter, law->toNormalised(value));
    };
    // the step that matches; of several, the one whose text names what is wanted (a notch is also measured as a deep peak, "Peak" is the step)
    auto setStep = [&](MappedQuantity quantity, auto matches, const juce::String& what) {
        const MappedParameter* parameter = mapping.find(band, quantity);
        if (parameter == nullptr)
        {
            return false;
        }
        const SwitchStep* chosen = nullptr;
        for (const SwitchStep& step : parameter->steps)
        {
            if (matches(step))
            {
                const bool named = step.text.containsIgnoreCase(what);
                if (chosen == nullptr || (named && ! chosen->text.containsIgnoreCase(what)))
                {
                    chosen = &step;
                }
            }
        }
        if (chosen != nullptr)
        {
            assign(*parameter, chosen->value);
            return true;
        }
        missing.add(parameter->name + ": no step for " + what);
        return false;
    };

    setStep(MappedQuantity::BandOnOff, [](const SwitchStep& step) { return step.bandPresent; }, "the band on");
    setStep(MappedQuantity::Type, [&](const SwitchStep& step) { return step.type && *step.type == wanted.type; }, reference::getFilterTypeName(wanted.type));
    setStep(MappedQuantity::Family, [&](const SwitchStep& step) { return step.family && sameFamilyForSwitch(*step.family, wanted.family); },
            eqmodel::getFamilyName(wanted.family));
    setLaw(MappedQuantity::Frequency, wanted.frequencyHz, true);
    const bool passFilter = ! hasGain(wanted.type);
    if (! passFilter)
    {
        setLaw(MappedQuantity::Gain, wanted.gainDb, true);
    }
    const bool ordered = wanted.family == eqmodel::DesignFamily::Butterworth || wanted.family == eqmodel::DesignFamily::LinkwitzRiley;
    if (ordered)
    {
        setLaw(MappedQuantity::Order, wanted.order, true);
    }
    else
    {
        setLaw(MappedQuantity::Q, wanted.q, true);
    }
    // what the parameters need to act: other parameters' values, unless the mapping set those itself
    for (const MappedParameter* parameter : used)
    {
        for (const Requirement& requirement : parameter->requirements)
        {
            const auto index = static_cast<size_t>(requirement.parameter);
            if (index < setting.size() && ! set[index])
            {
                setting[index] = requirement.value;
            }
        }
    }
    // a family switch without a step for the wanted family is fine if a requirement chose it (an order that acts only with Butterworth)
    for (int index = missing.size(); --index >= 0;)
    {
        const MappedParameter* family = mapping.find(band, MappedQuantity::Family);
        if (family != nullptr && missing[index].startsWith(family->name + ": no step"))
        {
            for (const MappedParameter* parameter : used)
            {
                for (const Requirement& requirement : parameter->requirements)
                {
                    if (requirement.parameter == family->index)
                    {
                        missing.remove(index);
                        break;
                    }
                }
            }
        }
    }
    return setting;
}

juce::var Mapping::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty("schema", kSchema);
    auto* plugin = new juce::DynamicObject();
    plugin->setProperty("name", pluginName);
    plugin->setProperty("manufacturer", manufacturer);
    plugin->setProperty("version", version);
    plugin->setProperty("format", format);
    plugin->setProperty("identifier", identifier);
    root->setProperty("plugin", juce::var(plugin));
    juce::Array<juce::var> rates;
    for (const double rate : sampleRates)
    {
        rates.add(rate);
    }
    root->setProperty("sampleRates", rates);
    root->setProperty("date", date);
    root->setProperty("proposedBy", proposedBy);
    root->setProperty("confirmedBy", confirmedBy);
    root->setProperty("notes", notes);
    root->setProperty("bands", bands);
    juce::Array<juce::var> list;
    for (const MappedParameter& parameter : parameters)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("index", parameter.index);
        object->setProperty("name", parameter.name);
        object->setProperty("role", parameter.role);
        object->setProperty("quantity", getMappedQuantityName(parameter.quantity));
        object->setProperty("band", parameter.band);
        object->setProperty("default", parameter.defaultValue);
        if (! parameter.requirements.empty())
        {
            juce::Array<juce::var> requirements;
            for (const Requirement& requirement : parameter.requirements)
            {
                requirements.add(requirementToVar(requirement));
            }
            object->setProperty("requires", requirements);
        }
        if (parameter.displayedLaw)
        {
            object->setProperty("displayedLaw", parameter.displayedLaw->toVar());
        }
        if (parameter.measuredLaw)
        {
            object->setProperty("measuredLaw", parameter.measuredLaw->toVar());
        }
        if (! parameter.steps.empty())
        {
            juce::Array<juce::var> steps;
            for (const SwitchStep& step : parameter.steps)
            {
                auto* stepObject = new juce::DynamicObject();
                stepObject->setProperty("value", step.value);
                stepObject->setProperty("text", step.text);
                stepObject->setProperty("description", step.description);
                if (step.type)
                {
                    stepObject->setProperty("type", reference::getFilterTypeName(*step.type));
                }
                if (step.family)
                {
                    stepObject->setProperty("family", eqmodel::getFamilyName(*step.family));
                }
                stepObject->setProperty("band", step.bandPresent);
                steps.add(juce::var(stepObject));
            }
            object->setProperty("steps", steps);
        }
        list.add(juce::var(object));
    }
    root->setProperty("parameters", list);
    return juce::var(root);
}

std::optional<Mapping> Mapping::fromVar(const juce::var& value, juce::String& error)
{
    if (! value.isObject() || static_cast<int>(value["schema"]) != kSchema)
    {
        error = "not a mapping of schema " + juce::String(kSchema);
        return std::nullopt;
    }
    Mapping mapping;
    const juce::var plugin = value["plugin"];
    mapping.pluginName = plugin["name"].toString();
    mapping.manufacturer = plugin["manufacturer"].toString();
    mapping.version = plugin["version"].toString();
    mapping.format = plugin["format"].toString();
    mapping.identifier = plugin["identifier"].toString();
    if (const juce::Array<juce::var>* rates = value["sampleRates"].getArray())
    {
        for (const juce::var& rate : *rates)
        {
            mapping.sampleRates.push_back(static_cast<double>(rate));
        }
    }
    mapping.date = value["date"].toString();
    mapping.proposedBy = value["proposedBy"].toString();
    mapping.confirmedBy = value["confirmedBy"].toString();
    mapping.notes = value["notes"].toString();
    mapping.bands = static_cast<int>(value["bands"]);
    const juce::Array<juce::var>* list = value["parameters"].getArray();
    if (list == nullptr)
    {
        error = "no parameters";
        return std::nullopt;
    }
    for (const juce::var& item : *list)
    {
        MappedParameter parameter;
        parameter.index = static_cast<int>(item["index"]);
        parameter.name = item["name"].toString();
        parameter.role = item["role"].toString();
        const std::optional<MappedQuantity> quantity = parseQuantity(item["quantity"].toString());
        if (! quantity)
        {
            error = parameter.name + ": unknown quantity " + item["quantity"].toString();
            return std::nullopt;
        }
        parameter.quantity = *quantity;
        parameter.band = static_cast<int>(item.getProperty("band", -1));
        parameter.defaultValue = static_cast<float>(static_cast<double>(item["default"]));
        if (const juce::Array<juce::var>* requirements = item["requires"].getArray())
        {
            for (const juce::var& entry : *requirements)
            {
                Requirement requirement;
                requirement.parameter = static_cast<int>(entry["parameter"]);
                requirement.name = entry["name"].toString();
                requirement.value = static_cast<float>(static_cast<double>(entry["value"]));
                requirement.text = entry["text"].toString();
                parameter.requirements.push_back(requirement);
            }
        }
        parameter.displayedLaw = KnobLaw::fromVar(item["displayedLaw"]);
        parameter.measuredLaw = KnobLaw::fromVar(item["measuredLaw"]);
        if (const juce::Array<juce::var>* steps = item["steps"].getArray())
        {
            for (const juce::var& entry : *steps)
            {
                SwitchStep step;
                step.value = static_cast<float>(static_cast<double>(entry["value"]));
                step.text = entry["text"].toString();
                step.description = entry["description"].toString();
                step.type = parseType(entry["type"].toString());
                step.family = parseFamily(entry["family"].toString());
                step.bandPresent = static_cast<bool>(entry["band"]);
                parameter.steps.push_back(step);
            }
        }
        mapping.parameters.push_back(parameter);
    }
    return mapping;
}

bool Mapping::write(const juce::File& file) const
{
    file.getParentDirectory().createDirectory();
    return file.replaceWithText(juce::JSON::toString(toVar()) + "\n");
}

std::optional<Mapping> Mapping::read(const juce::File& file, juce::String& error)
{
    if (! file.existsAsFile())
    {
        error = "no file " + file.getFullPathName();
        return std::nullopt;
    }
    juce::var value;
    const juce::Result result = juce::JSON::parse(file.loadFileAsString(), value);
    if (result.failed())
    {
        error = result.getErrorMessage();
        return std::nullopt;
    }
    return fromVar(value, error);
}

juce::String createMappingReport(const Mapping& mapping)
{
    juce::String text;
    text << "## Mapping: " << mapping.pluginName << " " << mapping.version << "\n\n";
    text << (mapping.confirmedBy.isEmpty() ? juce::String("**Proposal** (not confirmed)") : "Confirmed by " + mapping.confirmedBy) << "; proposed "
         << mapping.date << " by " << mapping.proposedBy << "; " << mapping.bands << (mapping.bands == 1 ? " band" : " bands") << ".\n\n";
    text << "| # | Parameter | Band | Quantity | Law as shown | Law as measured | Needs |\n|---|---|---|---|---|---|---|\n";
    for (const MappedParameter& parameter : mapping.parameters)
    {
        const juce::String unit = getMappedQuantityUnit(parameter.quantity);
        juce::String measured = parameter.measuredLaw ? parameter.measuredLaw->describe(unit) : juce::String("-");
        const bool describedSteps = std::any_of(parameter.steps.begin(), parameter.steps.end(), [](const SwitchStep& step) { return step.description.isNotEmpty(); });
        if (describedSteps && (! parameter.measuredLaw || parameter.quantity == MappedQuantity::Type || parameter.quantity == MappedQuantity::Family
                               || parameter.quantity == MappedQuantity::BandOnOff))
        {
            juce::StringArray steps;
            for (const SwitchStep& step : parameter.steps)
            {
                steps.add(step.text + ": " + (step.description.isNotEmpty() ? step.description : juce::String("?")));
            }
            measured = steps.joinIntoString("; ");
        }
        juce::StringArray needs;
        for (const Requirement& requirement : parameter.requirements)
        {
            needs.add(requirement.name + " = " + requirement.text);
        }
        text << "| " << (parameter.index + 1) << " | " << parameter.name << " | " << (parameter.band >= 0 ? juce::String(parameter.band + 1) : juce::String("-"))
             << " | " << (parameter.quantity == MappedQuantity::Other ? parameter.role : juce::String(getMappedQuantityName(parameter.quantity))) << " | "
             << (parameter.displayedLaw ? parameter.displayedLaw->describe(unit) : juce::String("-")) << " | " << measured << " | "
             << (needs.isEmpty() ? juce::String("-") : needs.joinIntoString(", ")) << " |\n";
    }
    return text;
}

juce::String getMappingFileName(const juce::String& pluginName)
{
    juce::String name;
    for (const juce::juce_wchar c : pluginName.toLowerCase())
    {
        name << (juce::CharacterFunctions::isLetterOrDigit(c) ? juce::String::charToString(c) : juce::String("_"));
    }
    while (name.contains("__"))
    {
        name = name.replace("__", "_");
    }
    return name.trimCharactersAtStart("_").trimCharactersAtEnd("_") + ".json";
}

juce::StringArray compareMappings(const Mapping& proposal, const Mapping& confirmed)
{
    juce::StringArray differences;
    if (proposal.parameters.size() != confirmed.parameters.size())
    {
        differences.add("parameters: " + juce::String(static_cast<int>(proposal.parameters.size())) + " instead of "
                        + juce::String(static_cast<int>(confirmed.parameters.size())));
        return differences;
    }
    if (proposal.bands != confirmed.bands)
    {
        differences.add("bands: " + juce::String(proposal.bands) + " instead of " + juce::String(confirmed.bands));
    }
    auto compareLaw = [&](const juce::String& what, const std::optional<KnobLaw>& a, const std::optional<KnobLaw>& b) {
        if (a.has_value() != b.has_value())
        {
            differences.add(what + ": " + (a ? "present" : "missing"));
            return;
        }
        if (! a)
        {
            return;
        }
        if (a->kind != b->kind)
        {
            differences.add(what + ": " + getLawKindName(a->kind) + " instead of " + getLawKindName(b->kind));
            return;
        }
        if (a->kind == LawKind::Table)
        {
            if (a->table.size() != b->table.size())
            {
                differences.add(what + ": " + juce::String(static_cast<int>(a->table.size())) + " steps instead of " + juce::String(static_cast<int>(b->table.size())));
                return;
            }
            for (size_t index = 0; index < a->table.size(); ++index)
            {
                const double scale = std::max({std::abs(b->table.front()), std::abs(b->table.back()), 1.0});
                if (std::abs(a->table[index] - b->table[index]) > kLawRangeTolerance * scale)
                {
                    differences.add(what + ": step " + juce::String(static_cast<int>(index)) + " " + juce::String(a->table[index]) + " instead of " + juce::String(b->table[index]));
                }
            }
            return;
        }
        const double range = std::max(std::abs(b->maximum - b->minimum), 1.0e-9);
        if (std::abs(a->minimum - b->minimum) > kLawRangeTolerance * range || std::abs(a->maximum - b->maximum) > kLawRangeTolerance * range)
        {
            differences.add(what + ": " + a->describe({}) + " instead of " + b->describe({}));
        }
        else if (a->kind == LawKind::Power && std::abs(std::log(a->skew / b->skew)) > kSkewTolerance)
        {
            differences.add(what + ": skew " + juce::String(a->skew, 3) + " instead of " + juce::String(b->skew, 3));
        }
    };
    for (size_t index = 0; index < proposal.parameters.size(); ++index)
    {
        const MappedParameter& a = proposal.parameters[index];
        const MappedParameter& b = confirmed.parameters[index];
        const juce::String name = a.name;
        if (a.name != b.name)
        {
            differences.add(juce::String(static_cast<int>(index) + 1) + ": name " + a.name + " instead of " + b.name);
            continue;
        }
        if (a.quantity != b.quantity)
        {
            differences.add(name + ": " + getMappedQuantityName(a.quantity) + " instead of " + getMappedQuantityName(b.quantity));
        }
        if (a.band != b.band)
        {
            differences.add(name + ": band " + juce::String(a.band) + " instead of " + juce::String(b.band));
        }
        juce::StringArray needsA;
        juce::StringArray needsB;
        for (const Requirement& requirement : a.requirements)
        {
            needsA.add(requirement.name + "=" + juce::String(requirement.value, 3));
        }
        for (const Requirement& requirement : b.requirements)
        {
            needsB.add(requirement.name + "=" + juce::String(requirement.value, 3));
        }
        if (needsA != needsB)
        {
            differences.add(name + ": needs " + needsA.joinIntoString(", ") + " instead of " + needsB.joinIntoString(", "));
        }
        compareLaw(name + " (as shown)", a.displayedLaw, b.displayedLaw);
        compareLaw(name + " (as measured)", a.measuredLaw, b.measuredLaw);
        if (a.steps.size() != b.steps.size())
        {
            differences.add(name + ": " + juce::String(static_cast<int>(a.steps.size())) + " steps instead of " + juce::String(static_cast<int>(b.steps.size())));
        }
        else
        {
            for (size_t step = 0; step < a.steps.size(); ++step)
            {
                if (a.steps[step].type != b.steps[step].type || a.steps[step].family != b.steps[step].family || a.steps[step].bandPresent != b.steps[step].bandPresent)
                {
                    differences.add(name + ", step " + a.steps[step].text + ": " + a.steps[step].description + " instead of " + b.steps[step].description);
                }
            }
        }
    }
    return differences;
}
}
