#include "pluginlab/measure/NullTest.h"

#include <cmath>
#include <complex>

#include <juce_dsp/juce_dsp.h>

#include "pluginlab/measure/FrequencyResponse.h"
#include "pluginlab/signals/Signals.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kBandLowHz = 20.0;

int getOrderAtLeast(int samples)
{
    int order = 0;
    while ((1 << order) < samples)
    {
        ++order;
    }
    return order;
}

double toDb(double powerRatio)
{
    return 10.0 * std::log10(std::max(powerRatio, 1.0e-30));
}

// r[k] = sum_n a[n] b[n + k] for k = -maximumLag ... maximumLag (b given from index start - maximumLag on), by FFT; returns the k of the largest |r|
int findIntegerDelay(const std::vector<double>& a, const std::vector<double>& b, int start, int length, int maximumLag)
{
    const int order = getOrderAtLeast(length + 2 * maximumLag);
    const int size = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<juce::dsp::Complex<float>> aTime(static_cast<size_t>(size)), bTime(static_cast<size_t>(size));
    std::vector<juce::dsp::Complex<float>> aSpectrum(static_cast<size_t>(size)), bSpectrum(static_cast<size_t>(size));
    for (int index = 0; index < length; ++index)
    {
        aTime[static_cast<size_t>(index)] = static_cast<float>(a[static_cast<size_t>(start + index)]);
    }
    for (int index = 0; index < length + 2 * maximumLag; ++index)
    {
        const int source = start - maximumLag + index;
        if (source >= 0 && source < static_cast<int>(b.size()))
        {
            bTime[static_cast<size_t>(index)] = static_cast<float>(b[static_cast<size_t>(source)]);
        }
    }
    fft.perform(aTime.data(), aSpectrum.data(), false);
    fft.perform(bTime.data(), bSpectrum.data(), false);
    for (size_t index = 0; index < bSpectrum.size(); ++index)
    {
        bSpectrum[index] *= std::conj(aSpectrum[index]);
    }
    fft.perform(bSpectrum.data(), bTime.data(), true);
    int best = 0;
    for (int index = 1; index <= 2 * maximumLag; ++index)
    {
        if (std::abs(bTime[static_cast<size_t>(index)].real()) > std::abs(bTime[static_cast<size_t>(best)].real()))
        {
            best = index;
        }
    }
    return best - maximumLag;
}

// The Hann-windowed spectrum of data[first, first + length)
std::vector<std::complex<double>> getWindowedSpectrum(const std::vector<double>& data, int first, int length)
{
    std::vector<double> windowed(static_cast<size_t>(length));
    for (int index = 0; index < length; ++index)
    {
        const double weight = 0.5 - 0.5 * std::cos(2.0 * kPi * index / length);
        const int source = first + index;
        double value = 0.0;
        if (source >= 0 && source < static_cast<int>(data.size()))
        {
            value = data[static_cast<size_t>(source)];
        }
        windowed[static_cast<size_t>(index)] = weight * value;
    }
    return getSpectrum(windowed, 0, length);
}

std::vector<double> getChannel(const juce::AudioBuffer<float>& buffer, int channel)
{
    const float* data = buffer.getReadPointer(std::min(channel, buffer.getNumChannels() - 1));
    return std::vector<double>(data, data + buffer.getNumSamples());
}
}

NullTestResult measureNull(const Device& a, const Device& b, const NullTestSettings& settings)
{
    NullTestResult result;
    result.settings = settings;
    const int start = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    const int length = settings.windowSamples;
    const int maximumLag = static_cast<int>(std::round(settings.maximumDelaySeconds * settings.sampleRate));
    const int total = start + length + maximumLag;

    juce::AudioBuffer<float> stimulus;
    if (settings.stimulus.getNumSamples() > 0)
    {
        stimulus.makeCopyOf(settings.stimulus);
    }
    else
    {
        signals::NoiseSettings noise;
        noise.colour = signals::NoiseColour::WhiteGaussian;
        noise.levelDbfsRms = 20.0 * std::log10(dbfsToRms(settings.levelDbfs)); // the generator's level is the sample rms
        noise.length = total;
        noise.seed = settings.seed;
        stimulus = signals::makeNoise(noise, settings.channels, signals::ChannelRelation::Uncorrelated);
    }
    const juce::AudioBuffer<float> outputA = a(stimulus, settings.sampleRate);
    const juce::AudioBuffer<float> outputB = b(stimulus, settings.sampleRate);

    const double binWidth = settings.sampleRate / length;
    const int firstBin = static_cast<int>(std::ceil(kBandLowHz / binWidth));
    const int lastBin = std::min(static_cast<int>(std::floor(std::min(kUpperBandEdgeHz, 0.5 * settings.sampleRate) / binWidth)), length / 2);
    double windowPower = 0.0;
    for (int index = 0; index < length; ++index)
    {
        const double weight = 0.5 - 0.5 * std::cos(2.0 * kPi * index / length);
        windowPower += weight * weight;
    }

    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<double> dataA = getChannel(outputA, channel);
        const std::vector<double> dataB = getChannel(outputB, channel);
        ChannelNull measured;
        const std::vector<std::complex<double>> spectrumA = getWindowedSpectrum(dataA, start, length);

        // unaligned: the same window of both
        const std::vector<std::complex<double>> plainB = getWindowedSpectrum(dataB, start, length);
        double powerA = 0.0;
        double powerUnaligned = 0.0;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            powerA += std::norm(spectrumA[static_cast<size_t>(bin)]);
            powerUnaligned += std::norm(spectrumA[static_cast<size_t>(bin)] - plainB[static_cast<size_t>(bin)]);
        }
        measured.unalignedNullDb = toDb(powerUnaligned / powerA);

        // delay: the integer part from the cross-correlation, the fraction from the phase slope of the cross-spectrum (weighted least squares)
        int integerDelay = 0;
        if (settings.alignDelay)
        {
            integerDelay = -findIntegerDelay(dataA, dataB, start, length, maximumLag);
        }
        const std::vector<std::complex<double>> spectrumB = getWindowedSpectrum(dataB, start - integerDelay, length);
        double fraction = 0.0;
        if (settings.alignDelay)
        {
            // A = g B e^{-j omega fraction}: the phase of A conj(B) is -omega fraction, turned by pi for g < 0 (the sign of the real cross power tells)
            double realCross = 0.0;
            for (int bin = firstBin; bin <= lastBin; ++bin)
            {
                realCross += (spectrumA[static_cast<size_t>(bin)] * std::conj(spectrumB[static_cast<size_t>(bin)])).real();
            }
            double sign = 1.0;
            if (realCross < 0.0)
            {
                sign = -1.0;
            }
            double numerator = 0.0;
            double denominator = 0.0;
            for (int bin = firstBin; bin <= lastBin; ++bin)
            {
                const std::complex<double> cross = sign * spectrumA[static_cast<size_t>(bin)] * std::conj(spectrumB[static_cast<size_t>(bin)]);
                const double omega = 2.0 * kPi * bin / length;
                const double weight = std::abs(cross);
                numerator += weight * omega * std::arg(cross);
                denominator += weight * omega * omega;
            }
            fraction = -numerator / denominator;
        }
        measured.delaySamples = integerDelay + fraction;

        // B delayed by the fraction (a phase ramp), then the least-squares gain g = Re sum A conj(B') / sum |B'|^2
        std::vector<std::complex<double>> alignedB(spectrumB.size());
        double crossPower = 0.0;
        double powerB = 0.0;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            const double omega = 2.0 * kPi * bin / length;
            alignedB[static_cast<size_t>(bin)] = spectrumB[static_cast<size_t>(bin)] * std::polar(1.0, -omega * fraction);
            crossPower += (spectrumA[static_cast<size_t>(bin)] * std::conj(alignedB[static_cast<size_t>(bin)])).real();
            powerB += std::norm(alignedB[static_cast<size_t>(bin)]);
        }
        double gain = 1.0;
        if (settings.alignGain)
        {
            gain = crossPower / powerB;
        }
        measured.gainDb = 20.0 * std::log10(std::max(std::abs(gain), 1.0e-30));
        measured.inverted = gain < 0.0;

        const std::vector<double> centres = getStandardThirdOctaveFrequencies();
        std::vector<double> bandA(centres.size(), 0.0);
        std::vector<double> bandResidual(centres.size(), 0.0);
        const double edge = std::pow(2.0, 1.0 / 6.0);
        double residualPower = 0.0;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            const std::complex<double> residual = spectrumA[static_cast<size_t>(bin)] - gain * alignedB[static_cast<size_t>(bin)];
            residualPower += std::norm(residual);
            const double f = bin * binWidth;
            for (size_t band = 0; band < centres.size(); ++band)
            {
                if (f >= centres[band] / edge && f < centres[band] * edge)
                {
                    bandA[band] += std::norm(spectrumA[static_cast<size_t>(bin)]);
                    bandResidual[band] += std::norm(residual);
                }
            }
        }
        measured.nullDepthDb = toDb(residualPower / powerA);
        // the rms in the band: 2 |X|^2 per bin of a real signal, divided by the window's power
        measured.residualDbfs = rmsToDbfs(std::sqrt(2.0 * residualPower / (length * windowPower)));
        for (size_t band = 0; band < centres.size(); ++band)
        {
            if (bandA[band] > 0.0)
            {
                measured.thirdOctaveHz.push_back(centres[band]);
                measured.thirdOctaveResidualDb.push_back(toDb(bandResidual[band] / bandA[band]));
            }
        }
        result.channels.push_back(measured);
    }
    return result;
}
}
