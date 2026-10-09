#pragma once

#include <vector>

#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/SweptImpulse.h"

namespace pluginlab::measure
{
// Phase response and group delay (docs/measurements/phase-and-group-delay.md): AES17-2015 6.8.3 (input-to-output phase response),
// 6.8.4 (group delay vs frequency), 6.2.7 (inter-channel phase response), annex A.4.6 / A.4.8.
struct PhaseSettings
{
    double sampleRate = 48000.0;
    double levelDbfs = -20.0;              // AES17 6.8.3: -20 dB relative to the maximum input level
    std::vector<double> frequencies;       // empty: 1/48 octave from 20 Hz to 20 kHz
    int channels = 2;
};

struct ChannelPhase
{
    int delaySamples = 0;                          // the delay removed first: the peak of the impulse response (AES17 6.8.2 a)
    std::vector<double> phaseDegrees;              // unwrapped phase of H after removing delaySamples (AES17 6.8.3 b: phase minus the delay's phase)
    double fitInterceptDegrees = 0.0;              // the straight line a + b f fitted to phaseDegrees in the passband (AES17 6.8.3 a; passband:
                                                   // the band of validity where the gain is within 40 dB of its largest value)
    double fitDelaySamples = 0.0;                  // delaySamples - b fs / 360: the delay of the fitted line
    std::vector<double> deviationFromLinearDegrees; // phaseDegrees - (a + b f) (AES17 6.8.3 a)
    double deviationMaxDegrees = 0.0;              // the summary of 6.8.3 over the passband: "+max/-min degrees from 20 Hz to 20 kHz"
    double deviationMinDegrees = 0.0;
    std::vector<double> groupDelaySamples;         // exact: Re{DTFT(n h) / DTFT(h)} of the window, minus that of the reference channel
    std::vector<double> groupDelayDifferenceSamples; // AES17 6.8.4: -(phase difference of neighbouring points) / (2 pi df / fs), at the midpoints
    std::vector<double> interChannelDegrees;       // AES17 6.2.7: arg(H_c / H_1), -180 ... 180 (0 for the first channel)
};

struct PhaseResponse
{
    PhaseSettings settings;
    std::vector<double> frequencyHz;
    std::vector<double> differenceFrequencyHz;     // the midpoints of groupDelayDifferenceSamples
    double validFromHz = 20.0;
    double validToHz = kUpperBandEdgeHz;
    std::vector<ChannelPhase> channels;
};

PhaseResponse measurePhaseResponse(const Device& device, const PhaseSettings& settings);

// The group delay of a segment (time of its first sample firstTime) at a frequency: Re{sum t x[t] e^(-j w t) / sum x[t] e^(-j w t)} (samples)
double getSegmentGroupDelay(const std::vector<double>& segment, int firstTime, double frequencyHz, double sampleRate);
}
