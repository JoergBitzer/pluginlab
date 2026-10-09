#pragma once

#include <utility>
#include <vector>

#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Nonlinear.h"

namespace pluginlab::test
{
// A Hammerstein system: a memoryless polynomial, then a linear filter (the known answer of the measurement tests for distortion followed by filtering)
class Hammerstein : public reference::Processor
{
public:
    Hammerstein(std::vector<double> coefficients, std::vector<reference::BiquadCoefficients> sections, double sampleRate)
        : m_shaper(reference::Waveshaper::makePolynomial(std::move(coefficients))), m_filter(std::move(sections), sampleRate)
    {
    }

    void reset() override
    {
        m_filter.reset();
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        m_shaper.process(buffer);
        m_filter.process(buffer);
    }

private:
    reference::Waveshaper m_shaper;
    reference::BiquadCascade m_filter;
};
}
