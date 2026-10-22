#pragma once

#include <memory>
#include <vector>

#include "pluginlab/match/Match.h"

namespace pluginlab::match
{
// The reverb domain of the matcher (W11.2f, docs/design/W11-reverb-plan.md 5): an impulse through the device, the decay analysis (the reverberant
// part, octaves), the features in units of a just noticeable difference, so that one tolerance fits all: T30 per octave and the broadband EDT as
// ln(T) / ln(1 + jnd) (a difference of 1 = 5 % with jnd 0.05), the pre-delay in units of preDelayJndMs, C80 in units of c80JndDb.
struct ReverbProbeSettings
{
    double sampleRate = 48000.0;
    double renderSeconds = 4.0;
    double levelDbfs = -6.0;
    double latencySamples = 0.0;
    double decayJnd = 0.05;          // 5 %: "the same" (the author)
    double preDelayJndMs = 1.0;
    double c80JndDb = 1.0;           // often quoted; to be checked against ISO 3382-1 (the plan)
};

class ReverbProbe : public Probe
{
public:
    explicit ReverbProbe(const ReverbProbeSettings& settings = {});
    std::vector<double> measure(const measure::Device& device) const override;
    juce::String describe() const override;
    const ReverbProbeSettings& getSettings() const { return m_settings; }

    // The names of the features (for reports)
    std::vector<juce::String> getFeatureNames() const;

private:
    ReverbProbeSettings m_settings;
};

// Every feature the same weight; within the tolerance when every one is within `jnds` just noticeable differences
std::shared_ptr<WeightedDifference> makeReverbObjective(size_t features, double jnds = 1.0);
}
