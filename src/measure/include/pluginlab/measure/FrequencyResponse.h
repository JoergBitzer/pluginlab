#pragma once

#include <complex>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Analyzer.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
// Frequency response (docs/measurements/frequency-response.md): AES17-2015 6.2.3 (stepped sine), annex A.3.4 (synchronous multitone),
// annex A.4.5 (exponential sweep; here the synchronized swept sine of Novak, Lotton, Simon, JAES 63(10), 2015).

enum class ResponseMethod
{
    SteppedSine,
    Multitone,
    SweptSine
};

const char* getResponseMethodName(ResponseMethod method);

struct ChannelResponse
{
    std::vector<std::complex<double>> response;   // output / input at each frequency (the phase includes the latency of the device)
    std::vector<double> magnitudeDb;              // 20 lg |H|
    std::vector<double> relativeDb;               // relative to the gain at the reference frequency (AES17 6.2.3: re 997 Hz)
    std::vector<double> phaseDegrees;             // arg H, -180 ... 180 (unwrapped and without latency: W7.4)
    std::vector<double> broadbandDb;              // stepped sine only: rms output / rms input through the standard low-pass (AES17 6.2.3)
    double referenceGainDb = 0.0;                 // the gain at the reference frequency
};

struct FrequencyResponse
{
    ResponseMethod method = ResponseMethod::SteppedSine;
    double sampleRate = 48000.0;
    std::vector<double> frequencyHz;
    double referenceHz = kStandardFrequencyHz;    // the multitone uses its tone nearest to 997 Hz
    double validFromHz = 20.0;                    // the band in which the method is accurate (see the document)
    double validToHz = kUpperBandEdgeHz;
    std::vector<ChannelResponse> channels;
};

// AES17 table 3: the standard third-octave frequencies from 20 Hz to 20 kHz, with 997 Hz in place of 1 kHz (6.2.3: "One of the test frequencies
// shall be 997 Hz")
std::vector<double> getStandardThirdOctaveFrequencies();

struct SteppedResponseSettings
{
    double sampleRate = 48000.0;
    std::vector<double> frequencies = getStandardThirdOctaveFrequencies();
    double levelDbfs = -20.0;        // AES17 6.2.3: -20 dB re the maximum input level
    int latencySamples = 0;          // of the device, if known (the window then starts later)
    double settleSeconds = 0.1;      // after the fade-in (and the latency), before the window
    double measureSeconds = 0.1;     // at least; whole periods, at least 10 periods
    int channels = 2;
};

struct MultitoneResponseSettings
{
    double sampleRate = 48000.0;
    double lowestHz = 20.0;
    double highestHz = kUpperBandEdgeHz;
    int numberOfTones = 31;          // about one per third octave
    double levelDbfs = -20.0;        // rms of the whole multitone
    int periodSamples = 0;           // 0: the power of two near 1.4 s (65536 at 48 kHz); the device's impulse response must be shorter
    int channels = 2;
};

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

FrequencyResponse measureSteppedResponse(const Device& device, const SteppedResponseSettings& settings);
FrequencyResponse measureMultitoneResponse(const Device& device, const MultitoneResponseSettings& settings);
FrequencyResponse measureSweptResponse(const Device& device, const SweepResponseSettings& settings);
}
