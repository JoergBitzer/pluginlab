#include "pluginlab/match/EqDomain.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/measure/FrequencyResponse.h"

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
}
