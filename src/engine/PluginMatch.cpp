#include "pluginlab/engine/PluginMatch.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "pluginlab/engine/Interactions.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/measure/FrequencyResponse.h"

namespace pluginlab::engine
{
namespace
{
constexpr int kMasterFitBands = 6;
constexpr int kTextLength = 64;
constexpr double kSignificantGainDb = 0.5; // a fitted gain band with less counts as noise of the fit

double getWorstFrequency(const std::vector<double>& residual, const std::vector<double>& frequencies)
{
    size_t worst = 0;
    for (size_t index = 1; index < residual.size() && index < frequencies.size(); ++index)
    {
        if (std::abs(residual[index]) > std::abs(residual[worst]))
        {
            worst = index;
        }
    }
    return frequencies.empty() ? 0.0 : frequencies[std::min(worst, frequencies.size() - 1)];
}

bool isGainType(reference::FilterType type)
{
    return type == reference::FilterType::Peak || type == reference::FilterType::LowShelf || type == reference::FilterType::HighShelf;
}

// the bands that matter: a gain band with at least kSignificantGainDb, any other band
std::vector<eqmodel::Band> getSignificantBands(const eqmodel::EqModel& model)
{
    std::vector<eqmodel::Band> bands;
    for (const eqmodel::Band& band : model.bands)
    {
        if (! isGainType(band.type) || std::abs(band.gainDb) >= kSignificantGainDb)
        {
            bands.push_back(band);
        }
    }
    return bands;
}

// families whose magnitudes are the same for the same numbers count as one (the state-variable filter and the linear-phase band have the RBJ
// magnitude; a Butterworth pass filter of order 2 is RBJ's Q 0.71)
juce::String getFamilyGroup(const eqmodel::Band& band)
{
    using Family = eqmodel::DesignFamily;
    if (band.family == Family::StateVariable || band.family == Family::LinearPhase || band.family == Family::Rbj)
    {
        return isGainType(band.type) ? "RBJ" : "pass";
    }
    if (band.family == Family::Butterworth || band.family == Family::LinkwitzRiley)
    {
        return "pass";
    }
    return eqmodel::getFamilyName(band.family);
}

juce::String describeBand(const eqmodel::Band& band)
{
    juce::String text = juce::String(eqmodel::getFamilyName(band.family)) + " " + reference::getFilterTypeName(band.type) + " " + formatFrequency(band.frequencyHz) + " Hz";
    if (isGainType(band.type))
    {
        text << " " << (band.gainDb >= 0.0 ? "+" : "") << juce::String(band.gainDb, 1) << " dB";
    }
    return text;
}

// the causes of a residual, in the order of the plan (4.4); NotFound if none applies
std::vector<MatchAttribution> attribute(juce::AudioPluginFormatManager& formatManager, const MatchTarget& target, const TargetMatchResult& item,
                                        const eqmodel::EqModel& masterModel, const std::vector<double>& grid, double sampleRate)
{
    std::vector<MatchAttribution> causes;
    // 1. a knob at the end of its range
    for (const ReleasedValue& value : item.values)
    {
        if (value.atBound)
        {
            causes.push_back({MatchCause::KnobRange, value.name + " at the end of its range (" + (value.text.isNotEmpty() ? value.text : juce::String(value.value, 3)) + ")"});
        }
    }
    // the target's bands at the found setting: the band fit of a careful measurement
    measure::SweepResponseSettings sweep;
    sweep.sampleRate = sampleRate;
    sweep.channels = 1;
    sweep.frequencies = grid;
    const measure::FrequencyResponse response = measure::measureSweptResponse(makePluginDevice(formatManager, target.plugin, item.setting), sweep);
    eqmodel::EqModel targetModel;
    if (! response.channels.empty())
    {
        eqmodel::FitSettings fit;
        fit.sampleRate = sampleRate;
        fit.maximumBands = kMasterFitBands;
        targetModel = eqmodel::fitEqModel(grid, response.channels[0].response, fit).model;
    }
    const std::vector<eqmodel::Band> masterBands = getSignificantBands(masterModel);
    const std::vector<eqmodel::Band> targetBands = getSignificantBands(targetModel);
    // 2. the target's bands do not add (W8.7), if its mapping has several bands: measured, so it comes before what the band fits suggest
    //    (a fit describes the response of interacting bands with another family)
    if (target.mapping && target.mapping->bands >= 2)
    {
        const InteractionResult interactions = measureInteractions(makePluginSubject(formatManager, target.plugin, sampleRate), *target.mapping);
        if (interactions.measured && ! interactions.additive)
        {
            causes.push_back({MatchCause::Interaction, interactions.conclusion});
        }
    }
    // 3. another design family: the master's band and the target's nearest band (within an octave) of different families
    for (const eqmodel::Band& band : masterBands)
    {
        const eqmodel::Band* nearest = nullptr;
        for (const eqmodel::Band& candidate : targetBands)
        {
            if (nearest == nullptr || std::abs(std::log2(candidate.frequencyHz / band.frequencyHz)) < std::abs(std::log2(nearest->frequencyHz / band.frequencyHz)))
            {
                nearest = &candidate;
            }
        }
        if (nearest != nullptr && std::abs(std::log2(nearest->frequencyHz / band.frequencyHz)) < 1.0 && getFamilyGroup(*nearest) != getFamilyGroup(band))
        {
            causes.push_back({MatchCause::DesignFamily, "the master's band is " + describeBand(band) + ", the target's " + describeBand(*nearest)
                                                            + "; the designs differ most at " + formatFrequency(item.worstHz) + " Hz"});
        }
    }
    // 4. a band missing: the master has more bands than the target makes with the released parameters
    if (masterBands.size() > targetBands.size())
    {
        causes.push_back({MatchCause::BandMissing, "the master has " + juce::String(static_cast<int>(masterBands.size())) + " bands, the target makes "
                                                       + juce::String(static_cast<int>(targetBands.size())) + " with the released parameters"});
    }
    if (causes.empty())
    {
        causes.push_back({MatchCause::NotFound, "no knob at the end of its range, the same design family, enough bands"});
    }
    return causes;
}

// the bands of a plugin master: the band fit (W8.3) of its response, measured carefully
eqmodel::EqModel fitMaster(juce::AudioPluginFormatManager& formatManager, const MatchMaster& master, const std::vector<double>& grid, double sampleRate)
{
    if (master.curve)
    {
        return *master.curve;
    }
    measure::SweepResponseSettings sweep;
    sweep.sampleRate = sampleRate;
    sweep.channels = 1;
    sweep.frequencies = grid;
    const measure::FrequencyResponse response = measure::measureSweptResponse(makePluginDevice(formatManager, *master.plugin, master.setting), sweep);
    if (response.channels.empty())
    {
        return {};
    }
    eqmodel::FitSettings fit;
    fit.sampleRate = sampleRate;
    fit.maximumBands = kMasterFitBands;
    return eqmodel::fitEqModel(grid, response.channels[0].response, fit).model;
}
}

const char* getMatchCauseName(MatchCause cause)
{
    switch (cause)
    {
        case MatchCause::KnobRange:
            return "a knob at the end of its range";
        case MatchCause::DesignFamily:
            return "another design family";
        case MatchCause::BandMissing:
            return "a band missing";
        case MatchCause::Interaction:
            return "the bands interact";
        case MatchCause::NotFound:
            return "not found";
    }
    return "";
}

juce::String MatchMaster::describe() const
{
    if (curve)
    {
        juce::StringArray bands;
        for (const eqmodel::Band& band : curve->bands)
        {
            juce::String text = juce::String(eqmodel::getFamilyName(band.family)) + " " + reference::getFilterTypeName(band.type) + " "
                                + formatFrequency(band.frequencyHz) + " Hz";
            if (band.type == reference::FilterType::Peak || band.type == reference::FilterType::LowShelf || band.type == reference::FilterType::HighShelf)
            {
                text << " " << (band.gainDb >= 0.0 ? "+" : "") << juce::String(band.gainDb, 1) << " dB";
            }
            text << " Q " << juce::String(band.q, 2);
            bands.add(text);
        }
        return "band-model curve: " + bands.joinIntoString(", ");
    }
    return plugin ? plugin->name + " " + plugin->version + " at its setting" : juce::String("none");
}

PluginMatchSettings::PluginMatchSettings()
{
    probe.stimulus = match::ProbeStimulus::Impulse;
}

std::vector<float> makeModelStart(const Mapping& mapping, const std::vector<float>& base, const eqmodel::EqModel& masterBands)
{
    std::vector<eqmodel::Band> bands = masterBands.bands;
    std::sort(bands.begin(), bands.end(), [](const eqmodel::Band& a, const eqmodel::Band& b) { return a.frequencyHz < b.frequencyHz; });
    std::vector<float> setting = base;
    for (int band = 0; band < static_cast<int>(bands.size()) && band < mapping.bands; ++band)
    {
        juce::StringArray missing;
        setting = makeSettingForBand(mapping, setting, band, bands[static_cast<size_t>(band)], missing);
    }
    return setting;
}

PluginMatchResult matchPlugins(juce::AudioPluginFormatManager& formatManager, const MatchMaster& master, const std::vector<MatchTarget>& targets,
                               const PluginMatchSettings& settings, const PluginMatchProgress& progress)
{
    PluginMatchResult result;
    result.master = master.describe();
    result.tolerance = settings.tolerance;
    const auto probe = std::make_shared<match::MagnitudeProbe>(settings.probe);
    result.frequencies = probe->getFrequencies();
    const double sampleRate = settings.probe.sampleRate;
    const auto features = [probe](const measure::Device& device) { return probe->measure(device); };

    // the master's features: exact for a curve; through a live instance for a plugin (it must be LTI too)
    if (master.curve)
    {
        result.masterFeatures = match::getCurveMagnitude(*master.curve, result.frequencies, sampleRate, settings.probe.floorDb);
    }
    else if (master.plugin)
    {
        if (progress)
        {
            progress(0.0, "master: " + master.plugin->name);
        }
        const LivePluginTarget live(formatManager, *master.plugin, sampleRate, settings.live);
        const LiveTargetCheck check = checkLiveTarget(formatManager, *master.plugin, live, master.setting, features);
        if (! check.usable)
        {
            result.error = "the master cannot be matched against: " + check.reason;
            return result;
        }
        result.masterFeatures = features(live.makeDevice(master.setting));
    }
    else
    {
        result.error = "no master";
        return result;
    }

    std::optional<eqmodel::EqModel> masterBands; // for the model start: computed once, when a target asks for it
    for (size_t targetIndex = 0; targetIndex < targets.size(); ++targetIndex)
    {
        const MatchTarget& target = targets[targetIndex];
        const double started = juce::Time::getMillisecondCounterHiRes();
        TargetMatchResult item;
        item.name = target.plugin.name + " " + target.plugin.version;
        const auto report = [&](double fraction, const juce::String& step) {
            if (progress)
            {
                progress((static_cast<double>(targetIndex) + fraction) / static_cast<double>(targets.size()), item.name + ": " + step);
            }
        };
        report(0.0, "checks");
        std::vector<float> start = target.start.empty() ? getDefaultSetting(formatManager, target.plugin, sampleRate) : target.start;
        if (target.modelStart && target.mapping)
        {
            if (! masterBands)
            {
                masterBands = fitMaster(formatManager, master, result.frequencies, sampleRate);
            }
            start = makeModelStart(*target.mapping, start, *masterBands);
            item.startedFromModel = true;
        }
        const LivePluginTarget live(formatManager, target.plugin, sampleRate, settings.live);
        const LiveTargetCheck check = checkLiveTarget(formatManager, target.plugin, live, start, features);
        if (! check.usable)
        {
            item.refused = true;
            item.reason = check.reason;
            item.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
            result.targets.push_back(item);
            continue;
        }
        // the parameter names and texts from an instance of the target
        juce::String error;
        const std::unique_ptr<juce::AudioPluginInstance> instance = formatManager.createPluginInstance(target.plugin, sampleRate, settings.live.blockSize, error);
        const auto solveFrom = [&](const std::vector<float>& from) {
            match::ParameterSpace space;
            space.base = from;
            for (const int index : target.released)
            {
                if (index < 0 || index >= static_cast<int>(from.size()))
                {
                    continue;
                }
                juce::String name = "parameter " + juce::String(index + 1);
                if (instance != nullptr && index < instance->getParameters().size())
                {
                    name = instance->getParameters()[index]->getName(kTextLength);
                }
                space.released.push_back({index, name, 0.0, 1.0, from[static_cast<size_t>(index)]});
            }
            match::MatchProblem problem(result.masterFeatures, [&live](const std::vector<float>& setting) { return live.makeDevice(setting); }, probe,
                                        match::makeMagnitudeObjective(result.frequencies.size(), settings.tolerance), space);
            item.optimizer = problem.solve(settings.budget, {}, [&report](double fraction, const juce::String& step) { report(fraction, step); });
            item.setting = space.makeSetting(item.optimizer.values);
            item.targetFeatures = problem.getFeatures(item.setting);
            item.values.clear();
            for (size_t index = 0; index < space.released.size(); ++index)
            {
                ReleasedValue value;
                value.index = space.released[index].index;
                value.name = space.released[index].name;
                value.value = item.setting[static_cast<size_t>(value.index)];
                value.atBound = index < item.optimizer.atBound.size() && item.optimizer.atBound[index];
                if (instance != nullptr && value.index < instance->getParameters().size())
                {
                    value.text = tidyNumberText(instance->getParameters()[value.index]->getText(value.value, kTextLength));
                }
                item.values.push_back(value);
            }
            item.worstHz = getWorstFrequency(item.optimizer.residual, result.frequencies);
        };
        solveFrom(start);
        if (! item.optimizer.withinTolerance)
        {
            // why not closer (W9.5); if nothing explains it and a mapping exists, one more try from the model start
            if (! masterBands)
            {
                masterBands = fitMaster(formatManager, master, result.frequencies, sampleRate);
            }
            report(0.95, "why not closer");
            item.causes = attribute(formatManager, target, item, *masterBands, result.frequencies, sampleRate);
            if (item.causes.size() == 1 && item.causes.front().cause == MatchCause::NotFound && target.mapping && ! item.startedFromModel)
            {
                const TargetMatchResult first = item;
                solveFrom(makeModelStart(*target.mapping, start, *masterBands));
                if (item.optimizer.withinTolerance)
                {
                    item.startedFromModel = true;
                    item.causes.clear();
                }
                else
                {
                    item = first; // the first result stays; the second start did not help
                    item.causes.front().evidence += "; a second start from the model did not help either";
                }
            }
        }
        item.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
        result.targets.push_back(item);
    }
    if (progress)
    {
        progress(1.0, "done");
    }
    return result;
}

juce::String createPluginMatchReport(const PluginMatchResult& result)
{
    juce::String text;
    text << "## Match\n\n";
    text << "Master: " << result.master << ". Each target's released parameters were moved until its magnitude response equals the master's "
         << "(tolerance " << match::getToleranceName(result.tolerance) << ", " << juce::roundToInt(result.frequencies.front()) << " Hz ... "
         << juce::roundToInt(result.frequencies.back()) << " Hz; W9).\n\n";
    if (result.error.isNotEmpty())
    {
        text << "**" << result.error << "**\n";
        return wrapMarkdownProse(text);
    }
    text << "| Target | Result | Largest / rms deviation | Found setting | Renders, time |\n|---|---|---|---|---|\n";
    for (const TargetMatchResult& item : result.targets)
    {
        if (item.refused)
        {
            text << "| " << item.name << " | **refused** | - | " << item.reason << " | " << juce::String(item.seconds, 1) << " s |\n";
            continue;
        }
        juce::StringArray values;
        for (const ReleasedValue& value : item.values)
        {
            values.add(value.name + " = " + (value.text.isNotEmpty() ? value.text : juce::String(value.value, 3)) + (value.atBound ? " (at the end of its range)" : ""));
        }
        const juce::String verdict = item.optimizer.withinTolerance ? juce::String("matched") : "**not within the tolerance** (" + juce::String(match::getStopReasonName(item.optimizer.reason)) + ")";
        text << "| " << item.name << (item.startedFromModel ? " (model start)" : "") << " | " << verdict << " | " << juce::String(item.optimizer.largest, 3) << " / "
             << juce::String(item.optimizer.rms, 3) << " dB | " << values.joinIntoString(", ") << " | " << item.optimizer.evaluations << ", "
             << juce::String(item.seconds, 1) << " s |\n";
    }
    juce::String why;
    for (const TargetMatchResult& item : result.targets)
    {
        for (const MatchAttribution& cause : item.causes)
        {
            why << "| " << item.name << " | " << getMatchCauseName(cause.cause) << " | " << cause.evidence << " |\n";
        }
    }
    if (why.isNotEmpty())
    {
        text << "\n### Why not closer\n\nThe causes in the order they are tested (W9.5); the first is the main one.\n\n| Target | Cause | Evidence |\n|---|---|---|\n" << why;
    }
    return wrapMarkdownProse(text);
}
}
