#pragma once

#include <vector>

namespace pluginlab::reference
{
// The reference impulse response of a reverb (W11.2, docs/design/W11-reverb-plan.md 7, the "convolution" mode of the Reference Reverb): the direct
// path, a pre-delay, a few early reflections and a tail of exponentially decaying noise whose decay time depends on frequency. The tail is
// noise with a flat magnitude spectrum (random phases: every frequency carries exactly its share of the energy), decayed per 1/12-octave sub-band
// by 10^(-3 t / T60(f)) in a short-time spectrum (2048 samples, 75 % overlap): the energy at frequency f falls by 60 dB in T60(f) seconds.
struct ReflectionTap
{
    double delaySeconds = 0.0;   // after the direct path
    double gain = 0.0;
};

struct SyntheticReverbSettings
{
    double sampleRate = 48000.0;
    double directGain = 1.0;                 // the dry path at sample 0 (0: 100 % wet)
    double preDelaySeconds = 0.02;           // the tail starts here (after the direct path)
    double tailLevel = 0.1;                  // RMS of the tail at its start
    std::vector<double> bandHz = {125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0}; // T60 given at these frequencies ...
    std::vector<double> t60Seconds = {2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0};               // ... interpolated in log f between, constant outside
    std::vector<ReflectionTap> reflections;
    double noiseFloor = 0.0;                 // RMS of white noise added over the whole response (a floor for Lundeby's method); 0: none
    double lengthSeconds = 0.0;              // 0: until the tail is 120 dB down at its longest T60 (at most 30 s)
    int seed = 1;
};

class SyntheticReverb
{
public:
    explicit SyntheticReverb(const SyntheticReverbSettings& settings);

    const SyntheticReverbSettings& getSettings() const;
    const std::vector<double>& getImpulseResponse() const;

    // T60 at a frequency (the interpolation of the settings)
    double getDecayTime(double frequencyHz) const;
    // The sub-bands of the tail: their edges and decay times (for an expected decay through a band filter)
    const std::vector<double>& getSubBandEdges() const;     // K + 1 edges in Hz
    const std::vector<double>& getSubBandDecayTimes() const; // K decay times

private:
    SyntheticReverbSettings m_settings;
    std::vector<double> m_response;
    std::vector<double> m_edges;
    std::vector<double> m_decays;
};
}
