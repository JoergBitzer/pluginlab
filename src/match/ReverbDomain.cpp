#include "pluginlab/match/ReverbDomain.h"

#include <cmath>

#include "pluginlab/measure/Reverb.h"

namespace pluginlab::match
{
namespace
{
constexpr double kShortestDecay = 0.01; // seconds: a decay the analysis did not find counts as this (ln of 0 would be infinite)
}

ReverbProbe::ReverbProbe(const ReverbProbeSettings& settings)
    : m_settings(settings)
{
}

std::vector<double> ReverbProbe::measure(const measure::Device& device) const
{
    const double fs = m_settings.sampleRate;
    const int length = static_cast<int>(m_settings.renderSeconds * fs);
    const double height = std::pow(10.0, m_settings.levelDbfs / 20.0);
    juce::AudioBuffer<float> input(2, length);
    input.clear();
    input.setSample(0, 0, static_cast<float>(height));
    input.setSample(1, 0, static_cast<float>(height));
    const juce::AudioBuffer<float> output = device(input, fs);
    std::vector<double> response;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        response.push_back(output.getSample(0, sample) / height);
    }
    measure::ReverbSettings settings;
    settings.sampleRate = fs;
    settings.latencySamples = m_settings.latencySamples;
    settings.thirdOctaves = false;
    const measure::ReverbAnalysis analysis = measure::analyseReverb(response, settings);
    const double unit = std::log(1.0 + m_settings.decayJnd);
    std::vector<double> features;
    for (const measure::BandDecay& band : analysis.octaves)
    {
        features.push_back(std::log(std::max(band.times.t30, kShortestDecay)) / unit);
    }
    features.push_back(std::log(std::max(analysis.broadband.edt, kShortestDecay)) / unit);
    features.push_back(1000.0 * analysis.preDelaySeconds / m_settings.preDelayJndMs);
    features.push_back(analysis.c80Db / m_settings.c80JndDb);
    return features;
}

juce::String ReverbProbe::describe() const
{
    return "impulse, " + juce::String(m_settings.renderSeconds, 1) + " s; T30 per octave and EDT in units of " + juce::String(100.0 * m_settings.decayJnd, 0)
           + " %, pre-delay in units of " + juce::String(m_settings.preDelayJndMs, 1) + " ms, C80 in units of " + juce::String(m_settings.c80JndDb, 1) + " dB";
}

std::vector<juce::String> ReverbProbe::getFeatureNames() const
{
    std::vector<juce::String> names;
    for (const measure::Band& band : measure::getBands(1, 125.0, 8000.0))
    {
        names.push_back("T30 " + juce::String(juce::roundToInt(band.centreHz)) + " Hz");
    }
    names.push_back("EDT");
    names.push_back("pre-delay");
    names.push_back("C80");
    return names;
}

std::shared_ptr<WeightedDifference> makeReverbObjective(size_t features, double jnds)
{
    return std::make_shared<WeightedDifference>(std::vector<double>(features, 1.0), jnds);
}
}
