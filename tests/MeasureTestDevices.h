#pragma once

#include <cmath>
#include <memory>
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

// Frequency-dependent crosstalk (as from a capacitance): each channel receives the other one through a filter, scaled: L' = L + k F(R), R' = R + k F(L)
class FilteredCrosstalk : public reference::Processor
{
public:
    FilteredCrosstalk(double factor, std::vector<reference::BiquadCoefficients> sections, double sampleRate)
        : m_factor(factor), m_filter(std::move(sections), sampleRate)
    {
    }

    void reset() override
    {
        m_filter.reset();
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        if (buffer.getNumChannels() < 2)
        {
            return;
        }
        juce::AudioBuffer<float> swapped(2, buffer.getNumSamples());
        swapped.copyFrom(0, 0, buffer, 1, 0, buffer.getNumSamples());
        swapped.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
        m_filter.process(swapped);
        buffer.addFrom(0, 0, swapped, 0, 0, buffer.getNumSamples(), static_cast<float>(m_factor));
        buffer.addFrom(1, 0, swapped, 1, 0, buffer.getNumSamples(), static_cast<float>(m_factor));
    }

private:
    double m_factor;
    reference::BiquadCascade m_filter;
};

// Processors in series
class Chain : public reference::Processor
{
public:
    void add(std::unique_ptr<reference::Processor> processor)
    {
        m_processors.push_back(std::move(processor));
    }

    void reset() override
    {
        for (auto& processor : m_processors)
        {
            processor->reset();
        }
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        for (auto& processor : m_processors)
        {
            processor->process(buffer);
        }
    }

private:
    std::vector<std::unique_ptr<reference::Processor>> m_processors;
};

// Integer-style overflow: values beyond +-1 wrap around to the other end (y = x - 2 for 1 <= x < 3, ...): the "rollover" of AES17 6.6.8
class Wraparound : public reference::Processor
{
public:
    void reset() override
    {
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int index = 0; index < buffer.getNumSamples(); ++index)
            {
                const double value = data[index];
                data[index] = static_cast<float>(value - 2.0 * std::floor((value + 1.0) / 2.0));
            }
        }
    }
};
}
