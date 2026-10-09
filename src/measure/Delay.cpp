#include "pluginlab/measure/Delay.h"

#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "pluginlab/signals/Signals.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kNoiseSeed = 31;
constexpr double kSecondToneRatio = 2.0;        // AES17 6.2.8 b: 997 Hz and 1994 Hz
constexpr double kSecondTonePhase = -kPi / 2.0; // the second tone shifted by minus a quarter cycle

// The vertex of the parabola through (index - 1, index, index + 1)
double interpolatePeak(double before, double at, double after, int index)
{
    const double curvature = before - 2.0 * at + after;
    if (std::abs(curvature) < 1.0e-30)
    {
        return index;
    }
    return index + 0.5 * (before - after) / curvature;
}

int getPowerOfTwoAtLeast(int samples)
{
    int size = 1;
    while (size < samples)
    {
        size *= 2;
    }
    return size;
}

// r[k] = sum x[n] y[n + k] for k = 0 ... maximumLag (by FFT)
std::vector<double> crossCorrelate(const float* x, const float* y, int length, int maximumLag)
{
    const int size = getPowerOfTwoAtLeast(2 * (length + maximumLag));
    const int order = juce::roundToInt(std::log2(size));
    juce::dsp::FFT fft(order);
    std::vector<juce::dsp::Complex<float>> xTime(static_cast<size_t>(size)), yTime(static_cast<size_t>(size));
    std::vector<juce::dsp::Complex<float>> xSpectrum(static_cast<size_t>(size)), ySpectrum(static_cast<size_t>(size));
    for (int index = 0; index < length; ++index)
    {
        xTime[static_cast<size_t>(index)] = {x[index], 0.0f};
    }
    for (int index = 0; index < length + maximumLag; ++index)
    {
        yTime[static_cast<size_t>(index)] = {y[index], 0.0f};
    }
    fft.perform(xTime.data(), xSpectrum.data(), false);
    fft.perform(yTime.data(), ySpectrum.data(), false);
    for (size_t bin = 0; bin < xSpectrum.size(); ++bin)
    {
        ySpectrum[bin] = ySpectrum[bin] * std::conj(xSpectrum[bin]);
    }
    fft.perform(ySpectrum.data(), yTime.data(), true);
    std::vector<double> correlation(static_cast<size_t>(maximumLag + 1));
    for (int lag = 0; lag <= maximumLag; ++lag)
    {
        correlation[static_cast<size_t>(lag)] = yTime[static_cast<size_t>(lag)].real();
    }
    return correlation;
}
}

DelayResult measureDelay(const Device& device, const DelaySettings& settings)
{
    DelayResult result;
    result.settings = settings;
    const double amplitude = std::pow(10.0, settings.levelDbfs / 20.0);

    // a: the impulse response by the synchronized sweep
    SweepResponseSettings sweep;
    sweep.sampleRate = settings.sampleRate;
    sweep.levelDbfs = settings.levelDbfs;
    sweep.channels = settings.channels;
    const SweptImpulses impulses = measureSweptImpulses(device, sweep);

    // b: cross-correlation with white noise (the output is longer by the largest delay looked for)
    const int noiseLength = static_cast<int>(settings.noiseSeconds * settings.sampleRate);
    const int maximumLag = static_cast<int>(settings.maximumDelaySeconds * settings.sampleRate);
    signals::NoiseSettings noise;
    noise.colour = signals::NoiseColour::WhiteGaussian;
    noise.levelDbfsRms = settings.levelDbfs;
    noise.length = noiseLength;
    noise.seed = kNoiseSeed;
    const juce::AudioBuffer<float> noiseSignal = signals::makeNoise(noise, settings.channels);
    juce::AudioBuffer<float> noiseInput(settings.channels, noiseLength + maximumLag);
    noiseInput.clear();
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        noiseInput.copyFrom(channel, 0, noiseSignal, channel, 0, noiseLength);
    }
    const juce::AudioBuffer<float> noiseOutput = device(noiseInput, settings.sampleRate);

    // polarity b: 997 Hz + 1994 Hz shifted by minus a quarter cycle: one dominant positive peak per period (2 against -1.125 for equal amplitudes)
    const int twoToneLength = static_cast<int>(settings.twoToneSeconds * settings.sampleRate);
    juce::AudioBuffer<float> twoTone(settings.channels, twoToneLength);
    for (int index = 0; index < twoToneLength; ++index)
    {
        const double phase = 2.0 * kPi * kStandardFrequencyHz * index / settings.sampleRate;
        const float value = static_cast<float>(amplitude / 2.0 * (std::sin(phase) + std::sin(kSecondToneRatio * phase + kSecondTonePhase)));
        for (int channel = 0; channel < settings.channels; ++channel)
        {
            twoTone.setSample(channel, index, value);
        }
    }
    const juce::AudioBuffer<float> twoToneOutput = device(twoTone, settings.sampleRate);

    for (int channel = 0; channel < settings.channels; ++channel)
    {
        ChannelDelay delay;
        const SweptChannel& swept = impulses.channels[static_cast<size_t>(channel)];
        const int size = static_cast<int>(swept.impulse.size());
        const auto at = [&swept, size](int index) { return static_cast<double>(swept.impulse[static_cast<size_t>((index % size + size) % size)]); };
        delay.impulsePeakSamples = swept.peak;
        const double peakValue = at(swept.peak);
        const double sign = std::copysign(1.0, peakValue);
        delay.impulsePeakInterpolated = interpolatePeak(sign * at(swept.peak - 1), sign * peakValue, sign * at(swept.peak + 1), swept.peak);
        delay.invertingByImpulse = peakValue < 0.0;
        // the phase delay at a low frequency, unwrapped around the peak: arg(H e^(j w peak)) is small for a delay near the peak
        const double omega = 2.0 * kPi * settings.phaseDelayHz / settings.sampleRate;
        const std::complex<double> response = getSweptResponse(impulses, channel, settings.phaseDelayHz) * std::polar(1.0, omega * swept.peak);
        double residualPhase = std::arg(response);
        if (delay.invertingByImpulse)
        {
            residualPhase = std::arg(-response); // an inversion is not a delay
        }
        delay.phaseDelaySamples = swept.peak - residualPhase / omega;

        const std::vector<double> correlation = crossCorrelate(noiseInput.getReadPointer(channel), noiseOutput.getReadPointer(channel), noiseLength, maximumLag);
        int best = 0;
        for (int lag = 1; lag <= maximumLag; ++lag)
        {
            if (std::abs(correlation[static_cast<size_t>(lag)]) > std::abs(correlation[static_cast<size_t>(best)]))
            {
                best = lag;
            }
        }
        delay.correlationPeakSamples = best;
        const double correlationSign = std::copysign(1.0, correlation[static_cast<size_t>(best)]);
        double before = 0.0;
        double after = 0.0;
        if (best > 0)
        {
            before = correlationSign * correlation[static_cast<size_t>(best - 1)];
        }
        if (best < maximumLag)
        {
            after = correlationSign * correlation[static_cast<size_t>(best + 1)];
        }
        delay.correlationPeakInterpolated = interpolatePeak(before, correlationSign * correlation[static_cast<size_t>(best)], after, best);

        // the last half of the two-tone response: the largest positive against the largest negative value
        double positive = 0.0;
        double negative = 0.0;
        for (int index = twoToneLength / 2; index < twoToneLength; ++index)
        {
            const double value = twoToneOutput.getSample(channel, index);
            positive = std::max(positive, value);
            negative = std::min(negative, value);
        }
        delay.twoToneRatio = positive / std::max(-negative, 1.0e-30);
        delay.invertingByTwoTone = delay.twoToneRatio < 1.0;
        result.channels.push_back(delay);
    }
    return result;
}
}
