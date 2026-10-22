#pragma once

#include <vector>

#include "pluginlab/reference/Processor.h"

namespace pluginlab::reference
{
// Schroeder's reverberator (1962) in Moorer's and Freeverb's arrangement (W11.2c, docs/design/W11-reverb-plan.md 7): parallel feedback combs (a
// delay D_k, gain g_k = 10^(-3 D_k / (T60 fs)), so that each comb falls by 60 dB in T60), their sum through all-passes in series. A teaching
// example of a poor artificial reverb: its tail repeats with the comb delays (peaks in the normalized autocorrelation) and stays sparse.
struct SchroederReverbSettings
{
    double sampleRate = 48000.0;
    double t60Seconds = 2.0;
    std::vector<int> combDelays = {1557, 1617, 1491, 1422};  // Freeverb's first four (at 44.1 kHz)
    std::vector<int> allPassDelays = {556, 441};
    double allPassGain = 0.5;
    double wetGain = 0.25;                                   // the comb sum scaled (no dry path)
};

class SchroederReverb : public Processor
{
public:
    explicit SchroederReverb(const SchroederReverbSettings& settings);

    double getCombGain(size_t comb) const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    struct Line
    {
        std::vector<double> buffer;
        size_t position = 0;
    };

    SchroederReverbSettings m_settings;
    std::vector<std::vector<Line>> m_combs;     // [channel][comb]
    std::vector<std::vector<Line>> m_allPasses; // [channel][all-pass]
};
}
