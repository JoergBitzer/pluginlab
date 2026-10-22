#pragma once

#include <array>
#include <memory>
#include <vector>

#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Compressor.h"

namespace pluginlab::reference
{
// Two small references for the two-tone test of W11.3d (docs/design/W11-dynamics-plan.md 4.4: "is it multiband or a dynamic EQ?").

// A two-band compressor: a Linkwitz-Riley crossover of order 4 (low-pass + high-pass sum to an all-pass), each band through its own compressor
// (the same settings), the bands added
class TwoBandCompressor : public Processor
{
public:
    TwoBandCompressor(const CompressorSettings& settings, double crossoverHz);

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    CompressorSettings m_settings;
    double m_crossoverHz;
    std::unique_ptr<BiquadCascade> m_lowPass;
    std::unique_ptr<BiquadCascade> m_highPass;
    std::unique_ptr<Compressor> m_low;
    std::unique_ptr<Compressor> m_high;
    juce::AudioBuffer<float> m_lowBuffer;
};

// A dynamic EQ band: a band-pass (RBJ, 0 dB peak) at the band's frequency feeds the detector of a compressor (the gain computer and the smoothing
// of the settings); the gain reduction sets the gain of a peak filter (RBJ) at the same frequency and Q, recomputed every updateSamples
class DynamicEq : public Processor
{
public:
    DynamicEq(const CompressorSettings& settings, double frequencyHz, double q, int updateSamples = 16);

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    CompressorSettings m_settings;
    double m_frequencyHz;
    double m_q;
    int m_updateSamples;
    double m_attack = 0.0;
    double m_release = 0.0;
    BiquadCoefficients m_bandPass;
    std::vector<std::array<double, 2>> m_detectorState;  // per channel
    std::vector<std::array<double, 2>> m_filterState;    // per channel: the peak filter
    double m_smoothed = 0.0;                              // the gain reduction (linked over the channels)
    BiquadCoefficients m_peak;
    int m_counter = 0;
};
}
