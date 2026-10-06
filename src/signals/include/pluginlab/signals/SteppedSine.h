#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace pluginlab::signals
{
// The stepped sine of the Audio Precision analyzers: log-spaced frequencies from high to low (20 kHz down to 20 Hz); every step is a sine
// that lasts for the latency of the device, a settling time and a measurement time. The measurement window of each step holds a whole
// number of periods. Slow, but reliable for the magnitude (no phase), THD, THD+N and crosstalk.
struct SteppedSineSettings
{
    double sampleRate = 48000.0;
    double startHz = 20000.0;
    double stopHz = 20.0;
    int stepsPerOctave = 3;
    double levelDbfsPeak = -6.0;
    int latencySamples = 0;            // of the device under test (measured before, e.g. by the fingerprint)
    double settleSeconds = 0.05;       // after the latency, before the measurement (the measurement decides: THD longer than the transfer function)
    double measureSeconds = 0.1;       // at least; rounded up to whole periods
    int minimumPeriods = 10;           // at low frequencies the window holds at least this many periods
};

struct SineStep
{
    double frequencyHz = 0.0;
    int start = 0;          // first sample of the step in the signal
    int measureStart = 0;   // the measurement window of the step
    int measureLength = 0;  // a whole number of periods (rounded to samples)
};

struct SteppedSine
{
    SteppedSineSettings settings;
    std::vector<SineStep> steps;
    juce::AudioBuffer<float> signal;
};

SteppedSine makeSteppedSine(const SteppedSineSettings& settings, int channels);

// The log-spaced frequencies of the steps (from startHz towards stopHz, both included)
std::vector<double> getSteppedSineFrequencies(const SteppedSineSettings& settings);
}
