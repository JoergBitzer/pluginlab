#include "pluginlab/reference/FdnReverb.h"

#include <algorithm>
#include <cmath>

#include <juce_core/juce_core.h>

namespace pluginlab::reference
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kLagrangeTaps = 4;
constexpr int kMargin = 4;
constexpr double kRateSpread = 0.15; // the lines' LFO rates spread by +-15 % around the rate
}

FdnReverb::FdnReverb(const FdnReverbSettings& settings)
    : m_settings(settings)
{
    const int longest = *std::max_element(settings.delays.begin(), settings.delays.end());
    m_size = static_cast<size_t>(longest + static_cast<int>(std::ceil(settings.lfoDepthSamples)) + kMargin);
    m_lines.assign(settings.delays.size(), std::vector<double>(m_size, 0.0));
    m_preDelay.assign(static_cast<size_t>(std::max(1L, std::lround(settings.preDelaySeconds * settings.sampleRate))) + 1, 0.0);
    juce::Random random(settings.seed);
    const size_t count = settings.delays.size();
    for (size_t line = 0; line < count; ++line)
    {
        m_lfoPhases.push_back(2.0 * kPi * random.nextDouble());
        double spread = 0.0;
        if (count > 1)
        {
            spread = (static_cast<double>(line) / static_cast<double>(count - 1) - 0.5) * 2.0 * kRateSpread;
        }
        m_lfoRates.push_back(settings.lfoRateHz * (1.0 + spread));
    }
}

double FdnReverb::getLineGain(size_t line) const
{
    return std::pow(10.0, -3.0 * m_settings.delays[line] / (m_settings.t60Seconds * m_settings.sampleRate));
}

void FdnReverb::reset()
{
    for (std::vector<double>& line : m_lines)
    {
        std::fill(line.begin(), line.end(), 0.0);
    }
    std::fill(m_preDelay.begin(), m_preDelay.end(), 0.0);
    m_position = 0;
    m_preDelayPosition = 0;
    m_sample = 0;
}

// the line's sample `delay` samples back (fractional: Lagrange of order 3 around it), read before this sample is written
double FdnReverb::readLine(size_t line, double delay) const
{
    const std::vector<double>& buffer = m_lines[line];
    const int integer = static_cast<int>(std::floor(delay));
    const double d = 1.0 + delay - integer;
    double value = 0.0;
    for (int k = 0; k < kLagrangeTaps; ++k)
    {
        double weight = 1.0;
        for (int n = 0; n < kLagrangeTaps; ++n)
        {
            if (n != k)
            {
                weight *= (d - n) / (k - n);
            }
        }
        const size_t back = static_cast<size_t>(integer - 1 + k);
        value += weight * buffer[(m_position + m_size - back) % m_size];
    }
    return value;
}

void FdnReverb::process(juce::AudioBuffer<float>& buffer)
{
    const size_t count = m_lines.size();
    std::vector<double> outputs(count);
    const double fs = m_settings.sampleRate;
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // the input: the mean of the channels, through the pre-delay
        double input = 0.0;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            input += buffer.getSample(channel, sample);
        }
        input /= std::max(1, buffer.getNumChannels());
        const double delayed = m_preDelay[m_preDelayPosition];
        m_preDelay[m_preDelayPosition] = input;
        m_preDelayPosition = (m_preDelayPosition + 1) % m_preDelay.size();

        // the lines' outputs (with their gains), the Householder matrix, back into the lines with the input
        double sum = 0.0;
        for (size_t line = 0; line < count; ++line)
        {
            double delay = m_settings.delays[line];
            if (m_settings.lfoDepthSamples > 0.0)
            {
                delay += m_settings.lfoDepthSamples * std::sin(m_lfoPhases[line] + 2.0 * kPi * m_lfoRates[line] * static_cast<double>(m_sample) / fs);
            }
            outputs[line] = getLineGain(line) * readLine(line, delay);
            sum += outputs[line];
        }
        const double householder = 2.0 / static_cast<double>(count) * sum;
        double left = 0.0;
        double right = 0.0;
        for (size_t line = 0; line < count; ++line)
        {
            m_lines[line][m_position] = outputs[line] - householder + delayed;
            // the outputs with signs: left + - + - ..., right + + - - ...
            double leftSign = 1.0;
            if (line % 2 == 1)
            {
                leftSign = -1.0;
            }
            double rightSign = 1.0;
            if ((line / 2) % 2 == 1)
            {
                rightSign = -1.0;
            }
            left += leftSign * outputs[line];
            right += rightSign * outputs[line];
        }
        m_position = (m_position + 1) % m_size;
        ++m_sample;
        const double scale = m_settings.wetGain / std::sqrt(static_cast<double>(count));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            double value = left;
            if (channel == 1)
            {
                value = right;
            }
            buffer.setSample(channel, sample, static_cast<float>(scale * value));
        }
    }
}
}
