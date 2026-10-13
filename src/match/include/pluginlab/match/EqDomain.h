#pragma once

#include <memory>
#include <vector>

#include "pluginlab/eqmodel/EqModel.h"
#include "pluginlab/match/Match.h"

namespace pluginlab::match
{
// The EQ domain of the matcher (W9.2, docs/matching/eq-domain.md): the magnitude response as the features, a band-model curve as a master
// without a render, the weighted dB difference as the objective. Phase later (the author: amplitude first).

struct MagnitudeProbeSettings
{
    double sampleRate = 48000.0;
    double lowestHz = 20.0;        // the user's frequency range
    double highestHz = 20000.0;    // at most 0.45 of the sample rate
    double perOctave = 12.0;       // points of the log grid
    double levelDbfs = -12.0;      // peak of the sweep
    double sweepSeconds = 0.5;
    double windowSeconds = 0.25;   // the impulse response after its peak (an EQ rings shortly; a long window costs time per render)
    double floorDb = -100.0;       // levels below are clamped (a stop band would otherwise dominate the dB difference)
};

// The magnitude in dB on a log grid in the range (synchronized sweep, channel 1)
class MagnitudeProbe : public Probe
{
public:
    explicit MagnitudeProbe(const MagnitudeProbeSettings& settings = {});
    std::vector<double> measure(const measure::Device& device) const override;
    juce::String describe() const override;
    const std::vector<double>& getFrequencies() const { return m_frequencies; }
    const MagnitudeProbeSettings& getSettings() const { return m_settings; }

private:
    MagnitudeProbeSettings m_settings;
    std::vector<double> m_frequencies;
};

// A band-model curve as the master (bands typed in or from a band fit): its exact magnitude in dB on the probe's grid, clamped at the floor
std::vector<double> getCurveMagnitude(const eqmodel::EqModel& model, const std::vector<double>& frequencies, double sampleRate, double floorDb = -100.0);

// The dB difference, every grid point the same weight (the grid is logarithmic: every octave counts the same); within the tolerance when the
// largest difference is below it (default 0.1 dB, planning: "equal")
std::shared_ptr<WeightedDifference> makeMagnitudeObjective(size_t features, double toleranceDb = 0.1);
}
