#include "pluginlab/engine/PluginMatch.h"

#include <algorithm>
#include <cmath>
#include <memory>

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
        match::ParameterSpace space;
        space.base = start;
        for (const int index : target.released)
        {
            if (index < 0 || index >= static_cast<int>(start.size()))
            {
                continue;
            }
            juce::String name = "parameter " + juce::String(index + 1);
            if (instance != nullptr && index < instance->getParameters().size())
            {
                name = instance->getParameters()[index]->getName(kTextLength);
            }
            space.released.push_back({index, name, 0.0, 1.0, start[static_cast<size_t>(index)]});
        }
        match::MatchProblem problem(result.masterFeatures, [&live](const std::vector<float>& setting) { return live.makeDevice(setting); }, probe,
                                    match::makeMagnitudeObjective(result.frequencies.size(), settings.tolerance), space);
        item.optimizer = problem.solve(settings.budget, {}, [&report](double fraction, const juce::String& step) { report(fraction, step); });
        item.setting = space.makeSetting(item.optimizer.values);
        item.targetFeatures = problem.getFeatures(item.setting);
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
    return wrapMarkdownProse(text);
}
}
