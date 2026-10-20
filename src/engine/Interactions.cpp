#include "pluginlab/engine/Interactions.h"

#include <algorithm>
#include <cmath>
#include <map>

#include "pluginlab/engine/ReportText.h"
#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/measure/FrequencyResponse.h"

namespace pluginlab::engine
{
namespace
{
constexpr double kLowestHz = 20.0;
constexpr double kHighestHz = 20000.0;
constexpr double kGridPerOctave = 12.0;
constexpr double kFloor = 1.0e-15;

double toDb(std::complex<double> value)
{
    return 20.0 * std::log10(std::max(std::abs(value), kFloor));
}

// "+" before a positive number or zero; a negative number brings its own sign
const char* getSignPrefix(double value)
{
    if (value >= 0.0)
    {
        return "+";
    }
    return "";
}

juce::String formatGain(double gainDb)
{
    return getSignPrefix(gainDb) + juce::String(gainDb, 1) + " dB";
}

juce::String formatDistance(double octaves)
{
    if (std::abs(octaves) < 1.0e-9)
    {
        return "0 (same frequency)";
    }
    if (std::abs(octaves - 1.0 / 3.0) < 1.0e-6)
    {
        return "1/3 octave";
    }
    const char* unitName;
    if (std::abs(octaves - 1.0) < 1.0e-9)
    {
        unitName = " octave";
    }
    else
    {
        unitName = " octaves";
    }
    return juce::String(octaves, 0) + unitName;
}

bool canSetBand(const Mapping& mapping, int band)
{
    return mapping.find(band, MappedQuantity::Frequency) != nullptr && mapping.find(band, MappedQuantity::Gain) != nullptr;
}

// The frequency of the largest deviation, only for a pair that interacts
juce::String formatInteractionFrequency(const InteractionCase& item)
{
    if (item.interacts)
    {
        return formatFrequency(item.atHz) + " Hz";
    }
    return juce::String("-");
}

juce::String formatVerdict(const InteractionCase& item)
{
    if (item.interacts)
    {
        return juce::String("**interacts**");
    }
    return juce::String("adds");
}

// The note of a pair in brackets, nothing without a note
juce::String formatNote(const juce::String& note)
{
    if (note.isNotEmpty())
    {
        return " (" + note + ")";
    }
    return juce::String();
}
}

InteractionResult measureInteractions(const SurveySubject& subject, const Mapping& mapping, const InteractionSettings& settings,
                                      const InteractionProgress& progress)
{
    const double started = juce::Time::getMillisecondCounterHiRes();
    InteractionResult result;
    result.pluginName = mapping.pluginName;
    result.settings = settings;
    result.bands = mapping.bands;
    std::vector<int> bands;
    for (int band = 0; band < mapping.bands; ++band)
    {
        if (canSetBand(mapping, band))
        {
            bands.push_back(band);
        }
    }
    if (bands.size() < 2)
    {
        if (bands.empty())
        {
            result.conclusion = "no band with a frequency and a gain in the mapping: nothing to combine";
        }
        else
        {
            result.conclusion = "one band with a frequency and a gain: nothing to combine";
        }
        return result;
    }
    result.measured = true;
    std::vector<float> base;
    for (const SurveyParameter& parameter : subject.parameters)
    {
        base.push_back(parameter.defaultValue);
    }
    const std::vector<double> grid = eqmodel::makeLogGrid(kLowestHz, std::min(kHighestHz, 0.45 * settings.sampleRate), kGridPerOctave);
    std::map<std::vector<float>, std::vector<std::complex<double>>> cache;
    const auto measure = [&](const std::vector<float>& setting) -> const std::vector<std::complex<double>>& {
        const auto found = cache.find(setting);
        if (found != cache.end())
        {
            return found->second;
        }
        measure::SweepResponseSettings sweep;
        sweep.sampleRate = settings.sampleRate;
        sweep.channels = 1;
        sweep.levelDbfs = settings.levelDbfs;
        sweep.approximateSeconds = settings.sweepSeconds;
        sweep.frequencies = grid;
        const measure::FrequencyResponse response = measure::measureSweptResponse(subject.makeDevice(setting), sweep);
        ++result.renders;
        std::vector<std::complex<double>> values;
        if (response.channels.empty())
        {
            values = std::vector<std::complex<double>>(grid.size(), 1.0);
        }
        else
        {
            values = response.channels[0].response;
        }
        return cache.emplace(setting, std::move(values)).first->second;
    };
    const auto makePeak = [&settings](double frequency, double gainDb) {
        eqmodel::Band band;
        band.family = eqmodel::DesignFamily::Rbj;
        band.type = reference::FilterType::Peak;
        band.frequencyHz = frequency;
        band.gainDb = gainDb;
        band.q = settings.q;
        return band;
    };
    int pairs = 0;
    for (size_t i = 0; i < bands.size(); ++i)
    {
        for (size_t j = i + 1; j < bands.size(); ++j)
        {
            ++pairs;
        }
    }
    const int total = pairs * static_cast<int>(settings.distancesOctaves.size()) * 2;
    int done = 0;
    const std::vector<std::complex<double>> neither = measure(base);
    for (size_t i = 0; i < bands.size(); ++i)
    {
        for (size_t j = i + 1; j < bands.size(); ++j)
        {
            for (const double distance : settings.distancesOctaves)
            {
                for (const double sign : {1.0, -1.0})
                {
                    InteractionCase item;
                    item.bandA = bands[i];
                    item.bandB = bands[j];
                    item.frequencyA = settings.centreHz;
                    item.frequencyB = settings.centreHz * std::pow(2.0, distance);
                    item.gainA = settings.gainDb;
                    item.gainB = sign * settings.gainDb;
                    if (progress)
                    {
                        progress(static_cast<double>(done) / std::max(total, 1),
                                 "bands " + juce::String(item.bandA + 1) + " and " + juce::String(item.bandB + 1) + ", " + formatDistance(distance));
                    }
                    ++done;
                    juce::StringArray missing;
                    const std::vector<float> onlyA = makeSettingForBand(mapping, base, item.bandA, makePeak(item.frequencyA, item.gainA), missing);
                    const std::vector<float> onlyB = makeSettingForBand(mapping, base, item.bandB, makePeak(item.frequencyB, item.gainB), missing);
                    const std::vector<float> both = makeSettingForBand(mapping, onlyA, item.bandB, makePeak(item.frequencyB, item.gainB), missing);
                    if (! missing.isEmpty())
                    {
                        missing.removeDuplicates(false);
                        item.note = "the mapping cannot set " + missing.joinIntoString(", ");
                    }
                    const std::vector<std::complex<double>>& a = measure(onlyA);
                    const std::vector<std::complex<double>>& b = measure(onlyB);
                    const std::vector<std::complex<double>>& ab = measure(both);
                    for (size_t index = 0; index < grid.size(); ++index)
                    {
                        const double deviation = toDb(ab[index]) - toDb(a[index]) - toDb(b[index]) + toDb(neither[index]);
                        if (std::abs(deviation) > std::abs(item.largestDeviationDb))
                        {
                            item.largestDeviationDb = deviation;
                            item.atHz = grid[index];
                        }
                    }
                    item.interacts = std::abs(item.largestDeviationDb) > settings.toleranceDb;
                    result.additive = result.additive && ! item.interacts;
                    result.cases.push_back(item);
                }
            }
        }
    }
    const auto worst = std::max_element(result.cases.begin(), result.cases.end(), [](const InteractionCase& x, const InteractionCase& y) {
        return std::abs(x.largestDeviationDb) < std::abs(y.largestDeviationDb);
    });
    if (result.additive)
    {
        result.conclusion = "the bands add in dB (largest deviation from the sum of single bands " + juce::String(std::abs(worst->largestDeviationDb), 3)
                            + " dB): single bands describe the plugin";
    }
    else
    {
        result.conclusion = "the bands interact: up to " + juce::String(worst->largestDeviationDb, 2) + " dB against the sum of single bands (bands "
                            + juce::String(worst->bandA + 1) + " and " + juce::String(worst->bandB + 1) + ", " + formatGain(worst->gainA) + " / "
                            + formatGain(worst->gainB) + ", at " + formatFrequency(worst->atHz)
                            + " Hz): the bands must be described together, a calibration or a match band by band is not enough";
    }
    if (progress)
    {
        progress(1.0, "done");
    }
    result.seconds = (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0;
    return result;
}

juce::String createInteractionReport(const InteractionResult& result)
{
    juce::String text;
    text << "## Interactions between bands\n\n";
    text << "Do the bands add in dB? Each pair of bands set to RBJ peaks through the mapping (band A at " << formatFrequency(result.settings.centreHz)
         << " Hz, band B above it, gains +-" << juce::String(result.settings.gainDb, 0) << " dB, Q " << juce::String(result.settings.q, 1)
         << "); the response of both against the sum of the single bands, dB(A+B) - dB(A) - dB(B) + dB(neither), 20 Hz ... 20 kHz (W8.7).";
    if (! result.measured)
    {
        text << " " << result.conclusion.substring(0, 1).toUpperCase() << result.conclusion.substring(1) << ".\n";
        return wrapMarkdownProse(text);
    }
    text << " " << result.renders << " renders (" << juce::roundToInt(result.seconds) << " s).\n\n";
    text << "**" << result.conclusion.substring(0, 1).toUpperCase() << result.conclusion.substring(1) << ".**\n\n";
    text << "| Bands | Distance | Gains | Largest deviation from the sum | At | Verdict |\n|---|---|---|---|---|---|\n";
    for (const InteractionCase& item : result.cases)
    {
        const double octaves = std::log2(item.frequencyB / item.frequencyA);
        text << "| " << (item.bandA + 1) << " and " << (item.bandB + 1) << " | " << formatDistance(octaves) << " | " << formatGain(item.gainA) << " / "
             << formatGain(item.gainB) << " | " << getSignPrefix(item.largestDeviationDb) << juce::String(item.largestDeviationDb, 3) << " dB | "
             << formatInteractionFrequency(item) << " | " << formatVerdict(item)
             << formatNote(item.note) << " |\n";
    }
    return wrapMarkdownProse(text);
}
}
