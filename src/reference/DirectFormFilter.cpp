#include "pluginlab/reference/DirectFormFilter.h"

#include <algorithm>

#include "ReferenceMath.h"

namespace pluginlab::reference
{
DirectFormFilter::DirectFormFilter(std::vector<double> numerator, std::vector<double> denominator, double sampleRate, int delaySamples,
                                   int latencySamples)
    : LinearProcessor(sampleRate)
    , m_numerator(std::move(numerator))
    , m_denominator(std::move(denominator))
    , m_delaySamples(delaySamples)
    , m_latencySamples(latencySamples)
{
    // normalise to a0 = 1 and make both polynomials the same length
    const double a0 = m_denominator.front();
    for (double& coefficient : m_numerator)
    {
        coefficient /= a0;
    }
    for (double& coefficient : m_denominator)
    {
        coefficient /= a0;
    }
    const size_t length = std::max(m_numerator.size(), m_denominator.size());
    m_numerator.resize(length, 0.0);
    m_denominator.resize(length, 0.0);
    setNumChannels(1);
}

const std::vector<double>& DirectFormFilter::getNumerator() const
{
    return m_numerator;
}

const std::vector<double>& DirectFormFilter::getDenominator() const
{
    return m_denominator;
}

int DirectFormFilter::getDelaySamples() const
{
    return m_delaySamples;
}

int DirectFormFilter::getLatencySamples() const
{
    return m_latencySamples;
}

void DirectFormFilter::reset()
{
    prepareChannels(static_cast<int>(m_state.size()));
}

void DirectFormFilter::prepareChannels(int channels)
{
    ChannelState cleared;
    cleared.delayLine.assign(static_cast<size_t>(m_delaySamples), 0.0);
    cleared.registers.assign(m_numerator.size(), 0.0);
    m_state.assign(static_cast<size_t>(channels), cleared);
}

double DirectFormFilter::processSample(int channel, double input)
{
    ChannelState& state = m_state[static_cast<size_t>(channel)];
    double delayed = input;
    if (m_delaySamples > 0)
    {
        delayed = state.delayLine[static_cast<size_t>(state.delayPosition)];
        state.delayLine[static_cast<size_t>(state.delayPosition)] = input;
        state.delayPosition = (state.delayPosition + 1) % m_delaySamples;
    }
    // transposed direct form II: registers[i] holds the part of the output i samples ahead
    const size_t order = m_numerator.size() - 1;
    const double output = m_numerator[0] * delayed + state.registers[0];
    for (size_t index = 0; index < order; ++index)
    {
        state.registers[index] = m_numerator[index + 1] * delayed - m_denominator[index + 1] * output + state.registers[index + 1];
    }
    return output;
}

std::complex<double> DirectFormFilter::getResponse(double frequencyHz) const
{
    const std::complex<double> inverseZ = detail::getInverseZ(frequencyHz, getSampleRate());
    const int length = static_cast<int>(m_numerator.size());
    const std::complex<double> ratio = detail::evaluatePolynomial(m_numerator.data(), length, inverseZ)
                                       / detail::evaluatePolynomial(m_denominator.data(), length, inverseZ);
    return ratio * std::pow(inverseZ, m_delaySamples);
}
}
