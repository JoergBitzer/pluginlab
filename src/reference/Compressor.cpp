#include "pluginlab/reference/Compressor.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/reference/Designs.h"

namespace pluginlab::reference
{
namespace
{
constexpr double kSilenceDb = -200.0;
}

Compressor::Compressor(const CompressorSettings& settings)
    : m_settings(settings)
{
    m_attack = std::exp(-1.0 / (std::max(settings.attackSeconds, 1.0e-9) * settings.sampleRate));
    m_release = std::exp(-1.0 / (std::max(settings.releaseSeconds, 1.0e-9) * settings.sampleRate));
    m_lookahead = static_cast<int>(std::lround(settings.lookaheadSeconds * settings.sampleRate));
    m_rmsLength = std::max(1, static_cast<int>(std::lround(settings.rmsSeconds * settings.sampleRate)));
    m_detectorAttack = std::exp(-1.0 / (std::max(settings.detectorAttackSeconds, 1.0e-9) * settings.sampleRate));
    m_detectorRelease = std::exp(-1.0 / (std::max(settings.detectorReleaseSeconds, 1.0e-9) * settings.sampleRate));
    m_hold = static_cast<int>(std::lround(settings.holdSeconds * settings.sampleRate));
    if (settings.sidechainHighPassHz > 0.0)
    {
        const BiquadCoefficients section = designButterworth(Pass::High, 2, settings.sampleRate, settings.sidechainHighPassHz).front();
        m_sidechainFilter = {section.b0, section.b1, section.b2, section.a1, section.a2};
    }
}

const CompressorSettings& Compressor::getSettings() const
{
    return m_settings;
}

// the detector's state at the start: the silence floor for a level in dB (LevelLog), no power otherwise
double Compressor::getDetectorStart() const
{
    if (m_settings.smoothing == Smoothing::LevelLog)
    {
        return kSilenceDb;
    }
    return 0.0;
}

double Compressor::computeReduction(double levelDb) const
{
    const double threshold = m_settings.thresholdDb;
    const double knee = m_settings.kneeDb;
    const double over = levelDb - threshold;
    double output = levelDb;
    if (2.0 * over > knee)
    {
        output = threshold + over / m_settings.ratio;
    }
    else if (knee > 0.0 && 2.0 * std::abs(over) <= knee)
    {
        output = levelDb + (1.0 / m_settings.ratio - 1.0) * (over + knee / 2.0) * (over + knee / 2.0) / (2.0 * knee);
    }
    return levelDb - output;
}

double Compressor::getStaticGainDb(double levelDb) const
{
    return m_settings.makeupDb - computeReduction(levelDb);
}

void Compressor::setRecording(bool record)
{
    m_record = record;
    m_trace.clear();
}

const std::vector<double>& Compressor::getGainTraceDb() const
{
    return m_trace;
}

int Compressor::getLatencySamples() const
{
    return m_lookahead;
}

void Compressor::reset()
{
    std::fill(m_smoothed.begin(), m_smoothed.end(), 0.0);
    std::fill(m_detector.begin(), m_detector.end(), getDetectorStart());
    std::fill(m_sidechainState.begin(), m_sidechainState.end(), std::array<double, 2>{0.0, 0.0});
    std::fill(m_holding.begin(), m_holding.end(), 0);
    for (std::vector<double>& line : m_delay)
    {
        std::fill(line.begin(), line.end(), 0.0);
    }
    for (std::vector<double>& window : m_squares)
    {
        std::fill(window.begin(), window.end(), 0.0);
    }
    std::fill(m_squareSum.begin(), m_squareSum.end(), 0.0);
    m_position = 0;
    m_rmsPosition = 0;
    m_trace.clear();
}

void Compressor::process(juce::AudioBuffer<float>& buffer)
{
    const size_t channels = static_cast<size_t>(buffer.getNumChannels());
    if (m_smoothed.size() != channels)
    {
        m_smoothed.assign(channels, 0.0);
        m_detector.assign(channels, getDetectorStart());
        m_sidechainState.assign(channels, {0.0, 0.0});
        m_holding.assign(channels, 0);
        m_delay.assign(channels, std::vector<double>(static_cast<size_t>(m_lookahead) + 1, 0.0));
        m_squares.assign(channels, std::vector<double>(static_cast<size_t>(m_rmsLength), 0.0));
        m_squareSum.assign(channels, 0.0);
        m_position = 0;
        m_rmsPosition = 0;
    }
    std::vector<double> levels(channels);
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // the detector levels, linked
        double loudest = kSilenceDb;
        for (size_t channel = 0; channel < channels; ++channel)
        {
            double input = buffer.getSample(static_cast<int>(channel), sample);
            if (! m_sidechainFilter.empty())
            {
                // the detector hears the high-passed signal (transposed direct form II)
                std::array<double, 2>& state = m_sidechainState[channel];
                const double filtered = m_sidechainFilter[0] * input + state[0];
                state[0] = m_sidechainFilter[1] * input - m_sidechainFilter[3] * filtered + state[1];
                state[1] = m_sidechainFilter[2] * input - m_sidechainFilter[4] * filtered;
                input = filtered;
            }
            double level = kSilenceDb;
            if (m_settings.detector == Detector::Peak)
            {
                if (std::abs(input) > 0.0)
                {
                    level = 20.0 * std::log10(std::abs(input));
                }
            }
            else
            {
                // a moving mean square (the sum updated, the oldest square out)
                m_squareSum[channel] += input * input - m_squares[channel][m_rmsPosition];
                m_squares[channel][m_rmsPosition] = input * input;
                const double meanSquare = std::max(0.0, m_squareSum[channel]) / m_rmsLength;
                if (meanSquare > 0.0)
                {
                    level = 10.0 * std::log10(meanSquare);
                }
            }
            levels[channel] = level;
            loudest = std::max(loudest, level);
        }
        m_rmsPosition = (m_rmsPosition + 1) % static_cast<size_t>(m_rmsLength);
        const size_t delayLength = static_cast<size_t>(m_lookahead) + 1;
        for (size_t channel = 0; channel < channels; ++channel)
        {
            double level = (1.0 - m_settings.stereoLink) * levels[channel] + m_settings.stereoLink * loudest;
            if (m_settings.smoothing == Smoothing::LevelLog)
            {
                // the level in dB smoothed (from the silence floor at the start), the gain computer unsmoothed after it
                double coefficient = m_release;
                if (level > m_detector[channel])
                {
                    coefficient = m_attack;
                }
                m_detector[channel] = coefficient * m_detector[channel] + (1.0 - coefficient) * level;
                m_smoothed[channel] = computeReduction(m_detector[channel]);
            }
            else
            {
                if (m_settings.smoothing == Smoothing::TwoStage)
                {
                    // the power smoothed (linear), then its level
                    const double power = std::pow(10.0, level / 10.0);
                    double coefficient = m_detectorRelease;
                    if (power > m_detector[channel])
                    {
                        coefficient = m_detectorAttack;
                    }
                    m_detector[channel] = coefficient * m_detector[channel] + (1.0 - coefficient) * power;
                    level = 10.0 * std::log10(std::max(m_detector[channel], 1.0e-20));
                }
                const double reduction = computeReduction(level);
                if (reduction > m_smoothed[channel])
                {
                    m_smoothed[channel] = m_attack * m_smoothed[channel] + (1.0 - m_attack) * reduction;
                    m_holding[channel] = m_hold;
                }
                else if (m_holding[channel] > 0)
                {
                    --m_holding[channel]; // hold: the reduction stays
                }
                else
                {
                    m_smoothed[channel] = m_release * m_smoothed[channel] + (1.0 - m_release) * reduction;
                }
            }
            const double gainDb = m_settings.makeupDb - m_smoothed[channel];
            // the look-ahead: the input of lookahead samples ago gets this gain
            std::vector<double>& line = m_delay[channel];
            line[m_position] = buffer.getSample(static_cast<int>(channel), sample);
            const double delayed = line[(m_position + 1) % delayLength];
            buffer.setSample(static_cast<int>(channel), sample, static_cast<float>(delayed * std::pow(10.0, gainDb / 20.0)));
            if (m_record && channel == 0)
            {
                m_trace.push_back(gainDb);
            }
        }
        m_position = (m_position + 1) % delayLength;
    }
}
}
