#include "pluginlab/reference/Utility.h"

#include <functional>

#include "ReferenceMath.h"

namespace pluginlab::reference
{
namespace
{
constexpr int kSeedOffsetPerChannel = 4; // as makeNoise for uncorrelated channels

void addPerSample(juce::AudioBuffer<float>& buffer, const std::function<double(int)>& valueAt)
{
    for (int index = 0; index < buffer.getNumSamples(); ++index)
    {
        const double value = valueAt(index);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.setSample(channel, index, static_cast<float>(buffer.getSample(channel, index) + value));
        }
    }
}
}

Gain::Gain(double gainDb, bool invertPolarity)
    : m_factor(detail::dbToGain(gainDb))
{
    if (invertPolarity)
    {
        m_factor = -m_factor;
    }
}

double Gain::getFactor() const
{
    return m_factor;
}

void Gain::reset()
{
}

void Gain::process(juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* data = buffer.getWritePointer(channel);
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            data[index] = static_cast<float>(m_factor * data[index]);
        }
    }
}

ChannelMatrix::ChannelMatrix(double ll, double lr, double rl, double rr)
    : m_ll(ll)
    , m_lr(lr)
    , m_rl(rl)
    , m_rr(rr)
{
}

ChannelMatrix ChannelMatrix::makeCrosstalk(double crosstalkDb)
{
    const double crosstalk = detail::dbToGain(crosstalkDb);
    return ChannelMatrix(1.0, crosstalk, crosstalk, 1.0);
}

ChannelMatrix ChannelMatrix::makeWidth(double width)
{
    const double same = (1.0 + width) / 2.0;
    const double other = (1.0 - width) / 2.0;
    return ChannelMatrix(same, other, other, same);
}

double ChannelMatrix::getLeftFromLeft() const
{
    return m_ll;
}

double ChannelMatrix::getLeftFromRight() const
{
    return m_lr;
}

double ChannelMatrix::getRightFromLeft() const
{
    return m_rl;
}

double ChannelMatrix::getRightFromRight() const
{
    return m_rr;
}

void ChannelMatrix::reset()
{
}

void ChannelMatrix::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
    {
        return;
    }
    float* left = buffer.getWritePointer(0);
    float* right = buffer.getWritePointer(1);
    for (int index = 0; index < buffer.getNumSamples(); ++index)
    {
        const double l = left[index];
        const double r = right[index];
        left[index] = static_cast<float>(m_ll * l + m_lr * r);
        right[index] = static_cast<float>(m_rl * l + m_rr * r);
    }
}

DcOffset::DcOffset(double offset)
    : m_offset(offset)
{
}

void DcOffset::reset()
{
}

void DcOffset::process(juce::AudioBuffer<float>& buffer)
{
    addPerSample(buffer, [this](int)
                 {
                     return m_offset;
                 });
}

HumAdder::HumAdder(double sampleRate, double frequencyHz, double levelDbfsPeak, std::vector<double> harmonicLevelsDb)
    : m_sampleRate(sampleRate)
    , m_frequencyHz(frequencyHz)
{
    const double fundamental = detail::dbToGain(levelDbfsPeak);
    m_amplitudes.push_back(fundamental);
    for (const double level : harmonicLevelsDb)
    {
        m_amplitudes.push_back(fundamental * detail::dbToGain(level));
    }
}

double HumAdder::getHum(juce::int64 sample) const
{
    double hum = 0.0;
    for (size_t index = 0; index < m_amplitudes.size(); ++index)
    {
        const double harmonic = static_cast<double>(index + 1);
        hum += m_amplitudes[index] * std::sin(detail::kTwoPi * harmonic * m_frequencyHz * static_cast<double>(sample) / m_sampleRate);
    }
    return hum;
}

void HumAdder::reset()
{
    m_position = 0;
}

void HumAdder::process(juce::AudioBuffer<float>& buffer)
{
    addPerSample(buffer, [this](int index)
                 {
                     return getHum(m_position + index);
                 });
    m_position += buffer.getNumSamples();
}

NoiseAdder::NoiseAdder(signals::NoiseColour colour, double levelDbfsRms, int seed)
    : m_colour(colour)
    , m_scale(detail::dbToGain(levelDbfsRms) / signals::NoiseGenerator::getRawRms(colour))
    , m_seed(seed)
{
}

void NoiseAdder::reset()
{
    m_generators.clear();
}

void NoiseAdder::process(juce::AudioBuffer<float>& buffer)
{
    while (static_cast<int>(m_generators.size()) < buffer.getNumChannels())
    {
        const int channel = static_cast<int>(m_generators.size());
        m_generators.push_back(std::make_unique<signals::NoiseGenerator>(m_colour, m_seed + kSeedOffsetPerChannel * channel));
    }
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float* data = buffer.getWritePointer(channel);
        signals::NoiseGenerator& generator = *m_generators[static_cast<size_t>(channel)];
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            data[index] = static_cast<float>(data[index] + m_scale * generator.nextSample());
        }
    }
}

Tremolo::Tremolo(double sampleRate, double rateHz, double depth)
    : m_sampleRate(sampleRate)
    , m_rateHz(rateHz)
    , m_depth(depth)
{
}

double Tremolo::getGain(juce::int64 sample) const
{
    return 1.0 - m_depth * (1.0 - std::cos(detail::kTwoPi * m_rateHz * static_cast<double>(sample) / m_sampleRate)) / 2.0;
}

void Tremolo::reset()
{
    m_position = 0;
}

void Tremolo::process(juce::AudioBuffer<float>& buffer)
{
    for (int index = 0; index < buffer.getNumSamples(); ++index)
    {
        const double gain = getGain(m_position + index);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.setSample(channel, index, static_cast<float>(gain * buffer.getSample(channel, index)));
        }
    }
    m_position += buffer.getNumSamples();
}
}
