#include "pluginlab/reference/Biquad.h"

#include "ReferenceMath.h"

namespace pluginlab::reference
{
BiquadCoefficients makeBiquad(double b0, double b1, double b2, double a0, double a1, double a2)
{
    BiquadCoefficients section;
    section.b0 = b0 / a0;
    section.b1 = b1 / a0;
    section.b2 = b2 / a0;
    section.a1 = a1 / a0;
    section.a2 = a2 / a0;
    return section;
}

std::complex<double> getBiquadResponse(const BiquadCoefficients& section, double frequencyHz, double sampleRate)
{
    const std::complex<double> inverseZ = detail::getInverseZ(frequencyHz, sampleRate);
    const double numerator[] = {section.b0, section.b1, section.b2};
    const double denominator[] = {1.0, section.a1, section.a2};
    return detail::evaluatePolynomial(numerator, 3, inverseZ) / detail::evaluatePolynomial(denominator, 3, inverseZ);
}

std::complex<double> getCascadeResponse(const std::vector<BiquadCoefficients>& sections, double frequencyHz, double sampleRate)
{
    std::complex<double> response = 1.0;
    for (const BiquadCoefficients& section : sections)
    {
        response *= getBiquadResponse(section, frequencyHz, sampleRate);
    }
    return response;
}

bool isStable(const BiquadCoefficients& section)
{
    return std::abs(section.a2) < 1.0 && std::abs(section.a1) < 1.0 + section.a2;
}

BiquadCascade::BiquadCascade(std::vector<BiquadCoefficients> sections, double sampleRate)
    : LinearProcessor(sampleRate)
    , m_sections(std::move(sections))
{
    setNumChannels(1);
}

void BiquadCascade::setSections(std::vector<BiquadCoefficients> sections)
{
    m_sections = std::move(sections);
    for (std::vector<SectionState>& channelState : m_state)
    {
        channelState.resize(m_sections.size());
    }
}

const std::vector<BiquadCoefficients>& BiquadCascade::getSections() const
{
    return m_sections;
}

void BiquadCascade::reset()
{
    prepareChannels(static_cast<int>(m_state.size()));
}

void BiquadCascade::prepareChannels(int channels)
{
    m_state.assign(static_cast<size_t>(channels), std::vector<SectionState>(m_sections.size()));
}

double BiquadCascade::processSample(int channel, double input)
{
    std::vector<SectionState>& channelState = m_state[static_cast<size_t>(channel)];
    double signal = input;
    for (size_t index = 0; index < m_sections.size(); ++index)
    {
        const BiquadCoefficients& section = m_sections[index];
        SectionState& state = channelState[index];
        const double output = section.b0 * signal + state.z1;
        state.z1 = section.b1 * signal - section.a1 * output + state.z2;
        state.z2 = section.b2 * signal - section.a2 * output;
        signal = output;
    }
    return signal;
}

std::complex<double> BiquadCascade::getResponse(double frequencyHz) const
{
    return getCascadeResponse(m_sections, frequencyHz, getSampleRate());
}
}
