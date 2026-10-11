#include "pluginlab/measure/SystemClass.h"

#include <cmath>
#include <complex>
#include <limits>

#include "pluginlab/measure/Distortion.h"
#include "pluginlab/measure/SweptImpulse.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kFrequenciesPerOctave = 6.0;
constexpr double kFloorDb = -105.0;             // harmonic responses below this (re H_1) are near the noise and are not used in shape tests

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}

struct LevelResponses
{
    std::vector<std::complex<double>> linear;                    // H_1 at the grid
    std::vector<std::vector<std::complex<double>>> harmonics;    // [n - 2][grid] H_n
    std::vector<std::vector<std::complex<double>>> linearAtHarmonic; // [n - 2][grid] H_1(n f)
    double validToHz = 0.0;
};

// A device whose input starts later: silence before the signal, removed from the output again
Device makeDelayedStart(const Device& device, int samples)
{
    return [device, samples](const juce::AudioBuffer<float>& input, double sampleRate)
    {
        juce::AudioBuffer<float> longer(input.getNumChannels(), input.getNumSamples() + samples);
        longer.clear();
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
        {
            longer.copyFrom(channel, samples, input, channel, 0, input.getNumSamples());
        }
        const juce::AudioBuffer<float> output = device(longer, sampleRate);
        juce::AudioBuffer<float> trimmed(output.getNumChannels(), input.getNumSamples());
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
        {
            trimmed.copyFrom(channel, 0, output, channel, samples, input.getNumSamples());
        }
        return trimmed;
    };
}

std::vector<double> makeGrid(double lowest, double highest)
{
    std::vector<double> grid;
    for (double f = lowest; f <= highest * 1.0001; f *= std::pow(2.0, 1.0 / kFrequenciesPerOctave))
    {
        grid.push_back(f);
    }
    return grid;
}

// The full-band linear response at the given frequencies (a sweep up to 0.95 Nyquist at a low level, where the harmonics hardly alias)
std::vector<std::complex<double>> measureLinear(const Device& device, const SystemClassSettings& settings, double level, const std::vector<double>& frequencies)
{
    SweepResponseSettings sweep;
    sweep.sampleRate = settings.sampleRate;
    sweep.levelDbfs = level;
    sweep.channels = 1;
    return getHarmonicResponse(measureSweptImpulses(device, sweep), 0, 1, frequencies);
}

// H_1 and H_n at one level from the full-band sweep, evaluated only for input frequencies whose harmonics up to maximumHarmonic stay below
// Nyquist (the grid); H_1(n f) comes from the sweep at the lowest level (given), where the harmonics are smallest. (A sweep that stops at
// Nyquist / maximumHarmonic avoids aliasing completely, but its abrupt end makes the linear pulse ring into the harmonic windows: -46 dB
// "harmonics" for a pure gain.)
LevelResponses measureLevel(const Device& device, const SystemClassSettings& settings, double level, const std::vector<double>& grid,
                            const std::vector<std::vector<std::complex<double>>>& linearAtHarmonic)
{
    SweepResponseSettings sweep;
    sweep.sampleRate = settings.sampleRate;
    sweep.levelDbfs = level;
    sweep.channels = 1;
    const SweptImpulses impulses = measureSweptImpulses(device, sweep);
    LevelResponses responses;
    responses.validToHz = impulses.validToHz;
    responses.linear = getHarmonicResponse(impulses, 0, 1, grid);
    for (int order = 2; order <= settings.maximumHarmonic; ++order)
    {
        responses.harmonics.push_back(getHarmonicResponse(impulses, 0, order, grid));
    }
    responses.linearAtHarmonic = linearAtHarmonic;
    return responses;
}

// max - min of a list (0 for fewer than two values)
double getSpread(const std::vector<double>& values)
{
    if (values.size() < 2)
    {
        return 0.0;
    }
    double low = values.front();
    double high = values.front();
    for (const double value : values)
    {
        low = std::min(low, value);
        high = std::max(high, value);
    }
    return high - low;
}

bool isUsable(double harmonicFrequency, const LevelResponses& responses)
{
    return harmonicFrequency <= std::min(responses.validToHz, 20000.0);
}
}

juce::String getSystemClassName(SystemClass systemClass)
{
    switch (systemClass)
    {
        case SystemClass::Lti:
            return "LTI";
        case SystemClass::TimeVariant:
            return "time-variant";
        case SystemClass::Memoryless:
            return "memoryless non-linear";
        case SystemClass::Hammerstein:
            return "Hammerstein";
        case SystemClass::Wiener:
            return "Wiener";
        case SystemClass::GeneralizedHammerstein:
            return "generalized Hammerstein";
        case SystemClass::Other:
            return "other";
    }
    return {};
}

juce::String getSystemClassMeaning(SystemClass systemClass)
{
    switch (systemClass)
    {
        case SystemClass::Lti:
            return "frequency response, phase, the band model and the null test are valid";
        case SystemClass::TimeVariant:
            return "the frequency response is an average; harmonic responses are not meaningful";
        case SystemClass::Memoryless:
            return "a curve without filtering: the harmonics do not depend on the frequency";
        case SystemClass::Hammerstein:
            return "a curve, then a filter: the frequency response describes the filter";
        case SystemClass::Wiener:
            return "a filter, then a curve: the distortion depends on the frequency through the filter";
        case SystemClass::GeneralizedHammerstein:
            return "parallel curves with their own filters: every harmonic has its own frequency response";
        case SystemClass::Other:
            return "none of the simple models (e.g. dynamics): measurements at one level describe only that level";
    }
    return {};
}

SystemClassResult classifySystem(const Device& device, const SystemClassSettings& settings)
{
    SystemClassResult result;
    result.settings = settings;
    SystemClassEvidence& evidence = result.evidence;
    const double bandEdge = std::min(settings.highestHz, 0.9 * 0.5 * settings.sampleRate / settings.maximumHarmonic);
    const std::vector<double> grid = makeGrid(settings.lowestHz, bandEdge);
    std::vector<std::vector<std::complex<double>>> linearAtHarmonic;
    for (int order = 2; order <= settings.maximumHarmonic; ++order)
    {
        std::vector<double> atHarmonic;
        for (const double f : grid)
        {
            atHarmonic.push_back(std::min(order * f, 0.45 * settings.sampleRate));
        }
        linearAtHarmonic.push_back(measureLinear(device, settings, settings.levelsDbfs.front(), atHarmonic));
    }
    std::vector<LevelResponses> levels;
    for (const double level : settings.levelsDbfs)
    {
        levels.push_back(measureLevel(device, settings, level, grid, linearAtHarmonic));
    }
    const size_t middle = levels.size() / 2;
    const size_t highest = levels.size() - 1;

    // 1. time variance: the middle level again, the sweep starting later within the render
    const LevelResponses offset = measureLevel(makeDelayedStart(device, static_cast<int>(std::round(settings.offsetSeconds * settings.sampleRate))), settings,
                                               settings.levelsDbfs[middle], grid, linearAtHarmonic);
    for (size_t index = 0; index < grid.size(); ++index)
    {
        evidence.timeVarianceDb = std::max(evidence.timeVarianceDb, std::abs(toDb(std::abs(offset.linear[index])) - toDb(std::abs(levels[middle].linear[index]))));
    }

    // the harmonics, the level dependence of H_1 and what odd harmonics allow for it (a polynomial: |delta H_1| <= 3 |H_3| + 10 |H_5|)
    double allowed = 0.0;
    for (size_t level = 0; level < levels.size(); ++level)
    {
        for (size_t order = 0; order < levels[level].harmonics.size(); ++order)
        {
            for (size_t index = 0; index < grid.size(); ++index)
            {
                if (! isUsable((order + 2) * grid[index], levels[level]))
                {
                    continue;
                }
                const double ratio = std::abs(levels[level].harmonics[order][index]) / std::abs(levels[level].linear[index]);
                evidence.largestHarmonicDb = std::max(evidence.largestHarmonicDb, toDb(ratio));
            }
        }
    }
    // what a curve's odd harmonics allow: |delta H_1| <= 3 |H_3| + 10 |H_5| (a polynomial's cubic and fifth-order terms), with the largest ratios
    // over frequency (a filter after the curve lowers H_3 at 3 f relative to H_1 at f)
    double largestThird = 0.0;
    double largestFifth = 0.0;
    double largestChange = 0.0;
    for (size_t index = 0; index < grid.size(); ++index)
    {
        const double low = std::abs(levels.front().linear[index]);
        const double high = std::abs(levels[highest].linear[index]);
        evidence.linearLevelSpreadDb = std::max(evidence.linearLevelSpreadDb, std::abs(toDb(high / low)));
        largestChange = std::max(largestChange, std::abs(high / low - 1.0));
        const size_t third = 1;
        const size_t fifth = 3;
        if (third < levels[highest].harmonics.size() && isUsable(3.0 * grid[index], levels[highest]))
        {
            largestThird = std::max(largestThird, std::abs(levels[highest].harmonics[third][index]) / high);
        }
        if (fifth < levels[highest].harmonics.size() && isUsable(5.0 * grid[index], levels[highest]))
        {
            largestFifth = std::max(largestFifth, std::abs(levels[highest].harmonics[fifth][index]) / high);
        }
    }
    allowed = 3.0 * largestThird + 10.0 * largestFifth;
    evidence.levelChangeBoundDb = toDb(1.0 + allowed);

    // significant harmonics at the highest level
    for (size_t order = 0; order < levels[highest].harmonics.size(); ++order)
    {
        for (size_t index = 0; index < grid.size(); ++index)
        {
            if (isUsable((order + 2) * grid[index], levels[highest])
                && toDb(std::abs(levels[highest].harmonics[order][index]) / std::abs(levels[highest].linear[index])) > settings.harmonicLimitDb)
            {
                evidence.significantOrders.push_back(static_cast<int>(order + 2));
                break;
            }
        }
    }

    // shape tests over frequency for every significant order (points below the noise floor are left out)
    std::vector<double> linearDb;
    for (size_t index = 0; index < grid.size(); ++index)
    {
        linearDb.push_back(toDb(std::abs(levels[highest].linear[index])));
    }
    evidence.linearFlatnessDb = getSpread(linearDb);
    // the lowest level at which every significant order is above the limit somewhere (the leading order dominates there): for the Wiener test
    size_t wienerLevel = highest;
    for (size_t level = 0; level < levels.size(); ++level)
    {
        bool allAbove = ! evidence.significantOrders.empty();
        for (const int order : evidence.significantOrders)
        {
            bool above = false;
            for (size_t index = 0; index < grid.size(); ++index)
            {
                const auto o = static_cast<size_t>(order - 2);
                above = above || (isUsable(order * grid[index], levels[level])
                                  && toDb(std::abs(levels[level].harmonics[o][index]) / std::abs(levels[level].linear[index])) > settings.harmonicLimitDb);
            }
            allAbove = allAbove && above;
        }
        if (allAbove)
        {
            wienerLevel = level;
            break;
        }
    }
    for (const int order : evidence.significantOrders)
    {
        const auto o = static_cast<size_t>(order - 2);
        std::vector<double> flat;
        std::vector<double> hammerstein;
        std::vector<double> wiener;
        std::vector<double> shape;
        for (size_t index = 0; index < grid.size(); ++index)
        {
            if (! isUsable(order * grid[index], levels[highest]))
            {
                continue;
            }
            const double harmonic = std::abs(levels[highest].harmonics[o][index]);
            if (toDb(harmonic / std::abs(levels[highest].linear[index])) < kFloorDb)
            {
                continue;
            }
            flat.push_back(toDb(harmonic));
            hammerstein.push_back(toDb(harmonic / std::abs(levels[highest].linearAtHarmonic[o][index])));
            // the Wiener test at its own level, and only where the harmonic is above the noise there
            const double lowHarmonic = std::abs(levels[wienerLevel].harmonics[o][index]);
            if (toDb(lowHarmonic / std::abs(levels[wienerLevel].linear[index])) > kFloorDb)
            {
                wiener.push_back(toDb(lowHarmonic / std::pow(std::abs(levels[wienerLevel].linear[index]), order)));
            }
            if (highest > 0)
            {
                const double previous = std::abs(levels[highest - 1].harmonics[o][index]);
                if (toDb(previous / std::abs(levels[highest - 1].linear[index])) > kFloorDb)
                {
                    shape.push_back(toDb(harmonic / previous));
                }
            }
        }
        evidence.harmonicFlatnessDb = std::max(evidence.harmonicFlatnessDb, getSpread(flat));
        evidence.hammersteinSpreadDb = std::max(evidence.hammersteinSpreadDb, getSpread(hammerstein));
        evidence.wienerSpreadDb = std::max(evidence.wienerSpreadDb, getSpread(wiener));
        evidence.shapeSpreadDb = std::max(evidence.shapeSpreadDb, getSpread(shape));
    }

    // the decision, in this order
    if (evidence.timeVarianceDb > settings.timeVarianceDb)
    {
        result.systemClass = SystemClass::TimeVariant;
        result.reason = "the same sweep starting later gives a different H_1 (" + juce::String(evidence.timeVarianceDb, 2) + " dB)";
        return result;
    }
    if (evidence.largestHarmonicDb < settings.harmonicLimitDb && evidence.linearLevelSpreadDb < settings.linearToleranceDb)
    {
        result.systemClass = SystemClass::Lti;
        result.reason = "no harmonic above " + juce::String(settings.harmonicLimitDb, 0) + " dB and H_1 the same at every level";
        return result;
    }
    if (toDb(1.0 + largestChange) - evidence.levelChangeBoundDb > settings.linearToleranceDb)
    {
        result.systemClass = SystemClass::Other;
        result.reason = "H_1 changes with the level (" + juce::String(evidence.linearLevelSpreadDb, 2)
                        + " dB) more than the odd harmonics of a curve explain: level-dependent gain (dynamics)";
        return result;
    }
    if (evidence.significantOrders.empty())
    {
        result.systemClass = SystemClass::Other;
        result.reason = "H_1 depends on the level although no harmonic is significant";
        return result;
    }
    if (evidence.linearFlatnessDb < settings.linearToleranceDb && evidence.harmonicFlatnessDb < settings.flatToleranceDb)
    {
        result.systemClass = SystemClass::Memoryless;
        result.reason = "H_1 and every significant H_n are flat over frequency";
        return result;
    }
    if (evidence.hammersteinSpreadDb < settings.flatToleranceDb)
    {
        result.systemClass = SystemClass::Hammerstein;
        result.reason = "every H_n(f) / H_1(n f) is constant: one filter after the curve";
        return result;
    }
    if (evidence.wienerSpreadDb < settings.wienerToleranceDb)
    {
        result.systemClass = SystemClass::Wiener;
        result.reason = "every H_n(f) / H_1(f)^n is constant: one filter before the curve";
        return result;
    }
    if (evidence.shapeSpreadDb < settings.shapeToleranceDb)
    {
        result.systemClass = SystemClass::GeneralizedHammerstein;
        result.reason = "the harmonic responses differ, but each keeps its shape across levels (leading order)";
        return result;
    }
    result.systemClass = SystemClass::Other;
    result.reason = "the harmonic responses change their shape with the level";
    return result;
}
}
