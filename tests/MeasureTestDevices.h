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

// A Wiener system: a linear filter, then a memoryless polynomial (W8.1)
class Wiener : public reference::Processor
{
public:
    Wiener(std::vector<reference::BiquadCoefficients> sections, std::vector<double> coefficients, double sampleRate)
        : m_filter(std::move(sections), sampleRate), m_shaper(reference::Waveshaper::makePolynomial(std::move(coefficients)))
    {
    }

    void reset() override
    {
        m_filter.reset();
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        m_filter.process(buffer);
        m_shaper.process(buffer);
    }

private:
    reference::BiquadCascade m_filter;
    reference::Waveshaper m_shaper;
};

// A generalized Hammerstein system (W8.1): y = x + G2(a2 x^2) + G3(a3 x^3), each power through a filter of its own
class GeneralizedHammerstein : public reference::Processor
{
public:
    GeneralizedHammerstein(double a2, std::vector<reference::BiquadCoefficients> second, double a3, std::vector<reference::BiquadCoefficients> third,
                           double sampleRate)
        : m_a2(a2), m_a3(a3), m_second(std::move(second), sampleRate), m_third(std::move(third), sampleRate)
    {
    }

    void reset() override
    {
        m_second.reset();
        m_third.reset();
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        juce::AudioBuffer<float> square(buffer.getNumChannels(), buffer.getNumSamples());
        juce::AudioBuffer<float> cube(buffer.getNumChannels(), buffer.getNumSamples());
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            for (int index = 0; index < buffer.getNumSamples(); ++index)
            {
                const double x = buffer.getSample(channel, index);
                square.setSample(channel, index, static_cast<float>(m_a2 * x * x));
                cube.setSample(channel, index, static_cast<float>(m_a3 * x * x * x));
            }
        }
        m_second.process(square);
        m_third.process(cube);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.addFrom(channel, 0, square, channel, 0, buffer.getNumSamples());
            buffer.addFrom(channel, 0, cube, channel, 0, buffer.getNumSamples());
        }
    }

private:
    double m_a2;
    double m_a3;
    reference::BiquadCascade m_second;
    reference::BiquadCascade m_third;
};

// A simple compressor (W8.1, "other"): an rms envelope (time constant 50 ms); above the threshold the gain reduces the level by the ratio
class Compressor : public reference::Processor
{
public:
    Compressor(double thresholdDbfs, double ratio, double sampleRate)
        : m_threshold(std::pow(10.0, thresholdDbfs / 20.0)), m_ratio(ratio), m_coefficient(std::exp(-1.0 / (0.05 * sampleRate)))
    {
    }

    void reset() override
    {
        m_power = 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer) override
    {
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            const double x = buffer.getSample(0, index);
            m_power = m_coefficient * m_power + (1.0 - m_coefficient) * x * x;
            const double level = std::sqrt(2.0 * m_power); // the amplitude of a sine with this power
            double gain = 1.0;
            if (level > m_threshold)
            {
                gain = std::pow(level / m_threshold, 1.0 / m_ratio - 1.0);
            }
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer.setSample(channel, index, static_cast<float>(gain * buffer.getSample(channel, index)));
            }
        }
    }

private:
    double m_threshold;
    double m_ratio;
    double m_coefficient;
    double m_power = 0.0;
};
}
