#include "pluginlab/match/EqDomain.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/measure/FrequencyResponse.h"
#include "pluginlab/measure/SweptImpulse.h"

namespace pluginlab::match
{
namespace
{
constexpr double kFloor = 1.0e-15;
constexpr double kHighestShare = 0.45; // of the sample rate: the grid ends there at the latest

double toDb(std::complex<double> value)
{
    return 20.0 * std::log10(std::max(std::abs(value), kFloor));
}
}

MagnitudeProbe::MagnitudeProbe(const MagnitudeProbeSettings& settings)
    : m_settings(settings),
      m_frequencies(eqmodel::makeLogGrid(settings.lowestHz, std::min(settings.highestHz, kHighestShare * settings.sampleRate), settings.perOctave))
{
}

std::vector<double> MagnitudeProbe::measure(const measure::Device& device) const
{
    if (m_settings.stimulus == ProbeStimulus::Impulse)
    {
        // an impulse of the sweep's peak level; H(f) = DTFT of the output / the impulse's amplitude (the delay only turns the phase)
        const double amplitude = std::pow(10.0, m_settings.levelDbfs / 20.0);
        const int length = static_cast<int>(m_settings.impulseSeconds * m_settings.sampleRate);
        juce::AudioBuffer<float> input(1, length);
        input.clear();
        input.setSample(0, 0, static_cast<float>(amplitude));
        const juce::AudioBuffer<float> output = device(input, m_settings.sampleRate);
        if (output.getNumChannels() == 0 || output.getNumSamples() == 0)
        {
            return std::vector<double>(m_frequencies.size(), m_settings.floorDb);
        }
        const std::vector<double> response(output.getReadPointer(0), output.getReadPointer(0) + output.getNumSamples());
        std::vector<double> magnitude;
        for (const double frequency : m_frequencies)
        {
            magnitude.push_back(std::max(toDb(measure::getDtft(response, 0, frequency, m_settings.sampleRate) / amplitude), m_settings.floorDb));
        }
        return magnitude;
    }
    measure::SweepResponseSettings sweep;
    sweep.sampleRate = m_settings.sampleRate;
    sweep.channels = 1;
    sweep.levelDbfs = m_settings.levelDbfs;
    sweep.approximateSeconds = m_settings.sweepSeconds;
    sweep.frequencies = m_frequencies;
    sweep.windowAfterSeconds = m_settings.windowSeconds;
    const measure::FrequencyResponse response = measure::measureSweptResponse(device, sweep);
    std::vector<double> magnitude;
    if (response.channels.empty())
    {
        return std::vector<double>(m_frequencies.size(), m_settings.floorDb);
    }
    for (const std::complex<double>& value : response.channels[0].response)
    {
        magnitude.push_back(std::max(toDb(value), m_settings.floorDb));
    }
    return magnitude;
}

juce::String MagnitudeProbe::describe() const
{
    if (m_settings.stimulus == ProbeStimulus::Impulse)
    {
        return "magnitude in dB, " + juce::String(juce::roundToInt(m_frequencies.front())) + " Hz ... " + juce::String(juce::roundToInt(m_frequencies.back()))
               + " Hz, " + juce::String(m_settings.perOctave, 0) + " points per octave (impulse of " + juce::String(m_settings.levelDbfs, 0) + " dBFS, "
               + juce::String(m_settings.impulseSeconds, 2) + " s of response, " + juce::String(m_settings.sampleRate / 1000.0, 1) + " kHz; levels below "
               + juce::String(m_settings.floorDb, 0) + " dB clamped)";
    }
    return "magnitude in dB, " + juce::String(juce::roundToInt(m_frequencies.front())) + " Hz ... " + juce::String(juce::roundToInt(m_frequencies.back()))
           + " Hz, " + juce::String(m_settings.perOctave, 0) + " points per octave (synchronized sweep, " + juce::String(m_settings.sweepSeconds, 2) + " s, "
           + juce::String(m_settings.levelDbfs, 0) + " dBFS, " + juce::String(m_settings.sampleRate / 1000.0, 1) + " kHz; levels below "
           + juce::String(m_settings.floorDb, 0) + " dB clamped)";
}

std::vector<double> getCurveMagnitude(const eqmodel::EqModel& model, const std::vector<double>& frequencies, double sampleRate, double floorDb)
{
    std::vector<double> magnitude;
    for (const double frequency : frequencies)
    {
        magnitude.push_back(std::max(toDb(eqmodel::getModelResponse(model, frequency, sampleRate)), floorDb));
    }
    return magnitude;
}

std::shared_ptr<WeightedDifference> makeMagnitudeObjective(size_t features, double toleranceDb)
{
    return std::make_shared<WeightedDifference>(std::vector<double>(features, 1.0), toleranceDb);
}

double getToleranceDb(MatchTolerance tolerance)
{
    switch (tolerance)
    {
        case MatchTolerance::Exact:
            return 0.01;
        case MatchTolerance::Strict:
            return 0.1;
        case MatchTolerance::Medium:
            return 0.25;
        case MatchTolerance::Loose:
            return 0.5;
    }
    return 0.1;
}

const char* getToleranceName(MatchTolerance tolerance)
{
    switch (tolerance)
    {
        case MatchTolerance::Exact:
            return "exact (0.01 dB)";
        case MatchTolerance::Strict:
            return "strict (0.1 dB)";
        case MatchTolerance::Medium:
            return "medium (0.25 dB)";
        case MatchTolerance::Loose:
            return "loose (0.5 dB)";
    }
    return "";
}

std::optional<MatchTolerance> parseTolerance(const juce::String& name)
{
    const juce::String word = name.trim().toLowerCase().upToFirstOccurrenceOf(" ", false, false);
    for (const MatchTolerance tolerance : {MatchTolerance::Exact, MatchTolerance::Strict, MatchTolerance::Medium, MatchTolerance::Loose})
    {
        if (juce::String(getToleranceName(tolerance)).startsWith(word) && word.isNotEmpty())
        {
            return tolerance;
        }
    }
    return std::nullopt;
}

std::shared_ptr<WeightedDifference> makeMagnitudeObjective(size_t features, MatchTolerance tolerance)
{
    return makeMagnitudeObjective(features, getToleranceDb(tolerance));
}
}

