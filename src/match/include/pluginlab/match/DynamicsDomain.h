#pragma once

#include <memory>
#include <vector>

#include "pluginlab/match/Match.h"

namespace pluginlab::match
{
// The dynamics domain of the matcher (W11.3g, docs/design/W11-dynamics-plan.md 5): a short version of the unit "dynamics" through the device and
// its gain read as features in units of levelJndDb, so that one tolerance fits all: the steady gain of a staircase (a 500 Hz square, every 3 dB
// from -60 to 0 dBFS, 0.25 s per step: the static curve) and the gain of one step (quiet, 0 dBFS, quiet again, 0.5 s each) at log-spaced times
// after the rise and after the fall (attack and release). The same stimulus for master and target, so the features compare directly; it is not
// the stimulus of the report (61 steps of 0.5 s), which would make every evaluation 30 s of audio.
struct DynamicsProbeSettings
{
    double sampleRate = 48000.0;
    double latencySamples = 0.0;
    double lowestDbfs = -60.0;
    double stepDb = 3.0;
    double stepSeconds = 0.25;
    double quietDbfs = -50.0;           // the step's quiet level
    double phaseSeconds = 0.5;
    double levelJndDb = 0.5;            // provisional (the comparison's "the same"); a question for the author
    std::vector<double> timesSeconds{0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.4};
};

class DynamicsProbe : public Probe
{
public:
    explicit DynamicsProbe(const DynamicsProbeSettings& settings = {});
    std::vector<double> measure(const measure::Device& device) const override;
    juce::String describe() const override;
    const DynamicsProbeSettings& getSettings() const { return m_settings; }

    // The names of the features (for reports)
    std::vector<juce::String> getFeatureNames() const;

private:
    DynamicsProbeSettings m_settings;
};

// Every feature the same weight; within the tolerance when every one is within `jnds` units
std::shared_ptr<WeightedDifference> makeDynamicsObjective(size_t features, double jnds = 1.0);
}
