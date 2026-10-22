#include "pluginlab/reference/SchroederReverb.h"

#include <cmath>

namespace pluginlab::reference
{
SchroederReverb::SchroederReverb(const SchroederReverbSettings& settings)
    : m_settings(settings)
{
}

double SchroederReverb::getCombGain(size_t comb) const
{
    return std::pow(10.0, -3.0 * m_settings.combDelays[comb] / (m_settings.t60Seconds * m_settings.sampleRate));
}

void SchroederReverb::reset()
{
    for (std::vector<std::vector<Line>>* lines : {&m_combs, &m_allPasses})
    {
        for (std::vector<Line>& channel : *lines)
        {
            for (Line& line : channel)
            {
                std::fill(line.buffer.begin(), line.buffer.end(), 0.0);
                line.position = 0;
            }
        }
    }
}

void SchroederReverb::process(juce::AudioBuffer<float>& buffer)
{
    const size_t channels = static_cast<size_t>(buffer.getNumChannels());
    if (m_combs.size() != channels)
    {
        m_combs.assign(channels, {});
        m_allPasses.assign(channels, {});
        for (size_t channel = 0; channel < channels; ++channel)
        {
            for (const int delay : m_settings.combDelays)
            {
                m_combs[channel].push_back({std::vector<double>(static_cast<size_t>(delay), 0.0), 0});
            }
            for (const int delay : m_settings.allPassDelays)
            {
                m_allPasses[channel].push_back({std::vector<double>(static_cast<size_t>(delay), 0.0), 0});
            }
        }
    }
    for (size_t channel = 0; channel < channels; ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const double input = buffer.getSample(static_cast<int>(channel), sample);
            // the combs: y = x(n - D) + g y(n - D), read before write
            double sum = 0.0;
            for (size_t comb = 0; comb < m_combs[channel].size(); ++comb)
            {
                Line& line = m_combs[channel][comb];
                const double delayed = line.buffer[line.position];
                line.buffer[line.position] = input + getCombGain(comb) * delayed;
                line.position = (line.position + 1) % line.buffer.size();
                sum += delayed;
            }
            // the all-passes (Schroeder's form): v = x + a v(n - D), y = -a v + v(n - D)
            double value = m_settings.wetGain * sum;
            for (Line& line : m_allPasses[channel])
            {
                const double delayed = line.buffer[line.position];
                const double fed = value + m_settings.allPassGain * delayed;
                line.buffer[line.position] = fed;
                line.position = (line.position + 1) % line.buffer.size();
                value = -m_settings.allPassGain * fed + delayed;
            }
            buffer.setSample(static_cast<int>(channel), sample, static_cast<float>(value));
        }
    }
}
}
