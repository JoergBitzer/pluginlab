#pragma once

#include <complex>
#include <vector>

namespace pluginlab::measure
{
// The analyzer of AES17-2015, clause 5.2 (docs/measurements/README.md).

constexpr double kStandardFrequencyHz = 997.0;      // AES17 3.12.1, 5.4: the test frequency of most measurements
constexpr double kUpperBandEdgeHz = 20000.0;        // AES17 4.3 (44.1 and 48 kHz; the same is used at higher rates)
constexpr double kMinimumIntegrationSeconds = 0.025; // AES17 5.2.3: the level meter integrates at least 25 ms

// dBFS after AES17 3.12: an rms level relative to the rms of a full-scale sine (amplitude 1): 20 lg(rms sqrt(2)); a full-scale sine is 0 dBFS
double rmsToDbfs(double rms);
double dbfsToRms(double dbfs);

// AES17 5.2.5 standard low-pass filter for the upper band-edge frequency 20 kHz: passband +-0.1 dB from 20 Hz to 20 kHz, at least 60 dB of
// attenuation above 24 kHz. At 44.1 and 48 kHz nothing above 24 kHz can exist: the filter is the identity. Above, a linear-phase FIR (Kaiser window,
// 70 dB design attenuation, transition 20 ... 24 kHz); its delay is (taps - 1) / 2 samples, which a level measurement does not need to remove.
class StandardLowPass
{
public:
    explicit StandardLowPass(double sampleRate);

    bool isIdentity() const;
    int getTaps() const;
    std::complex<double> getResponse(double frequencyHz) const;

    // The filtered signal (same length; the first taps - 1 samples are the filter's run-in)
    std::vector<double> process(const float* data, int length) const;

private:
    double m_sampleRate;
    std::vector<double> m_taps;
};

// True rms of samples [start, start + length) (AES17 5.2.3: a true-rms meter)
double getRms(const std::vector<double>& data, int start, int length);

// The complex amplitude of one frequency in samples [start, start + length): 2/N sum x[n] e^(-j 2 pi f n / fs), n counted from start. For a whole
// number of periods in the window this is exact (a frequency-domain band-pass of one bin, AES17 5.2.10 with a rectangular window on a synchronous
// signal, annex A.2); |amplitude| / sqrt(2) is the rms of the tone, the angle its phase (of a cosine) at sample start.
std::complex<double> getToneAmplitude(const std::vector<double>& data, int start, int length, double frequencyHz, double sampleRate);

// The length of a measurement window: a whole number of periods of the frequency, at least `seconds` and at least 25 ms (AES17 5.2.3)
int getWholePeriodLength(double frequencyHz, double sampleRate, double seconds);
}
