#include "pluginlab/reference/Nonlinear.h"

#include <algorithm>
#include <cmath>

#include "ReferenceMath.h"

namespace pluginlab::reference
{
namespace
{
double getBinomial(int n, int k)
{
    double value = 1.0;
    for (int index = 1; index <= k; ++index)
    {
        value *= static_cast<double>(n - k + index) / index;
    }
    return value;
}
}

Waveshaper::Waveshaper(std::function<double(double)> curve)
    : m_curve(std::move(curve))
{
}

Waveshaper Waveshaper::makePolynomial(std::vector<double> coefficients)
{
    return Waveshaper([coefficients](double x)
                      {
                          // Horner
                          double y = 0.0;
                          for (auto coefficient = coefficients.rbegin(); coefficient != coefficients.rend(); ++coefficient)
                          {
                              y = y * x + *coefficient;
                          }
                          return y;
                      });
}

Waveshaper Waveshaper::makeHardClip(double threshold)
{
    return Waveshaper([threshold](double x)
                      {
                          return std::clamp(x, -threshold, threshold);
                      });
}

Waveshaper Waveshaper::makeSoftClip(double drive)
{
    return Waveshaper([drive](double x)
                      {
                          return std::tanh(drive * x);
                      });
}

double Waveshaper::processSample(double input) const
{
    return m_curve(input);
}

void Waveshaper::reset()
{
}

void Waveshaper::process(juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* data = buffer.getWritePointer(channel);
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            data[index] = static_cast<float>(m_curve(data[index]));
        }
    }
}

std::vector<double> getShaperHarmonics(const Waveshaper& shaper, double amplitude, int count, int points)
{
    std::vector<double> sines(static_cast<size_t>(count + 1), 0.0);
    std::vector<double> cosines(static_cast<size_t>(count + 1), 0.0);
    for (int point = 0; point < points; ++point)
    {
        const double angle = detail::kTwoPi * (point + 0.5) / points;
        const double y = shaper.processSample(amplitude * std::sin(angle));
        for (int harmonic = 0; harmonic <= count; ++harmonic)
        {
            sines[static_cast<size_t>(harmonic)] += y * std::sin(harmonic * angle);
            cosines[static_cast<size_t>(harmonic)] += y * std::cos(harmonic * angle);
        }
    }
    std::vector<double> amplitudes(static_cast<size_t>(count + 1));
    amplitudes[0] = cosines[0] / points;
    for (int harmonic = 1; harmonic <= count; ++harmonic)
    {
        amplitudes[static_cast<size_t>(harmonic)] = 2.0 / points * std::hypot(sines[static_cast<size_t>(harmonic)], cosines[static_cast<size_t>(harmonic)]);
    }
    return amplitudes;
}

std::vector<double> getPolynomialHarmonics(const std::vector<double>& coefficients, double amplitude, int count)
{
    // the cosine coefficients for x = A cos(wt) (for A sin(wt) the magnitudes are the same); the DC term keeps its sign
    std::vector<double> components(static_cast<size_t>(count + 1), 0.0);
    for (int power = 0; power < static_cast<int>(coefficients.size()); ++power)
    {
        const double scale = coefficients[static_cast<size_t>(power)] * std::pow(amplitude, power) / std::pow(2.0, power);
        for (int j = 0; j <= power; ++j)
        {
            const int harmonic = std::abs(power - 2 * j);
            if (harmonic <= count)
            {
                // cos^n = 2^-n sum_j C(n, j) cos((n - 2j) wt); the terms j and n - j add up for harmonic > 0
                components[static_cast<size_t>(harmonic)] += scale * getBinomial(power, j);
            }
        }
    }
    std::vector<double> amplitudes(components.size());
    amplitudes[0] = components[0];
    for (size_t harmonic = 1; harmonic < components.size(); ++harmonic)
    {
        amplitudes[harmonic] = std::abs(components[harmonic]);
    }
    return amplitudes;
}

std::vector<double> getHardClipHarmonics(double amplitude, double threshold, int count)
{
    std::vector<double> amplitudes(static_cast<size_t>(count + 1), 0.0);
    if (count < 1)
    {
        return amplitudes;
    }
    if (amplitude <= threshold)
    {
        amplitudes[1] = amplitude;
        return amplitudes;
    }
    const double a = std::asin(threshold / amplitude);
    const double scale = 4.0 / detail::kPi;
    amplitudes[1] = scale * (amplitude * (a / 2.0 - std::sin(2.0 * a) / 4.0) + threshold * std::cos(a));
    for (int harmonic = 3; harmonic <= count; harmonic += 2)
    {
        const double n = harmonic;
        const double sinePart = amplitude / 2.0 * (std::sin((n - 1.0) * a) / (n - 1.0) - std::sin((n + 1.0) * a) / (n + 1.0));
        amplitudes[static_cast<size_t>(harmonic)] = std::abs(scale * (sinePart + threshold * std::cos(n * a) / n));
    }
    return amplitudes;
}

Quantizer::Quantizer(int bits, bool dither, int seed)
    : m_bits(bits)
    , m_dither(dither)
    , m_seed(seed)
    , m_step(std::pow(2.0, 1 - bits))
    , m_random(seed)
{
}

double Quantizer::getStep() const
{
    return m_step;
}

double Quantizer::getExpectedSnrDb(int bits, bool dither, double amplitude)
{
    // signal A^2/2 against q^2/12 (+ q^2/6 dither): 10 log10(6 A^2 / q^2) = 6.02 N + 1.76 + 20 log10(A)
    const double step = std::pow(2.0, 1 - bits);
    double noisePower = step * step / 12.0;
    if (dither)
    {
        noisePower += step * step / 6.0;
    }
    return 10.0 * std::log10(amplitude * amplitude / 2.0 / noisePower);
}

void Quantizer::reset()
{
    m_random.setSeed(m_seed);
}

void Quantizer::process(juce::AudioBuffer<float>& buffer)
{
    const double highest = 1.0 - m_step;
    for (int index = 0; index < buffer.getNumSamples(); ++index)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            double value = buffer.getSample(channel, index);
            if (m_dither)
            {
                value += (m_random.nextDouble() - 0.5) * m_step + (m_random.nextDouble() - 0.5) * m_step;
            }
            const double quantized = std::clamp(std::round(value / m_step) * m_step, -1.0, highest);
            buffer.setSample(channel, index, static_cast<float>(quantized));
        }
    }
}
}
