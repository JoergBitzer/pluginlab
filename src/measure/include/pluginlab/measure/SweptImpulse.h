#pragma once

#include <complex>
#include <vector>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// The impulse response of a device by the synchronized swept sine (Novak, Lotton, Simon, JAES 63(10), 2015; AES17-2015 annex A.4), shared by the
// frequency response (W7.2), delay and polarity (W7.3) and phase / group delay (W7.4); see docs/measurements/frequency-response.md.

struct SweepResponseSettings
{
    double sampleRate = 48000.0;
    double startHz = 5.0;            // two octaves below the passband, so that 20 Hz is measured cleanly
    double stopHz = 0.0;             // 0: 0.95 of Nyquist, at most 40 kHz
    double approximateSeconds = 4.0;
    double levelDbfs = -20.0;        // peak of the sweep
    double windowBeforeSeconds = 0.25;   // the window around the peak of the impulse response: before it (the band-limited impulse rings before its peak)
    double windowAfterSeconds = 0.5;     // and after it (the tail of the device)
    std::vector<double> frequencies; // where the response is evaluated; empty: 1/24 octave from 20 Hz to 20 kHz (and 997 Hz)
    int channels = 2;
};

// One channel: the full circular deconvolution, the linear peak, and the window around it
struct SweptChannel
{
    std::vector<float> impulse;      // the deconvolution (circular: index 0 = time 0; the harmonic responses lie at the end)
    int peak = 0;                    // index of the absolute peak of the linear impulse response (the delay of the peak, AES17 6.8.2 a)
    std::vector<double> segment;     // the windowed impulse response around the peak
    int firstTime = 0;               // time (samples) of the first sample of the segment
};

struct SweptImpulses
{
    double sampleRate = 48000.0;
    double rateSeconds = 0.0;        // L of the sweep
    double validFromHz = 20.0;
    double validToHz = kUpperBandEdgeHz;
    // the reference channel: the stimulus itself, deconvolved and windowed the same way (around time 0)
    std::vector<double> referenceSegment;
    int referenceFirstTime = 0;
    std::vector<SweptChannel> channels;
};

SweptImpulses measureSweptImpulses(const Device& device, const SweepResponseSettings& settings);

// H(f) of a channel: the DTFT of its window divided by the DTFT of the reference window (the phase with the time origin of the deconvolution,
// i.e. the latency included)
std::complex<double> getSweptResponse(const SweptImpulses& impulses, int channel, double frequencyHz);

// The DTFT of a segment whose first sample is at time firstTime (samples): sum x[n] e^(-j 2 pi f (firstTime + n) / fs)
std::complex<double> getDtft(const std::vector<double>& segment, int firstTime, double frequencyHz, double sampleRate);
}
