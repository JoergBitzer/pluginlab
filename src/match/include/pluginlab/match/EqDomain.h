#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "pluginlab/eqmodel/EqModel.h"
#include "pluginlab/match/Match.h"

namespace pluginlab::match
{
// The EQ domain of the matcher (W9.2, docs/matching/eq-domain.md): the magnitude response as the features, a band-model curve as a master
// without a render, the weighted dB difference as the objective. Phase later (the author: amplitude first).

// The stimulus: a synchronized sweep (robust, slower) or an impulse (the fast render path of W9.3: one short render, its spectrum)
enum class ProbeStimulus
{
    Sweep,
    Impulse
};

struct MagnitudeProbeSettings
{
    ProbeStimulus stimulus = ProbeStimulus::Sweep;
    double impulseSeconds = 0.35;  // the impulse response recorded (latency and ringing of an EQ fit in it)
    double sampleRate = 48000.0;
    double lowestHz = 20.0;        // the user's frequency range
    double highestHz = 20000.0;    // at most 0.45 of the sample rate
    bool upToNyquist = false;      // the grid ends at exactly fs / 2 instead (impulse only: the live display compares EQs where they cramp)
    double perOctave = 12.0;       // points of the log grid
    double levelDbfs = -12.0;      // peak of the sweep
    double sweepSeconds = 0.5;
    double windowSeconds = 0.25;   // the impulse response after its peak (an EQ rings shortly; a long window costs time per render)
    double floorDb = -100.0;       // levels below are clamped (a stop band would otherwise dominate the dB difference)
};

// The magnitude in dB on a log grid in the range (channel 1): from a synchronized sweep, or from the spectrum of the impulse response
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

// How close is close enough (the author: the just noticeable difference of a level in a third-octave band is about 1 dB): the largest dB
// difference allowed. Exact is for checking precision (twins), strict is planning's "equal", medium and loose are perceptually still fine
// and end the search sooner.
enum class MatchTolerance
{
    Exact,      // 0.01 dB
    Strict,     // 0.1 dB (default)
    Medium,     // 0.25 dB
    Loose       // 0.5 dB
};

double getToleranceDb(MatchTolerance tolerance);
const char* getToleranceName(MatchTolerance tolerance);           // "exact (0.01 dB)", ...
std::optional<MatchTolerance> parseTolerance(const juce::String& name); // "exact", "strict", "medium", "loose"

// The dB difference, every grid point the same weight (the grid is logarithmic: every octave counts the same); within the tolerance when the
// largest difference is below it
std::shared_ptr<WeightedDifference> makeMagnitudeObjective(size_t features, double toleranceDb = 0.1);
std::shared_ptr<WeightedDifference> makeMagnitudeObjective(size_t features, MatchTolerance tolerance);
}
