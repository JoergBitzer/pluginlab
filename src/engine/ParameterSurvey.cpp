#include "pluginlab/engine/ParameterSurvey.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <memory>

#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/measure/Distortion.h"
#include "pluginlab/measure/SweptImpulse.h"

namespace pluginlab::engine
{
namespace
{
constexpr double kTiny = 1.0e-15;
constexpr double kValidDb = -80.0;            // frequencies where both responses are below this are not compared (stop bands)
constexpr double kPolarityTolerance = 2.0e-3; // |H_k + H_0| / |H_0| below this: the sign flipped
constexpr float kContextOddValue = 0.3713f;   // an irregular value: a fractional delay, an odd setting
constexpr int kFitBands = 3;
constexpr int kMaximumHarmonic = 5;
constexpr double kHarmonicBandFactor = 0.9;   // the band of W8.1: harmonics up to the 5th below 0.9 Nyquist
constexpr double kHarmonicLowestHz = 50.0;
constexpr double kStationaryDb = 3.0;
constexpr double kStopBandDb = 60.0;          // harmonics where H_1 is more than this below its maximum are noise (as in W8.1)
constexpr double kMinorBandShare = 0.35;      // a band with less effect than this share of the dominant band is a detail of the fit         // the second half of the idle output within this of the first: stationary

double toDb(double magnitude)
{
    return 20.0 * std::log10(std::max(magnitude, kTiny));
}

bool isDiscrete(const SurveyParameter& parameter, const SurveySettings& settings)
{
    return parameter.isBoolean || (parameter.numSteps >= 2 && parameter.numSteps <= settings.maximumSteps);
}

float getStepValue(int step, int numSteps)
{
    return static_cast<float>(step) / static_cast<float>(numSteps - 1);
}

int getSteps(const SurveyParameter& parameter)
{
    return parameter.isBoolean ? 2 : parameter.numSteps;
}

// The values of a probe (does the parameter act at all in this context?): three points
std::vector<float> getProbeValues(const SurveyParameter& parameter, const SurveySettings& settings)
{
    if (isDiscrete(parameter, settings))
    {
        const int steps = getSteps(parameter);
        if (steps <= 4)
        {
            std::vector<float> values;
            for (int step = 0; step < steps; ++step)
            {
                values.push_back(getStepValue(step, steps));
            }
            return values;
        }
        return {0.0f, getStepValue(steps / 2, steps), 1.0f};
    }
    return {0.05f, 0.95f};
}

// The values another parameter takes as a context: high, low and an irregular value; every step of a discrete one but its default
std::vector<float> getContextValues(const SurveyParameter& parameter, const SurveySettings& settings, bool inPair = false)
{
    std::vector<float> values;
    if (isDiscrete(parameter, settings))
    {
        const int steps = getSteps(parameter);
        for (int step = 0; step < steps; ++step)
        {
            const float value = getStepValue(step, steps);
            if (std::abs(value - parameter.defaultValue) > 0.5f / static_cast<float>(steps))
            {
                values.push_back(value);
            }
        }
        return values;
    }
    for (const float value : {0.95f, 0.05f, kContextOddValue})
    {
        if (inPair && value == kContextOddValue)
        {
            continue; // pairs: the ends only (the search grows with the product)
        }
        if (std::abs(value - parameter.defaultValue) > 1.0e-3f)
        {
            values.push_back(value);
        }
    }
    return values;
}

// A number with many digits ("21.5999985") rounded for reading; other texts as the plugin shows them
juce::String tidyText(const juce::String& text)
{
    const juce::String trimmed = text.trim();
    if (trimmed.isEmpty() || ! trimmed.containsOnly("0123456789.-+") || ! trimmed.containsAnyOf("0123456789"))
    {
        return trimmed;
    }
    const double value = trimmed.getDoubleValue();
    const double magnitude = std::abs(value);
    const int decimals = magnitude >= 100.0 ? 0 : (magnitude >= 10.0 ? 1 : (magnitude >= 1.0 ? 2 : 3));
    if (magnitude < 0.5 * std::pow(10.0, -decimals))
    {
        return "0";
    }
    return juce::String(value, decimals);
}

juce::String getValueText(const SurveyParameter& parameter, float value)
{
    juce::String text = parameter.getText ? tidyText(parameter.getText(value)) : juce::String(value, 3);
    if (parameter.label.isNotEmpty() && ! text.endsWith(parameter.label))
    {
        text << " " << parameter.label;
    }
    return text.trim();
}

// The differences between two points
struct Change
{
    double magnitudeDb = 0.0;       // largest |dB difference| of the direct path
    double spreadDb = 0.0;          // max - min of the dB difference over frequency
    double complexRatio = 0.0;      // largest |H_k - H_0| / max(|H_k|, |H_0|)
    bool polarity = false;          // H_k = -H_0
    double crossDb = 0.0;
    int delaySamples = 0;
    double harmonicDb = 0.0;
    double idleDb = 0.0;
};

Change compare(const SurveyPoint& reference, const SurveyPoint& point, const SurveySettings& settings)
{
    Change change;
    double lowest = 1.0e9;
    double highest = -1.0e9;
    double polarityError = 0.0;
    bool anyValid = false;
    for (size_t index = 0; index < reference.direct.size() && index < point.direct.size(); ++index)
    {
        const double a = std::abs(reference.direct[index]);
        const double b = std::abs(point.direct[index]);
        if (toDb(std::max(a, b)) < kValidDb)
        {
            continue;
        }
        anyValid = true;
        const double difference = toDb(b) - toDb(a);
        lowest = std::min(lowest, difference);
        highest = std::max(highest, difference);
        change.magnitudeDb = std::max(change.magnitudeDb, std::abs(difference));
        change.complexRatio = std::max(change.complexRatio, std::abs(point.direct[index] - reference.direct[index]) / std::max({a, b, kTiny}));
        polarityError = std::max(polarityError, std::abs(point.direct[index] + reference.direct[index]) / std::max({a, b, kTiny}));
    }
    if (anyValid)
    {
        change.spreadDb = highest - lowest;
        change.polarity = polarityError < kPolarityTolerance && change.complexRatio > 1.0;
    }
    for (size_t index = 0; index < reference.cross.size() && index < point.cross.size(); ++index)
    {
        const double a = std::max(toDb(std::abs(reference.cross[index])), settings.crossFloorDb);
        const double b = std::max(toDb(std::abs(point.cross[index])), settings.crossFloorDb);
        change.crossDb = std::max(change.crossDb, std::abs(b - a));
    }
    change.delaySamples = std::abs(point.delaySamples - reference.delaySamples);
    change.harmonicDb = std::abs(std::max(point.harmonicDb, settings.harmonicFloorDb) - std::max(reference.harmonicDb, settings.harmonicFloorDb));
    change.idleDb = std::abs(std::max(point.idleDbfs, settings.idleFloorDbfs) - std::max(reference.idleDbfs, settings.idleFloorDbfs));
    return change;
}

bool isChange(const Change& change, const SurveySettings& settings)
{
    const double complexLimit = std::pow(10.0, settings.nothingDb / 20.0) - 1.0;
    return change.magnitudeDb > settings.nothingDb || change.complexRatio > complexLimit || change.crossDb > settings.nothingDb
           || change.delaySamples > 0 || change.harmonicDb > settings.harmonicChangeDb || change.idleDb > settings.idleChangeDb;
}

// Measures points and caches them by setting
class SurveyBench
{
public:
    SurveyBench(const SurveySubject& subject, const SurveySettings& settings, std::vector<double> frequencies)
        : m_subject(subject), m_settings(settings), m_frequencies(std::move(frequencies))
    {
        const double highest = kHarmonicBandFactor * settings.sampleRate / 2.0 / kMaximumHarmonic;
        m_harmonicGrid = eqmodel::makeLogGrid(kHarmonicLowestHz, highest, 6.0);
    }

    const SurveyPoint& measure(const std::vector<float>& setting)
    {
        auto found = m_cache.find(setting);
        if (found != m_cache.end())
        {
            return found->second;
        }
        ++m_renders;
        const double start = juce::Time::getMillisecondCounterHiRes();
        SurveyPoint point = render(setting);
        m_seconds += (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        return m_cache.emplace(setting, std::move(point)).first->second;
    }

    int getRenders() const
    {
        return m_renders;
    }

    double getSeconds() const
    {
        return m_seconds;
    }

    const std::vector<double>& getFrequencies() const
    {
        return m_frequencies;
    }

private:
    SurveyPoint render(const std::vector<float>& setting) const
    {
        SurveyPoint point;
        const measure::Device device = m_subject.makeDevice(setting);
        const int idleSamples = static_cast<int>(std::round(m_settings.idleSeconds * m_settings.sampleRate));
        auto idleSquares = std::make_shared<std::array<double, 2>>(std::array<double, 2>{0.0, 0.0}); // first and second half
        auto idleCount = std::make_shared<std::array<int, 2>>(std::array<int, 2>{0, 0});
        // silence, then the sweep on the left input only
        const measure::Device wrapped = [device, idleSamples, idleSquares, idleCount](const juce::AudioBuffer<float>& input, double sampleRate)
        {
            juce::AudioBuffer<float> extended(input.getNumChannels(), idleSamples + input.getNumSamples());
            extended.clear();
            extended.copyFrom(0, idleSamples, input, 0, 0, input.getNumSamples());
            const juce::AudioBuffer<float> output = device(extended, sampleRate);
            juce::AudioBuffer<float> result(input.getNumChannels(), input.getNumSamples());
            result.clear();
            for (int channel = 0; channel < output.getNumChannels(); ++channel)
            {
                const float* data = output.getReadPointer(channel);
                for (int sample = 0; sample < idleSamples && sample < output.getNumSamples(); ++sample)
                {
                    const size_t half = sample < idleSamples / 2 ? 0 : 1;
                    (*idleSquares)[half] += static_cast<double>(data[sample]) * data[sample];
                    ++(*idleCount)[half];
                }
                if (channel < result.getNumChannels() && output.getNumSamples() >= idleSamples + input.getNumSamples())
                {
                    result.copyFrom(channel, 0, output, channel, idleSamples, input.getNumSamples());
                }
            }
            return result;
        };
        measure::SweepResponseSettings sweep;
        sweep.sampleRate = m_settings.sampleRate;
        sweep.approximateSeconds = m_settings.sweepSeconds;
        sweep.levelDbfs = m_settings.levelDbfs;
        sweep.frequencies = m_frequencies;
        sweep.channels = std::max(1, std::min(2, m_subject.channels));
        const measure::SweptImpulses impulses = measure::measureSweptImpulses(wrapped, sweep);
        if (impulses.channels.empty())
        {
            return point;
        }
        for (const double frequency : m_frequencies)
        {
            point.direct.push_back(measure::getSweptResponse(impulses, 0, frequency));
            if (impulses.channels.size() > 1)
            {
                point.cross.push_back(measure::getSweptResponse(impulses, 1, frequency));
            }
        }
        point.delaySamples = impulses.channels[0].peak;
        const std::vector<std::complex<double>> linear = measure::getHarmonicResponse(impulses, 0, 1, m_harmonicGrid);
        double largestLinear = 0.0;
        for (const std::complex<double>& value : linear)
        {
            largestLinear = std::max(largestLinear, std::abs(value));
        }
        for (int order = 2; order <= kMaximumHarmonic; ++order)
        {
            const std::vector<std::complex<double>> harmonic = measure::getHarmonicResponse(impulses, 0, order, m_harmonicGrid);
            for (size_t index = 0; index < harmonic.size() && index < linear.size(); ++index)
            {
                if (std::abs(linear[index]) > kTiny && toDb(std::abs(linear[index]) / largestLinear) > -kStopBandDb) // not in a stop band
                {
                    point.harmonicDb = std::max(point.harmonicDb, toDb(std::abs(harmonic[index]) / std::abs(linear[index])));
                }
            }
        }
        // the idle output counts only if it is stationary (noise, hum, DC); a decay is the setting still settling (a resonance rings)
        if ((*idleCount)[0] > 0 && (*idleCount)[1] > 0)
        {
            const double first = 10.0 * std::log10(std::max((*idleSquares)[0] / (*idleCount)[0], 1.0e-30));
            const double second = 10.0 * std::log10(std::max((*idleSquares)[1] / (*idleCount)[1], 1.0e-30));
            if (second >= first - kStationaryDb)
            {
                point.idleDbfs = second;
            }
            else
            {
                point.settlingDbfs = first;
            }
        }
        return point;
    }

    const SurveySubject& m_subject;
    const SurveySettings& m_settings;
    std::vector<double> m_frequencies;
    std::vector<double> m_harmonicGrid;
    std::map<std::vector<float>, SurveyPoint> m_cache;
    int m_renders = 0;
    double m_seconds = 0.0;
};

std::vector<float> makeSetting(const std::vector<float>& base, const std::vector<int>& parameters, const std::vector<float>& values)
{
    std::vector<float> setting = base;
    for (size_t index = 0; index < parameters.size(); ++index)
    {
        setting[static_cast<size_t>(parameters[index])] = values[index];
    }
    return setting;
}

// --- the role from the points -------------------------------------------------------------------------------------------------------------------

struct BandSignature
{
    reference::FilterType type;
    eqmodel::DesignFamily family;
};

juce::String describeStructure(const eqmodel::FitResult& fit)
{
    if (! fit.goodDescription)
    {
        return "no band description";
    }
    if (fit.model.bands.empty())
    {
        return std::abs(fit.model.outputGainDb) > 0.01 ? "flat " + juce::String(fit.model.outputGainDb, 2) + " dB" : "flat";
    }
    juce::StringArray parts;
    for (const eqmodel::Band& band : fit.model.bands)
    {
        juce::String part = juce::String(reference::getFilterTypeName(band.type)) + " " + juce::String(band.frequencyHz, 0) + " Hz";
        if (band.type == reference::FilterType::Peak || band.type == reference::FilterType::LowShelf || band.type == reference::FilterType::HighShelf)
        {
            part << " " << (band.gainDb >= 0.0 ? "+" : "") << juce::String(band.gainDb, 1) << " dB";
        }
        if (band.family == eqmodel::DesignFamily::Butterworth || band.family == eqmodel::DesignFamily::LinkwitzRiley)
        {
            part << " order " << band.order;
        }
        else
        {
            part << " Q " << juce::String(band.q, 2);
        }
        part << " (" << eqmodel::getFamilyName(band.family) << ")";
        parts.add(part);
    }
    juce::String text = parts.joinIntoString(", ");
    if (fit.linearPhase)
    {
        text << ", linear phase";
    }
    return text;
}

// The types of the bands (sorted), and with the families
juce::String getTypeKey(const eqmodel::FitResult& fit, bool withFamily)
{
    juce::StringArray keys;
    for (const eqmodel::Band& band : fit.model.bands)
    {
        juce::String key = reference::getFilterTypeName(band.type);
        if (withFamily)
        {
            const bool passFilter = band.type == reference::FilterType::LowPass || band.type == reference::FilterType::HighPass;
            key << "/" << (passFilter ? juce::String("pass") : juce::String(eqmodel::getFamilyName(band.family)));
        }
        keys.add(key);
    }
    keys.sort(false);
    juce::String text = keys.joinIntoString(",");
    if (withFamily && fit.linearPhase)
    {
        text << ",linear";
    }
    return text;
}

bool hasGain(reference::FilterType type)
{
    return type == reference::FilterType::Peak || type == reference::FilterType::LowShelf || type == reference::FilterType::HighShelf;
}

struct QuantityChange
{
    ParameterRole role;
    double size;          // normalised: 1 = clearly changed
    juce::String text;
};

// The band quantities that change over the points (all fits good). Returns false if the structure differs.
bool analyseBands(const std::vector<SurveyPoint>& points, const SurveySettings& settings, bool discrete, std::vector<QuantityChange>& changes,
                  juce::String& structureText, bool& onOff)
{
    onOff = false;
    std::map<juce::String, int> counts;
    for (const SurveyPoint& point : points)
    {
        if (! point.fit->model.bands.empty())
        {
            ++counts[getTypeKey(*point.fit, false)];
        }
    }
    if (counts.size() > 1)
    {
        juce::StringArray texts;
        for (const SurveyPoint& point : points)
        {
            texts.add(point.text + ": " + describeStructure(*point.fit));
        }
        structureText = texts.joinIntoString("; ");
        return false;
    }
    std::vector<const SurveyPoint*> present;
    int absent = 0;
    for (const SurveyPoint& point : points)
    {
        if (point.fit->model.bands.empty())
        {
            ++absent;
        }
        else
        {
            present.push_back(&point);
        }
    }
    const double outputLowest = std::min_element(points.begin(), points.end(), [](const SurveyPoint& a, const SurveyPoint& b) {
                                    return a.fit->model.outputGainDb < b.fit->model.outputGainDb;
                                })->fit->model.outputGainDb;
    const double outputHighest = std::max_element(points.begin(), points.end(), [](const SurveyPoint& a, const SurveyPoint& b) {
                                     return a.fit->model.outputGainDb < b.fit->model.outputGainDb;
                                 })->fit->model.outputGainDb;
    if (outputHighest - outputLowest > settings.gainChangeDb)
    {
        changes.push_back({ParameterRole::Level, (outputHighest - outputLowest) / 3.0,
                           "output gain " + juce::String(outputLowest, 2) + " ... " + juce::String(outputHighest, 2) + " dB"});
    }
    if (present.empty())
    {
        return true;
    }
    const size_t bandCount = present.front()->fit->model.bands.size();
    // the bands in the order of their frequency
    auto sortedBands = [](const SurveyPoint& point) {
        std::vector<eqmodel::Band> bands = point.fit->model.bands;
        std::sort(bands.begin(), bands.end(), [](const eqmodel::Band& a, const eqmodel::Band& b) { return a.type == b.type ? a.frequencyHz < b.frequencyHz : a.type < b.type; });
        return bands;
    };
    bool anyBandQuantity = false;
    for (size_t slot = 0; slot < bandCount; ++slot)
    {
        double lowF = 1.0e9, highF = 0.0, lowG = 1.0e9, highG = -1.0e9, lowQ = 1.0e9, highQ = 0.0;
        int lowOrder = 100, highOrder = 0;
        reference::FilterType type = reference::FilterType::Peak;
        for (const SurveyPoint* point : present)
        {
            const eqmodel::Band band = sortedBands(*point)[slot];
            type = band.type;
            lowF = std::min(lowF, band.frequencyHz);
            highF = std::max(highF, band.frequencyHz);
            lowG = std::min(lowG, band.gainDb);
            highG = std::max(highG, band.gainDb);
            lowQ = std::min(lowQ, band.q);
            highQ = std::max(highQ, band.q);
            lowOrder = std::min(lowOrder, band.order);
            highOrder = std::max(highOrder, band.order);
        }
        const bool gainMoves = hasGain(type) && highG - lowG > settings.gainChangeDb;
        if (absent > 0 && hasGain(type) && (gainMoves || present.size() == 1))
        {
            lowG = std::min(lowG, 0.0); // a band that vanishes has the gain 0 there
            highG = std::max(highG, 0.0);
        }
        const juce::String bandName = juce::String(reference::getFilterTypeName(type)) + (bandCount > 1 ? " " + juce::String(static_cast<int>(slot) + 1) : "");
        const double octaves = std::log2(highF / lowF);
        if (octaves > settings.frequencyChangeOctaves)
        {
            changes.push_back({ParameterRole::Frequency, octaves / 0.5, bandName + " frequency " + juce::String(lowF, 0) + " ... " + juce::String(highF, 0) + " Hz"});
            anyBandQuantity = true;
        }
        if (hasGain(type) && highG - lowG > settings.gainChangeDb && (gainMoves || ! discrete))
        {
            changes.push_back({ParameterRole::Gain, (highG - lowG) / 3.0, bandName + " gain " + juce::String(lowG, 1) + " ... " + juce::String(highG, 1) + " dB"});
            anyBandQuantity = true;
        }
        if (highQ / lowQ - 1.0 > settings.widthChange)
        {
            changes.push_back({ParameterRole::Width, std::log2(highQ / lowQ) / 0.5, bandName + " Q " + juce::String(lowQ, 2) + " ... " + juce::String(highQ, 2)});
            anyBandQuantity = true;
        }
        if (highOrder != lowOrder)
        {
            changes.push_back({ParameterRole::Slope, static_cast<double>(highOrder - lowOrder), bandName + " order " + juce::String(lowOrder) + " ... " + juce::String(highOrder)});
            anyBandQuantity = true;
        }
    }
    if (absent > 0 && ! anyBandQuantity)
    {
        onOff = true; // the band is either there (always the same) or not
    }
    return true;
}

juce::String describeContext(const std::vector<SurveyParameter>& parameters, const std::vector<int>& indices, const std::vector<float>& values)
{
    juce::StringArray parts;
    for (size_t index = 0; index < indices.size(); ++index)
    {
        const SurveyParameter& parameter = parameters[static_cast<size_t>(indices[index])];
        parts.add(parameter.name + " = " + getValueText(parameter, values[index]));
    }
    return parts.joinIntoString(", ");
}

// A peak or shelf with (almost) no gain changes nothing: the fit may leave one in where it does not hurt the residual
void removeSilentBands(eqmodel::FitResult& fit, double gainLimitDb)
{
    std::vector<eqmodel::Band>& bands = fit.model.bands;
    bands.erase(std::remove_if(bands.begin(), bands.end(), [gainLimitDb](const eqmodel::Band& band) { return hasGain(band.type) && std::abs(band.gainDb) < gainLimitDb; }),
                bands.end());
}

// Leaves only the dominant bands: a peak or shelf with less than kMinorBandShare of the largest gain is a detail of the fit (e.g. at the edge of the
// measured band). Returns what was left out.
juce::StringArray keepDominantBands(std::vector<SurveyPoint>& points)
{
    juce::StringArray removed;
    for (SurveyPoint& point : points)
    {
        std::vector<eqmodel::Band>& bands = point.fit->model.bands;
        double largest = 0.0;
        bool anyPass = false;
        for (const eqmodel::Band& band : bands)
        {
            if (hasGain(band.type))
            {
                largest = std::max(largest, std::abs(band.gainDb));
            }
            else
            {
                anyPass = true;
            }
        }
        const double limit = anyPass ? 3.0 : kMinorBandShare * largest; // next to a pass filter, only shelves and peaks of a few dB count
        for (auto band = bands.begin(); band != bands.end();)
        {
            if (hasGain(band->type) && std::abs(band->gainDb) < limit)
            {
                removed.add(point.text + ": " + reference::getFilterTypeName(band->type) + " " + juce::String(band->frequencyHz, 0) + " Hz "
                            + juce::String(band->gainDb, 1) + " dB");
                band = bands.erase(band);
            }
            else
            {
                ++band;
            }
        }
    }
    return removed;
}

class Surveyor
{
public:
    Surveyor(const SurveySubject& subject, const SurveySettings& settings, std::vector<double> frequencies)
        : m_subject(subject), m_settings(settings), m_bench(subject, settings, std::move(frequencies))
    {
        for (const SurveyParameter& parameter : subject.parameters)
        {
            m_default.push_back(parameter.defaultValue);
        }
    }

    const std::vector<float>& getDefault() const
    {
        return m_default;
    }

    SurveyBench& getBench()
    {
        return m_bench;
    }

    // does the parameter act in this context?
    bool probe(int parameter, const std::vector<float>& base)
    {
        const std::vector<float> values = getProbeValues(m_subject.parameters[static_cast<size_t>(parameter)], m_settings);
        const SurveyPoint first = m_bench.measure(makeSetting(base, {parameter}, {values.front()}));
        for (size_t index = 1; index < values.size(); ++index)
        {
            if (isChange(compare(first, m_bench.measure(makeSetting(base, {parameter}, {values[index]})), m_settings), m_settings))
            {
                return true;
            }
        }
        return false;
    }

    // a context in which a parameter that does nothing at the default acts: one other parameter moved, then two
    bool findContext(int parameter, std::vector<int>& indices, std::vector<float>& values)
    {
        const int start = m_bench.getRenders();
        const int count = static_cast<int>(m_subject.parameters.size());
        std::vector<int> order;
        for (const bool discretePass : {false, true})
        {
            for (int other = 0; other < count; ++other)
            {
                if (other != parameter && isDiscrete(m_subject.parameters[static_cast<size_t>(other)], m_settings) == discretePass)
                {
                    order.push_back(other);
                }
            }
        }
        // the contexts in which other parameters acted, alone first
        for (const auto& [contextIndices, contextValues] : m_activating)
        {
            if (std::find(contextIndices.begin(), contextIndices.end(), parameter) == contextIndices.end()
                && probe(parameter, makeSetting(m_default, contextIndices, contextValues)))
            {
                return remember(contextIndices, contextValues, indices, values);
            }
        }
        for (const int other : order)
        {
            for (const float value : getContextValues(m_subject.parameters[static_cast<size_t>(other)], m_settings))
            {
                if (m_bench.getRenders() - start > m_settings.maximumSearchRenders)
                {
                    return false;
                }
                if (probe(parameter, makeSetting(m_default, {other}, {value})))
                {
                    return remember({other}, {value}, indices, values);
                }
            }
        }
        if (! m_settings.searchPairs)
        {
            return false;
        }
        // pairs: a context in which another parameter acted, with one more parameter moved (an enable switch and a gain)
        for (const auto& [contextIndices, contextValues] : m_activating)
        {
            if (contextIndices.size() != 1 || contextIndices.front() == parameter)
            {
                continue;
            }
            for (const int other : order)
            {
                if (other == contextIndices.front())
                {
                    continue;
                }
                for (const float value : getContextValues(m_subject.parameters[static_cast<size_t>(other)], m_settings, true))
                {
                    if (m_bench.getRenders() - start > m_settings.maximumSearchRenders)
                    {
                        return false;
                    }
                    const std::vector<int> pairIndices{contextIndices.front(), other};
                    const std::vector<float> pairValues{contextValues.front(), value};
                    if (probe(parameter, makeSetting(m_default, pairIndices, pairValues)))
                    {
                        return remember(pairIndices, pairValues, indices, values);
                    }
                }
            }
        }
        // pairs: discrete pairs first (a design and a type), then mixed, then continuous
        std::vector<std::pair<int, int>> pairs;
        for (int discreteCount = 2; discreteCount >= 0; --discreteCount)
        {
            for (size_t a = 0; a < order.size(); ++a)
            {
                for (size_t b = a + 1; b < order.size(); ++b)
                {
                    const int first = std::min(order[a], order[b]);
                    const int second = std::max(order[a], order[b]);
                    const int discrete = (isDiscrete(m_subject.parameters[static_cast<size_t>(first)], m_settings) ? 1 : 0)
                                         + (isDiscrete(m_subject.parameters[static_cast<size_t>(second)], m_settings) ? 1 : 0);
                    if (discrete == discreteCount)
                    {
                        pairs.emplace_back(first, second);
                    }
                }
            }
        }
        for (const auto& [first, second] : pairs)
        {
            for (const float firstValue : getContextValues(m_subject.parameters[static_cast<size_t>(first)], m_settings, true))
            {
                for (const float secondValue : getContextValues(m_subject.parameters[static_cast<size_t>(second)], m_settings, true))
                {
                    if (m_bench.getRenders() - start > m_settings.maximumSearchRenders)
                    {
                        return false;
                    }
                    if (probe(parameter, makeSetting(m_default, {first, second}, {firstValue, secondValue})))
                    {
                        return remember({first, second}, {firstValue, secondValue}, indices, values);
                    }
                }
            }
        }
        return false;
    }

    bool remember(const std::vector<int>& contextIndices, const std::vector<float>& contextValues, std::vector<int>& indices, std::vector<float>& values)
    {
        indices = contextIndices;
        values = contextValues;
        const auto key = std::make_pair(contextIndices, contextValues);
        if (std::find(m_activating.begin(), m_activating.end(), key) == m_activating.end())
        {
            m_activating.push_back(key);
        }
        return true;
    }

    ContextSurvey survey(int parameterIndex, const std::vector<int>& contextParameters, const std::vector<float>& contextValues,
                         std::optional<measure::SystemClass> baseClass)
    {
        const SurveyParameter& parameter = m_subject.parameters[static_cast<size_t>(parameterIndex)];
        ContextSurvey result;
        result.contextParameters = contextParameters;
        result.contextValues = contextValues;
        result.contextText = contextParameters.empty() ? juce::String("default setting") : describeContext(m_subject.parameters, contextParameters, contextValues);
        const std::vector<float> base = makeSetting(m_default, contextParameters, contextValues);
        for (const float value : getSurveyValues(parameter, m_settings))
        {
            SurveyPoint point = m_bench.measure(makeSetting(base, {parameterIndex}, {value}));
            point.value = value;
            point.text = getValueText(parameter, value);
            result.points.push_back(std::move(point));
        }
        decideRole(result, parameter, base, parameterIndex, baseClass);
        return result;
    }

    std::optional<measure::SystemClass> classify(const std::vector<float>& setting)
    {
        if (! m_settings.classify)
        {
            return std::nullopt;
        }
        auto found = m_classes.find(setting);
        if (found != m_classes.end())
        {
            return found->second;
        }
        measure::SystemClassSettings classSettings;
        classSettings.sampleRate = m_settings.sampleRate;
        const double start = juce::Time::getMillisecondCounterHiRes();
        const measure::SystemClass systemClass = measure::classifySystem(m_subject.makeDevice(setting), classSettings).systemClass;
        m_classifySeconds += (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
        m_classes.emplace(setting, systemClass);
        return systemClass;
    }

private:
    void decideRole(ContextSurvey& result, const SurveyParameter& parameter, const std::vector<float>& base, int parameterIndex,
                    std::optional<measure::SystemClass> baseClass)
    {
        const SurveySettings& s = m_settings;
        const bool discrete = isDiscrete(parameter, s);
        Change largest;
        bool anyChange = false;
        bool allPolarity = true;
        bool anyDiffers = false;
        for (size_t index = 1; index < result.points.size(); ++index)
        {
            const Change change = compare(result.points.front(), result.points[index], s);
            if (isChange(change, s))
            {
                anyChange = true;
                anyDiffers = true;
                allPolarity = allPolarity && change.polarity;
            }
            largest.magnitudeDb = std::max(largest.magnitudeDb, change.magnitudeDb);
            largest.spreadDb = std::max(largest.spreadDb, change.spreadDb);
            largest.complexRatio = std::max(largest.complexRatio, change.complexRatio);
            largest.crossDb = std::max(largest.crossDb, change.crossDb);
            largest.delaySamples = std::max(largest.delaySamples, change.delaySamples);
            largest.harmonicDb = std::max(largest.harmonicDb, change.harmonicDb);
            largest.idleDb = std::max(largest.idleDb, change.idleDb);
        }
        if (! anyChange)
        {
            result.role = ParameterRole::Nothing;
            result.detail = "no change (direct path within " + juce::String(largest.magnitudeDb, 4) + " dB)";
            return;
        }
        // the system class at both ends
        const std::optional<measure::SystemClass> first = classify(makeSetting(base, {parameterIndex}, {result.points.front().value}));
        const std::optional<measure::SystemClass> last = classify(makeSetting(base, {parameterIndex}, {result.points.back().value}));
        bool timeVariant = false;
        bool nonLinear = ! (first && last); // without the class, the harmonics of the survey sweep decide
        if (first && last)
        {
            nonLinear = (*first != measure::SystemClass::Lti && *first != measure::SystemClass::TimeVariant)
                        || (*last != measure::SystemClass::Lti && *last != measure::SystemClass::TimeVariant);
            result.classes = {*first, *last};
            result.changesSystemClass = *first != *last || (baseClass && (*first != *baseClass || *last != *baseClass));
            timeVariant = *first == measure::SystemClass::TimeVariant || *last == measure::SystemClass::TimeVariant;
        }
        juce::StringArray effects;
        if (largest.idleDb > s.idleChangeDb)
        {
            effects.add("idle output " + rangeText([](const SurveyPoint& p) { return p.idleDbfs; }, result.points, s.idleFloorDbfs, "dBFS"));
        }
        const bool distortion = largest.harmonicDb > s.harmonicChangeDb && nonLinear;
        if (distortion)
        {
            effects.add("harmonics " + rangeText([](const SurveyPoint& p) { return p.harmonicDb; }, result.points, s.harmonicFloorDb, "dB"));
        }
        if (largest.crossDb > s.nothingDb)
        {
            effects.add("cross path changes by up to " + juce::String(largest.crossDb, 1) + " dB");
        }
        if (largest.delaySamples > 0)
        {
            effects.add("delay " + juce::String(minOf(result.points, [](const SurveyPoint& p) { return p.delaySamples; })) + " ... "
                        + juce::String(maxOf(result.points, [](const SurveyPoint& p) { return p.delaySamples; })) + " samples");
        }
        if (result.changesSystemClass)
        {
            effects.add("system class " + measure::getSystemClassName(result.classes.front()) + " / " + measure::getSystemClassName(result.classes.back()));
        }
        const bool magnitudeChanges = largest.magnitudeDb > s.nothingDb;
        // added noise or hum shows in the idle output (and makes two sweeps differ, i.e. "time-variant"): the idle output explains it first
        if (largest.idleDb > s.idleChangeDb)
        {
            setRole(result, discrete, ParameterRole::IdleOutput, {}, effects);
            return;
        }
        if (timeVariant)
        {
            setRole(result, discrete, ParameterRole::Modulation, "time-variant", effects);
            return;
        }
        if (distortion)
        {
            setRole(result, discrete, ParameterRole::Distortion, {}, effects);
            return;
        }
        if (largest.crossDb > s.nothingDb)
        {
            if (magnitudeChanges)
            {
                result.alsoChanges.add("direct path by up to " + juce::String(largest.magnitudeDb, 2) + " dB");
            }
            result.role = ParameterRole::Stereo;
            result.detail = effects.joinIntoString("; ");
            return;
        }
        if (! magnitudeChanges)
        {
            if (anyDiffers && allPolarity)
            {
                result.role = ParameterRole::Polarity;
                result.detail = "the sign of the output flips";
            }
            else if (largest.delaySamples > 0)
            {
                result.role = ParameterRole::Latency;
                result.detail = effects.joinIntoString("; ");
            }
            else
            {
                setRole(result, discrete, ParameterRole::Other, "the phase changes, the magnitude does not", effects);
            }
            return;
        }
        if (largest.spreadDb < s.flatDb)
        {
            result.role = ParameterRole::Level;
            result.detail = "direct path " + rangeText([](const SurveyPoint& p) { return toDb(std::abs(p.direct[p.direct.size() / 2])); }, result.points, -300.0, "dB")
                            + " (flat within " + juce::String(largest.spreadDb, 3) + " dB)";
            if (! effects.isEmpty())
            {
                result.alsoChanges.addArray(effects);
            }
            return;
        }
        // the response changes in shape: describe each point by bands
        bool allGood = true;
        for (SurveyPoint& point : result.points)
        {
            eqmodel::FitSettings fitSettings;
            fitSettings.sampleRate = s.sampleRate;
            fitSettings.maximumBands = kFitBands;
            fitSettings.latencySamples = point.delaySamples;
            const double start = juce::Time::getMillisecondCounterHiRes();
            point.fit = eqmodel::fitEqModel(m_bench.getFrequencies(), point.direct, fitSettings);
            removeSilentBands(*point.fit, s.gainChangeDb);
            m_fitSeconds += (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0;
            allGood = allGood && point.fit->goodDescription;
        }
        if (! allGood)
        {
            juce::StringArray texts;
            for (const SurveyPoint& point : result.points)
            {
                texts.add(point.text + ": " + describeStructure(*point.fit));
            }
            setRole(result, discrete, ParameterRole::Other, texts.joinIntoString("; "), effects);
            return;
        }
        std::vector<QuantityChange> changes;
        juce::String structureText;
        bool onOff = false;
        bool sameStructure = analyseBands(result.points, s, discrete, changes, structureText, onOff);
        if (! sameStructure && ! discrete)
        {
            // a continuous parameter: the dominant band decides; small extra bands of the fit are reported, not taken as a change of structure
            std::vector<SurveyPoint> reduced = result.points;
            const juce::StringArray removed = keepDominantBands(reduced);
            std::vector<QuantityChange> reducedChanges;
            juce::String reducedText;
            if (! removed.isEmpty() && analyseBands(reduced, s, discrete, reducedChanges, reducedText, onOff))
            {
                sameStructure = true;
                changes = reducedChanges;
                effects.add("small bands of the fit left out (" + removed.joinIntoString("; ") + ")");
            }
        }
        // a discrete parameter that switches the design family or the phase with the same types
        if (sameStructure && discrete)
        {
            std::map<juce::String, int> families;
            for (const SurveyPoint& point : result.points)
            {
                if (! point.fit->model.bands.empty())
                {
                    ++families[getTypeKey(*point.fit, true)];
                }
            }
            if (families.size() > 1)
            {
                juce::StringArray texts;
                for (const SurveyPoint& point : result.points)
                {
                    texts.add(point.text + ": " + describeStructure(*point.fit));
                }
                result.role = ParameterRole::Switch;
                result.detail = "switches the design: " + texts.joinIntoString("; ");
                result.alsoChanges.addArray(effects);
                return;
            }
        }
        if (! sameStructure)
        {
            setRole(result, discrete, discrete ? ParameterRole::Switch : ParameterRole::Other, "the bands change: " + structureText, effects);
            return;
        }
        if (onOff)
        {
            setRole(result, discrete, discrete ? ParameterRole::Switch : ParameterRole::Other, "switches a band on and off: " + describePoints(result.points), effects);
            return;
        }
        if (changes.empty())
        {
            setRole(result, discrete, ParameterRole::Other, "the response changes, but no band quantity: " + describePoints(result.points), effects);
            return;
        }
        std::sort(changes.begin(), changes.end(), [](const QuantityChange& a, const QuantityChange& b) { return a.size > b.size; });
        result.role = changes.front().role;
        result.detail = changes.front().text;
        for (size_t index = 1; index < changes.size(); ++index)
        {
            result.alsoChanges.add(changes[index].text);
        }
        result.alsoChanges.addArray(effects);
    }

    static void setRole(ContextSurvey& result, bool discrete, ParameterRole role, const juce::String& detail, const juce::StringArray& effects)
    {
        result.role = discrete ? ParameterRole::Switch : role;
        juce::StringArray parts;
        if (discrete && role != ParameterRole::Switch && role != ParameterRole::Other)
        {
            parts.add("changes the " + getRoleName(role).toLowerCase());
        }
        if (detail.isNotEmpty())
        {
            parts.add(detail);
        }
        parts.addArray(effects);
        result.detail = parts.joinIntoString("; ");
    }

    static juce::String describePoints(const std::vector<SurveyPoint>& points)
    {
        juce::StringArray texts;
        for (const SurveyPoint& point : points)
        {
            texts.add(point.text + ": " + describeStructure(*point.fit));
        }
        return texts.joinIntoString("; ");
    }

    template <typename Getter>
    static juce::String rangeText(Getter getter, const std::vector<SurveyPoint>& points, double floor, const juce::String& unit)
    {
        double lowest = 1.0e9;
        double highest = -1.0e9;
        for (const SurveyPoint& point : points)
        {
            lowest = std::min(lowest, std::max(getter(point), floor));
            highest = std::max(highest, std::max(getter(point), floor));
        }
        auto text = [floor](double value) { return value <= floor ? juce::String("below ") + juce::String(floor, 0) : juce::String(value, 1); };
        return text(lowest) + " ... " + text(highest) + " " + unit;
    }

    template <typename Getter>
    static int minOf(const std::vector<SurveyPoint>& points, Getter getter)
    {
        int value = 1 << 30;
        for (const SurveyPoint& point : points)
        {
            value = std::min(value, getter(point));
        }
        return value;
    }

    template <typename Getter>
    static int maxOf(const std::vector<SurveyPoint>& points, Getter getter)
    {
        int value = -(1 << 30);
        for (const SurveyPoint& point : points)
        {
            value = std::max(value, getter(point));
        }
        return value;
    }

    const SurveySubject& m_subject;
    const SurveySettings& m_settings;
    SurveyBench m_bench;
    std::vector<float> m_default;
    std::map<std::vector<float>, measure::SystemClass> m_classes;
    std::vector<std::pair<std::vector<int>, std::vector<float>>> m_activating; // contexts found so far (tried first)

public:
    double m_classifySeconds = 0.0;
    double m_fitSeconds = 0.0;
};

juce::String makeSummary(const ParameterSurvey& survey)
{
    juce::String text = getRoleName(survey.role);
    const ContextSurvey& telling = survey.coupled ? *survey.inContext : survey.atDefault;
    if (telling.detail.isNotEmpty() && survey.role != ParameterRole::Nothing)
    {
        text << ": " << telling.detail;
    }
    if (survey.coupled)
    {
        text << " (acts only when " << survey.inContext->contextText << ")";
    }
    return text;
}
}

juce::String getRoleName(ParameterRole role)
{
    switch (role)
    {
        case ParameterRole::Nothing: return "does nothing";
        case ParameterRole::Level: return "level";
        case ParameterRole::Frequency: return "band frequency";
        case ParameterRole::Gain: return "band gain";
        case ParameterRole::Width: return "band width (Q)";
        case ParameterRole::Slope: return "band order";
        case ParameterRole::Latency: return "latency";
        case ParameterRole::Polarity: return "polarity";
        case ParameterRole::Stereo: return "stereo (cross path)";
        case ParameterRole::Distortion: return "distortion";
        case ParameterRole::IdleOutput: return "idle output (noise, hum, DC)";
        case ParameterRole::Modulation: return "modulation (time-variant)";
        case ParameterRole::Switch: return "switch";
        case ParameterRole::Other: return "other";
    }
    return {};
}

std::vector<float> getSurveyValues(const SurveyParameter& parameter, const SurveySettings& settings)
{
    std::vector<float> values;
    if (isDiscrete(parameter, settings))
    {
        const int steps = getSteps(parameter);
        for (int step = 0; step < steps; ++step)
        {
            values.push_back(getStepValue(step, steps));
        }
        return values;
    }
    const int count = std::max(2, settings.continuousPoints);
    for (int index = 0; index < count; ++index)
    {
        values.push_back(0.05f + 0.9f * static_cast<float>(index) / static_cast<float>(count - 1));
    }
    return values;
}

SurveySubject makePluginSubject(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate)
{
    SurveySubject subject;
    juce::String error;
    std::shared_ptr<juce::AudioPluginInstance> instance(formatManager.createPluginInstance(description, sampleRate, 512, error).release());
    if (instance == nullptr)
    {
        return subject;
    }
    for (juce::AudioProcessorParameter* parameter : instance->getParameters())
    {
        SurveyParameter surveyParameter;
        surveyParameter.name = parameter->getName(100);
        surveyParameter.label = parameter->getLabel();
        surveyParameter.numSteps = parameter->isDiscrete() || parameter->getNumSteps() < juce::AudioProcessor::getDefaultNumParameterSteps()
                                       ? parameter->getNumSteps()
                                       : 0;
        surveyParameter.isBoolean = parameter->isBoolean();
        surveyParameter.defaultValue = parameter->getValue();
        // the text the plugin shows (the instance stays alive with the subject; getText does not change the value)
        surveyParameter.getText = [instance, parameter](float value) { return parameter->getText(value, 64); };
        subject.parameters.push_back(surveyParameter);
    }
    subject.channels = getPluginChannels(formatManager, description, sampleRate);
    subject.makeDevice = [&formatManager, description](const std::vector<float>& setting) { return makePluginDevice(formatManager, description, setting); };
    return subject;
}

SurveyResult surveyParameters(const SurveySubject& subject, const SurveySettings& settings, const SurveyProgress& progress)
{
    SurveyResult result;
    result.settings = settings;
    result.frequencies = settings.frequencies.empty() ? eqmodel::makeLogGrid(20.0, 20000.0, 12.0) : settings.frequencies;
    Surveyor surveyor(subject, result.settings, result.frequencies);
    const int count = static_cast<int>(subject.parameters.size());
    result.defaultPoint = surveyor.getBench().measure(surveyor.getDefault());
    result.defaultClass = surveyor.classify(surveyor.getDefault());

    // 1. every parameter at the default setting; a parameter that does nothing there: a context in which it acts
    std::map<std::pair<std::vector<int>, std::vector<float>>, int> contextCounts;
    for (int index = 0; index < count; ++index)
    {
        if (progress)
        {
            progress(index, count);
        }
        ParameterSurvey survey;
        survey.index = index;
        survey.parameter = subject.parameters[static_cast<size_t>(index)];
        if (surveyor.probe(index, surveyor.getDefault()))
        {
            survey.atDefault = surveyor.survey(index, {}, {}, result.defaultClass);
        }
        else
        {
            survey.atDefault.contextText = "default setting";
            survey.atDefault.detail = "no change";
            std::vector<int> indices;
            std::vector<float> values;
            if (surveyor.findContext(index, indices, values))
            {
                survey.inContext = surveyor.survey(index, indices, values, std::nullopt);
                survey.coupled = true;
                if (indices.size() == 1)
                {
                    ++contextCounts[{indices, values}];
                }
            }
        }
        result.parameters.push_back(std::move(survey));
    }

    // 2. a switch is surveyed again where most coupled parameters act (e.g. a design switch with a band that has gain)
    if (! contextCounts.empty())
    {
        const auto best = std::max_element(contextCounts.begin(), contextCounts.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
        const std::vector<int>& indices = best->first.first;
        const std::vector<float>& values = best->first.second;
        for (ParameterSurvey& survey : result.parameters)
        {
            const bool isContextParameter = std::find(indices.begin(), indices.end(), survey.index) != indices.end();
            if (! survey.coupled && isDiscrete(survey.parameter, settings) && survey.atDefault.role != ParameterRole::Nothing && ! isContextParameter)
            {
                survey.inContext = surveyor.survey(survey.index, indices, values, std::nullopt);
            }
        }
    }

    for (ParameterSurvey& survey : result.parameters)
    {
        if (survey.coupled)
        {
            survey.role = survey.inContext->role;
        }
        else if (survey.inContext && survey.inContext->role != ParameterRole::Nothing)
        {
            survey.role = survey.inContext->role;
        }
        else
        {
            survey.role = survey.atDefault.role;
        }
        survey.summary = makeSummary(survey);
    }
    result.renders = surveyor.getBench().getRenders();
    result.renderSeconds = surveyor.getBench().getSeconds();
    result.classifySeconds = surveyor.m_classifySeconds;
    result.fitSeconds = surveyor.m_fitSeconds;
    if (progress)
    {
        progress(count, count);
    }
    return result;
}

juce::String createSurveyReport(const SurveyResult& result, const juce::String& title)
{
    juce::String text;
    text << "## " << title << "\n\n";
    text << "Every parameter alone over its range (W8.4). Sweep " << juce::String(result.settings.levelDbfs, 0) << " dBFS on the left input, "
         << juce::String(result.settings.sampleRate / 1000.0, 1) << " kHz";
    if (result.defaultClass)
    {
        text << "; system class at the default setting: **" << measure::getSystemClassName(*result.defaultClass) << "**";
    }
    text << ". " << result.renders << " renders (" << juce::String(result.renderSeconds, 0) << " s), system class "
         << juce::String(result.classifySeconds, 0) << " s, band fits " << juce::String(result.fitSeconds, 0) << " s.\n\n";
    text << "| # | Parameter | Role | Coupled | Class change | Range (texts) |\n|---|---|---|---|---|---|\n";
    for (const ParameterSurvey& survey : result.parameters)
    {
        const ContextSurvey& shown = survey.inContext ? *survey.inContext : survey.atDefault;
        juce::String range;
        if (! shown.points.empty())
        {
            range = shown.points.front().text + " ... " + shown.points.back().text;
        }
        const bool classChange = survey.atDefault.changesSystemClass || (survey.inContext && survey.inContext->changesSystemClass);
        text << "| " << (survey.index + 1) << " | " << survey.parameter.name << " | " << getRoleName(survey.role) << " | "
             << (survey.coupled ? "when " + survey.inContext->contextText : juce::String("-")) << " | " << (classChange ? "yes" : "-") << " | " << range << " |\n";
    }
    text << "\n### Evidence\n\n";
    for (const ParameterSurvey& survey : result.parameters)
    {
        text << "- **" << survey.parameter.name << "**: " << survey.summary;
        if (! survey.coupled && survey.inContext)
        {
            text << " (with " << survey.inContext->contextText << ": " << survey.inContext->detail << ")";
        }
        const ContextSurvey& shown = survey.inContext ? *survey.inContext : survey.atDefault;
        if (! shown.alsoChanges.isEmpty())
        {
            text << "; also: " << shown.alsoChanges.joinIntoString("; ");
        }
        text << "\n";
    }
    return wrapMarkdownProse(text);
}
}
