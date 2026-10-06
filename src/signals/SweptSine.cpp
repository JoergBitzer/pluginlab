#include "pluginlab/signals/SweptSine.h"

#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "pluginlab/signals/Signals.h"

namespace pluginlab::signals
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr double kQuarterPi = kPi / 4.0;
constexpr double kInverseFilterGain = 2.0;
}

SweptSine makeSweptSine(const SweptSineSettings& settings, int channels)
{
    SweptSine sweep;
    sweep.settings = settings;
    const double logRatio = std::log(settings.stopHz / settings.startHz);
    // eq. 3 / 32 of the paper: k must be an integer for the synchronisation
    const double k = std::max(1.0, std::round(settings.startHz * settings.approximateSeconds / logRatio));
    sweep.rate = k / settings.startHz;
    sweep.durationSeconds = sweep.rate * logRatio;
    sweep.amplitude = dbToGain(settings.levelDbfsPeak);
    sweep.startSample = static_cast<int>(std::round(settings.preSilenceSeconds * settings.sampleRate));
    sweep.sweepLength = static_cast<int>(std::floor(sweep.durationSeconds * settings.sampleRate));
    const int postSamples = static_cast<int>(std::round(settings.postSilenceSeconds * settings.sampleRate));
    sweep.signal = makeSilence(sweep.startSample + sweep.sweepLength + postSamples, channels);
    for (int index = 0; index < sweep.sweepLength; ++index)
    {
        const double time = index / settings.sampleRate;
        // eq. 33: x(t) = sin(2 pi f1 L exp(t / L))
        const double value = sweep.amplitude * std::sin(kTwoPi * settings.startHz * sweep.rate * std::exp(time / sweep.rate));
        for (int channel = 0; channel < channels; ++channel)
        {
            sweep.signal.setSample(channel, sweep.startSample + index, static_cast<float>(value));
        }
    }
    return sweep;
}

std::complex<double> getInverseFilter(const SweptSine& sweep, double frequencyHz)
{
    // eq. 43: X~(f) = 2 sqrt(f / L) exp(-j 2 pi f L (1 - ln(f / f1)) + j pi / 4)
    const double rate = sweep.rate;
    const double phase = -kTwoPi * frequencyHz * rate * (1.0 - std::log(frequencyHz / sweep.settings.startHz)) + kQuarterPi;
    return kInverseFilterGain * std::sqrt(frequencyHz / rate) * std::polar(1.0, phase);
}

double getHarmonicAdvanceSamples(const SweptSine& sweep, int harmonic)
{
    return sweep.rate * std::log(static_cast<double>(harmonic)) * sweep.settings.sampleRate;
}

std::vector<float> deconvolveSweptSine(const SweptSine& sweep, const std::vector<float>& response, int fftSize)
{
    // the response from the first sample of the sweep on (the pre-silence is not part of the deconvolution)
    const size_t from = std::min(response.size(), static_cast<size_t>(sweep.startSample));
    const size_t available = response.size() - from;
    if (fftSize <= 0)
    {
        fftSize = juce::nextPowerOfTwo(static_cast<int>(available));
    }
    const int order = juce::roundToInt(std::log2(fftSize));
    juce::dsp::FFT fft(order);
    std::vector<juce::dsp::Complex<float>> time(static_cast<size_t>(fftSize));
    std::vector<juce::dsp::Complex<float>> spectrum(static_cast<size_t>(fftSize));
    for (size_t index = 0; index < static_cast<size_t>(fftSize) && index < available; ++index)
    {
        time[index] = {response[from + index], 0.0f};
    }
    fft.perform(time.data(), spectrum.data(), false);

    // h = IFFT(Y X~) / (fs A): the continuous inverse filter, the DFT approximation of the Fourier integral and the sweep amplitude
    const double sampleRate = sweep.settings.sampleRate;
    const double scale = 1.0 / (sampleRate * sweep.amplitude);
    spectrum[0] = {0.0f, 0.0f};
    spectrum[static_cast<size_t>(fftSize / 2)] = {0.0f, 0.0f};
    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        const double frequency = bin * sampleRate / fftSize;
        const std::complex<double> filtered =
            std::complex<double>(spectrum[static_cast<size_t>(bin)].real(), spectrum[static_cast<size_t>(bin)].imag()) * getInverseFilter(sweep, frequency) * scale;
        spectrum[static_cast<size_t>(bin)] = {static_cast<float>(filtered.real()), static_cast<float>(filtered.imag())};
        spectrum[static_cast<size_t>(fftSize - bin)] = {static_cast<float>(filtered.real()), static_cast<float>(-filtered.imag())};
    }
    fft.perform(spectrum.data(), time.data(), true);
    std::vector<float> impulseResponse(static_cast<size_t>(fftSize));
    for (size_t index = 0; index < impulseResponse.size(); ++index)
    {
        impulseResponse[index] = time[index].real();
    }
    return impulseResponse;
}
}
