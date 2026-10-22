#pragma once

#include <vector>

#include "pluginlab/reference/Processor.h"

namespace pluginlab::reference
{
// A feedback delay network (W11.2d, docs/design/W11-reverb-plan.md 7, the algorithmic part of the Reference Reverb; Jot and Chaigne 1991):
// N delay lines of coprime lengths, a Householder feedback matrix (I - 2/N 1 1^T: lossless), per line the gain g_i = 10^(-3 D_i / (T60 fs)) so
// that every circulation loses 60 dB in T60 (the decay is exactly T60 at every frequency), the input into every line, the outputs summed with
// alternating signs (left) and in a second order (right). Optionally an LFO per line on its length (sine, rates spread around lfoRateHz, Lagrange
// taps per sample): the reflections move, a time-variant reverb on demand. The decay at low frequencies stays T60; the interpolation in the loop
// damps the highs on every circulation (Lagrange falls towards Nyquist), so the broadband decay gets shorter (1 Hz, 8 samples: 1.41 s for 1.5 s).
struct FdnReverbSettings
{
    double sampleRate = 48000.0;
    double t60Seconds = 1.5;
    std::vector<int> delays = {1031, 1327, 1523, 1709, 1907, 2111, 2333, 2539}; // samples, coprime
    double preDelaySeconds = 0.01;
    double wetGain = 0.3;
    double lfoRateHz = 0.0;           // 0: no modulation
    double lfoDepthSamples = 0.0;
    int seed = 1;                     // the LFOs' start phases (fixed per seed)
};

class FdnReverb : public Processor
{
public:
    explicit FdnReverb(const FdnReverbSettings& settings);

    double getLineGain(size_t line) const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    double readLine(size_t line, double delay) const;

    FdnReverbSettings m_settings;
    std::vector<std::vector<double>> m_lines;
    std::vector<double> m_lfoPhases;
    std::vector<double> m_lfoRates;
    std::vector<double> m_preDelay;    // a mono line of the pre-delay
    size_t m_position = 0;
    size_t m_preDelayPosition = 0;
    juce::int64 m_sample = 0;
    size_t m_size = 0;
};
}
