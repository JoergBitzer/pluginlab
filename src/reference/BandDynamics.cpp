#include "pluginlab/reference/BandDynamics.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/reference/Designs.h"

namespace pluginlab::reference
{
namespace
{
constexpr int kCrossoverOrder = 4;
constexpr double kSilenceDb = -200.0;

// one sample through a biquad in transposed direct form II
double filterSample(const BiquadCoefficients& section, std::array<double, 2>& state, double input)
{
    const double output = section.b0 * input + state[0];
    state[0] = section.b1 * input - section.a1 * output + state[1];
    state[1] = section.b2 * input - section.a2 * output;
    return output;
}

// the gain computer of Giannoulis et al. (the compressor's), the reduction in dB
double getReduction(const CompressorSettings& settings, double levelDb)
{
    const double over = levelDb - settings.thresholdDb;
    double output = levelDb;
    if (2.0 * over > settings.kneeDb)
    {
        output = settings.thresholdDb + over / settings.ratio;
    }
    else if (settings.kneeDb > 0.0 && 2.0 * std::abs(over) <= settings.kneeDb)
    {
        output = levelDb + (1.0 / settings.ratio - 1.0) * (over + settings.kneeDb / 2.0) * (over + settings.kneeDb / 2.0) / (2.0 * settings.kneeDb);
    }
    return levelDb - output;
}
}

TwoBandCompressor::TwoBandCompressor(const CompressorSettings& settings, double crossoverHz)
    : m_settings(settings), m_crossoverHz(crossoverHz)
{
    reset();
}

void TwoBandCompressor::reset()
{
    m_lowPass = std::make_unique<BiquadCascade>(designLinkwitzRiley(Pass::Low, kCrossoverOrder, m_settings.sampleRate, m_crossoverHz), m_settings.sampleRate);
    m_highPass = std::make_unique<BiquadCascade>(designLinkwitzRiley(Pass::High, kCrossoverOrder, m_settings.sampleRate, m_crossoverHz), m_settings.sampleRate);
    m_low = std::make_unique<Compressor>(m_settings);
    m_high = std::make_unique<Compressor>(m_settings);
}

void TwoBandCompressor::process(juce::AudioBuffer<float>& buffer)
{
    m_lowBuffer.makeCopyOf(buffer);
    m_lowPass->process(m_lowBuffer);
    m_highPass->process(buffer);
    m_low->process(m_lowBuffer);
    m_high->process(buffer);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        buffer.addFrom(channel, 0, m_lowBuffer, channel, 0, buffer.getNumSamples());
    }
}

DynamicEq::DynamicEq(const CompressorSettings& settings, double frequencyHz, double q, int updateSamples)
    : m_settings(settings), m_frequencyHz(frequencyHz), m_q(q), m_updateSamples(std::max(1, updateSamples))
{
    m_attack = std::exp(-1.0 / (std::max(settings.attackSeconds, 1.0e-9) * settings.sampleRate));
    m_release = std::exp(-1.0 / (std::max(settings.releaseSeconds, 1.0e-9) * settings.sampleRate));
    m_bandPass = designRbj(FilterType::BandPassUnity, settings.sampleRate, frequencyHz, 0.0, q);
    m_peak = designRbj(FilterType::Peak, settings.sampleRate, frequencyHz, 0.0, q);
}

void DynamicEq::reset()
{
    std::fill(m_detectorState.begin(), m_detectorState.end(), std::array<double, 2>{0.0, 0.0});
    std::fill(m_filterState.begin(), m_filterState.end(), std::array<double, 2>{0.0, 0.0});
    m_smoothed = 0.0;
    m_counter = 0;
    m_peak = designRbj(FilterType::Peak, m_settings.sampleRate, m_frequencyHz, 0.0, m_q);
}

void DynamicEq::process(juce::AudioBuffer<float>& buffer)
{
    const size_t channels = static_cast<size_t>(buffer.getNumChannels());
    if (m_detectorState.size() != channels)
    {
        m_detectorState.assign(channels, {0.0, 0.0});
        m_filterState.assign(channels, {0.0, 0.0});
    }
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // the detector: the band-passed peak level of the louder channel, the reduction smoothed in dB (attack and release)
        double loudest = kSilenceDb;
        for (size_t channel = 0; channel < channels; ++channel)
        {
            const double band = filterSample(m_bandPass, m_detectorState[channel], buffer.getSample(static_cast<int>(channel), sample));
            if (std::abs(band) > 0.0)
            {
                loudest = std::max(loudest, 20.0 * std::log10(std::abs(band)));
            }
        }
        const double reduction = getReduction(m_settings, loudest);
        double coefficient = m_release;
        if (reduction > m_smoothed)
        {
            coefficient = m_attack;
        }
        m_smoothed = coefficient * m_smoothed + (1.0 - coefficient) * reduction;
        if (m_counter == 0)
        {
            m_peak = designRbj(FilterType::Peak, m_settings.sampleRate, m_frequencyHz, -m_smoothed, m_q);
        }
        m_counter = (m_counter + 1) % m_updateSamples;
        for (size_t channel = 0; channel < channels; ++channel)
        {
            const double output = filterSample(m_peak, m_filterState[channel], buffer.getSample(static_cast<int>(channel), sample));
            buffer.setSample(static_cast<int>(channel), sample, static_cast<float>(output));
        }
    }
}
}
