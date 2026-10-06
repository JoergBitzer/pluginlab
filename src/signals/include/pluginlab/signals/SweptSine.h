#pragma once

#include <complex>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace pluginlab::signals
{
// The synchronized exponential swept sine (Novak, Lotton, Simon: "Synchronized Swept-Sine: Theory, Application and Implementation",
// JAES 63(10), 2015):  x(t) = sin(2 pi f1 L exp(t / L)),  L = k / f1,  k = round(f1 T~ / ln(f2 / f1)),  duration T = L ln(f2 / f1).
// It starts and ends with phase 0, and a delay of L ln(n) is the same as its n-th harmonic, so the impulse responses of the harmonics
// of a nonlinear system come out at -L ln(n) after the deconvolution with the analytic inverse filter
//   X~(f) = 2 sqrt(f / L) exp(-j 2 pi f L (1 - ln(f / f1)) + j pi / 4)        (eq. 43 of the paper).
struct SweptSineSettings
{
    double sampleRate = 48000.0;
    double startHz = 30.0;            // the agreed default (10 Hz in reserve)
    double stopHz = 20000.0;
    double approximateSeconds = 5.0;  // the real duration follows from the rounding of k
    double levelDbfsPeak = -6.0;
    double preSilenceSeconds = 0.1;
    double postSilenceSeconds = 1.0;  // room for the tail and the latency of the system
};

struct SweptSine
{
    SweptSineSettings settings;
    double rate = 0.0;          // L in seconds
    double durationSeconds = 0.0;
    int startSample = 0;        // the first sample of the sweep in the signal (after the pre-silence)
    int sweepLength = 0;        // samples of the sweep itself
    double amplitude = 0.0;     // linear peak
    juce::AudioBuffer<float> signal;
};

SweptSine makeSweptSine(const SweptSineSettings& settings, int channels);

// The impulse response of a system from its response to the sweep (one channel; response[0] is the sample that belongs to the first sample
// of the signal, i.e. the pre-silence is part of it). The linear impulse response starts at index 0 (plus the latency of the system); the
// response of harmonic n is at the end of the buffer, getHarmonicPosition() samples before its end. Scaled so that the identity gives a unit
// impulse. fftSize: a power of two larger than the response (0: chosen automatically).
std::vector<float> deconvolveSweptSine(const SweptSine& sweep, const std::vector<float>& response, int fftSize = 0);

// L ln(n): the time by which the impulse response of harmonic n comes before the linear one, in samples
double getHarmonicAdvanceSamples(const SweptSine& sweep, int harmonic);

// The analytic inverse filter at one frequency (for tests and for the deconvolution)
std::complex<double> getInverseFilter(const SweptSine& sweep, double frequencyHz);
}
