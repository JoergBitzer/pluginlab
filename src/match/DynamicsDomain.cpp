#include "pluginlab/match/DynamicsDomain.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/measure/Dynamics.h"

namespace pluginlab::match
{
namespace
{
constexpr double kHighestDbfs = 0.0;

// the stimulus through the device (the same samples on two channels), its gain envelope in dB (channel 1)
std::vector<double> getEnvelope(const measure::Device& device, const std::vector<float>& signal, double sampleRate, double latencySamples)
{
    juce::AudioBuffer<float> input(2, static_cast<int>(signal.size()));
    for (int channel = 0; channel < 2; ++channel)
    {
        input.copyFrom(channel, 0, signal.data(), static_cast<int>(signal.size()));
    }
    const juce::AudioBuffer<float> output = device(input, sampleRate);
    std::vector<double> out;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        out.push_back(output.getSample(0, sample));
    }
    return measure::getGainEnvelopeDb(std::vector<double>(signal.begin(), signal.end()), out, static_cast<int>(std::lround(latencySamples)));
}

// the envelope at a sample (aligned to the input by the lag), the last value if it is shorter
double at(const std::vector<double>& envelope, int sample)
{
    if (envelope.empty())
    {
        return 0.0;
    }
    return envelope[static_cast<size_t>(std::clamp(sample, 0, static_cast<int>(envelope.size()) - 1))];
}
}

DynamicsProbe::DynamicsProbe(const DynamicsProbeSettings& settings)
    : m_settings(settings)
{
}

std::vector<double> DynamicsProbe::measure(const measure::Device& device) const
{
    const double fs = m_settings.sampleRate;
    const int lag = static_cast<int>(std::lround(m_settings.latencySamples));
    std::vector<double> features;

    // the static curve: the mean gain of each step's last quarter
    measure::StaircaseSettings staircase;
    staircase.sampleRate = fs;
    staircase.stepSeconds = m_settings.stepSeconds;
    for (double level = m_settings.lowestDbfs; level <= kHighestDbfs + 1.0e-9; level += m_settings.stepDb)
    {
        staircase.levelsDbfs.push_back(level);
    }
    const std::vector<double> curve = getEnvelope(device, measure::makeStaircase(staircase), fs, m_settings.latencySamples);
    const int step = static_cast<int>(std::lround(m_settings.stepSeconds * fs));
    for (size_t index = 0; index < staircase.levelsDbfs.size(); ++index)
    {
        const int end = static_cast<int>(index + 1) * step;
        double sum = 0.0;
        for (int sample = end - step / 4; sample < end; ++sample)
        {
            sum += at(curve, sample + lag);
        }
        features.push_back(sum / (step / 4) / m_settings.levelJndDb);
    }

    // the step: the gain at log-spaced times after the rise and after the fall
    measure::StaircaseSettings stepSignal;
    stepSignal.sampleRate = fs;
    stepSignal.stepSeconds = m_settings.phaseSeconds;
    stepSignal.levelsDbfs = {m_settings.quietDbfs, kHighestDbfs, m_settings.quietDbfs};
    const std::vector<double> gain = getEnvelope(device, measure::makeStaircase(stepSignal), fs, m_settings.latencySamples);
    const int phase = static_cast<int>(std::lround(m_settings.phaseSeconds * fs));
    for (const int start : {phase, 2 * phase})
    {
        for (const double time : m_settings.timesSeconds)
        {
            features.push_back(at(gain, start + lag + static_cast<int>(std::lround(time * fs))) / m_settings.levelJndDb);
        }
    }
    return features;
}

juce::String DynamicsProbe::describe() const
{
    return "a 500 Hz square: a staircase every " + juce::String(m_settings.stepDb, 1) + " dB from " + juce::String(m_settings.lowestDbfs, 1) + " dBFS ("
           + juce::String(m_settings.stepSeconds, 2) + " s per step) and a step " + juce::String(m_settings.quietDbfs, 1) + " / 0 dBFS; the gain in units of "
           + juce::String(m_settings.levelJndDb, 2) + " dB";
}

std::vector<juce::String> DynamicsProbe::getFeatureNames() const
{
    std::vector<juce::String> names;
    for (double level = m_settings.lowestDbfs; level <= kHighestDbfs + 1.0e-9; level += m_settings.stepDb)
    {
        names.push_back("gain at " + juce::String(juce::roundToInt(level)) + " dBFS");
    }
    for (const juce::String phase : {"attack", "release"})
    {
        for (const double time : m_settings.timesSeconds)
        {
            names.push_back(phase + " " + juce::String(time * 1000.0, 1) + " ms");
        }
    }
    return names;
}

std::shared_ptr<WeightedDifference> makeDynamicsObjective(size_t features, double jnds)
{
    return std::make_shared<WeightedDifference>(std::vector<double>(features, 1.0), jnds);
}
}
