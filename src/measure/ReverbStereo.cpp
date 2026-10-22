#include "pluginlab/measure/Reverb.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include <juce_dsp/juce_dsp.h>

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTiny = 1.0e-300;
constexpr double kSmoothingSeconds = 0.04; // the IAD's RMS

int getOrderAtLeast(int samples)
{
    int order = 0;
    while ((1 << order) < samples)
    {
        ++order;
    }
    return order;
}

double toDb(double energy)
{
    return 10.0 * std::log10(std::max(energy, kTiny));
}

// the mean of x over the window [n - half, n + half] for every n (a moving average by prefix sums)
std::vector<double> smooth(const std::vector<double>& values, int half)
{
    std::vector<double> prefix(values.size() + 1, 0.0);
    for (size_t index = 0; index < values.size(); ++index)
    {
        prefix[index + 1] = prefix[index] + values[index];
    }
    std::vector<double> result(values.size());
    for (size_t index = 0; index < values.size(); ++index)
    {
        const size_t from = static_cast<size_t>(std::max(0, static_cast<int>(index) - half));
        const size_t to = std::min(values.size(), index + static_cast<size_t>(half) + 1);
        result[index] = (prefix[to] - prefix[from]) / static_cast<double>(to - from);
    }
    return result;
}
}

CrossCorrelation getIacc(const std::vector<double>& left, const std::vector<double>& right, int start, double fromSeconds, double toSeconds, double sampleRate)
{
    CrossCorrelation result;
    const int length = static_cast<int>(std::min(left.size(), right.size()));
    const int from = std::clamp(start + static_cast<int>(std::lround(fromSeconds * sampleRate)), 0, length);
    const int to = std::clamp(start + static_cast<int>(std::lround(toSeconds * sampleRate)), from, length);
    const int maximumLag = static_cast<int>(std::lround(0.001 * sampleRate));
    double energyLeft = 0.0;
    double energyRight = 0.0;
    for (int sample = from; sample < to; ++sample)
    {
        energyLeft += left[static_cast<size_t>(sample)] * left[static_cast<size_t>(sample)];
        energyRight += right[static_cast<size_t>(sample)] * right[static_cast<size_t>(sample)];
    }
    const double norm = std::sqrt(energyLeft * energyRight);
    if (norm <= 0.0)
    {
        return result;
    }
    // r(k) = sum L(n) R(n + k) / norm, both cut to [from, to); the value of the largest magnitude, with its sign
    for (int lag = -maximumLag; lag <= maximumLag; ++lag)
    {
        double sum = 0.0;
        for (int sample = from; sample < to; ++sample)
        {
            const int other = sample + lag;
            if (other >= from && other < to)
            {
                sum += left[static_cast<size_t>(sample)] * right[static_cast<size_t>(other)];
            }
        }
        const double value = sum / norm;
        if (std::abs(value) > std::abs(result.value))
        {
            result.value = value;
            result.lagSeconds = lag / sampleRate;
        }
    }
    return result;
}

Coherence getCoherence(const std::vector<double>& left, const std::vector<double>& right, int start, int end, double sampleRate, int frameSize)
{
    Coherence result;
    const int order = getOrderAtLeast(frameSize);
    frameSize = 1 << order;
    end = std::min({end, static_cast<int>(left.size()), static_cast<int>(right.size())});
    const int bins = frameSize / 2 + 1;
    juce::dsp::FFT fft(order);
    std::vector<double> window(static_cast<size_t>(frameSize));
    for (int index = 0; index < frameSize; ++index)
    {
        window[static_cast<size_t>(index)] = 0.5 - 0.5 * std::cos(2.0 * kPi * index / frameSize);
    }
    std::vector<double> powerLeft(static_cast<size_t>(bins), 0.0), powerRight(static_cast<size_t>(bins), 0.0);
    std::vector<std::complex<double>> cross(static_cast<size_t>(bins));
    std::vector<juce::dsp::Complex<float>> timeLeft(static_cast<size_t>(frameSize)), timeRight(static_cast<size_t>(frameSize));
    std::vector<juce::dsp::Complex<float>> spectrumLeft(static_cast<size_t>(frameSize)), spectrumRight(static_cast<size_t>(frameSize));
    for (int from = start; from + frameSize <= end; from += frameSize / 2)
    {
        for (int index = 0; index < frameSize; ++index)
        {
            timeLeft[static_cast<size_t>(index)] = static_cast<float>(left[static_cast<size_t>(from + index)] * window[static_cast<size_t>(index)]);
            timeRight[static_cast<size_t>(index)] = static_cast<float>(right[static_cast<size_t>(from + index)] * window[static_cast<size_t>(index)]);
        }
        fft.perform(timeLeft.data(), spectrumLeft.data(), false);
        fft.perform(timeRight.data(), spectrumRight.data(), false);
        for (int bin = 0; bin < bins; ++bin)
        {
            const std::complex<double> l(spectrumLeft[static_cast<size_t>(bin)].real(), spectrumLeft[static_cast<size_t>(bin)].imag());
            const std::complex<double> r(spectrumRight[static_cast<size_t>(bin)].real(), spectrumRight[static_cast<size_t>(bin)].imag());
            powerLeft[static_cast<size_t>(bin)] += std::norm(l);
            powerRight[static_cast<size_t>(bin)] += std::norm(r);
            cross[static_cast<size_t>(bin)] += l * std::conj(r);
        }
    }
    for (int bin = 0; bin < bins; ++bin)
    {
        result.frequencies.push_back(bin * sampleRate / frameSize);
        const double denominator = powerLeft[static_cast<size_t>(bin)] * powerRight[static_cast<size_t>(bin)];
        double value = 0.0;
        if (denominator > 0.0)
        {
            value = std::norm(cross[static_cast<size_t>(bin)]) / denominator;
        }
        result.coherence.push_back(value);
    }
    return result;
}

InterauralDifference getInterauralDifference(const std::vector<double>& left, const std::vector<double>& right, double sampleRate, double lowestCentreHz,
                                             double highestCentreHz, double stepSeconds)
{
    InterauralDifference result;
    const size_t length = std::min(left.size(), right.size());
    const int half = static_cast<int>(kSmoothingSeconds * sampleRate / 2.0);
    const size_t step = static_cast<size_t>(std::max(1.0, stepSeconds * sampleRate));
    for (size_t sample = 0; sample < length; sample += step)
    {
        result.times.push_back(static_cast<double>(sample) / sampleRate);
    }
    for (const Band& band : getBands(1, lowestCentreHz, highestCentreHz))
    {
        if (band.highHz >= 0.5 * sampleRate)
        {
            continue;
        }
        const std::vector<double> l = filterBand(std::vector<double>(left.begin(), left.begin() + static_cast<std::ptrdiff_t>(length)), band, sampleRate);
        const std::vector<double> r = filterBand(std::vector<double>(right.begin(), right.begin() + static_cast<std::ptrdiff_t>(length)), band, sampleRate);
        std::vector<double> difference(length);
        std::vector<double> sum(length);
        for (size_t sample = 0; sample < length; ++sample)
        {
            difference[sample] = (l[sample] - r[sample]) * (l[sample] - r[sample]);
            sum[sample] = l[sample] * l[sample] + r[sample] * r[sample];
        }
        const std::vector<double> smoothedDifference = smooth(difference, half);
        const std::vector<double> smoothedSum = smooth(sum, half);
        std::vector<double> normalized;
        std::vector<double> plain;
        for (size_t sample = 0; sample < length; sample += step)
        {
            normalized.push_back(toDb(smoothedDifference[sample] / std::max(smoothedSum[sample], kTiny)));
            plain.push_back(toDb(smoothedDifference[sample]));
        }
        result.bands.push_back(band);
        result.normalizedDb.push_back(normalized);
        result.differenceDb.push_back(plain);
    }
    return result;
}

SpatialDistribution getSpatialDistribution(const std::vector<double>& left, const std::vector<double>& right, double sampleRate, double blockSeconds, int sectors,
                                           double floorDb)
{
    SpatialDistribution result;
    const int length = static_cast<int>(std::min(left.size(), right.size()));
    const int block = std::max(2, static_cast<int>(std::lround(blockSeconds * sampleRate)));
    const int hop = block / 2;
    const double width = 2.0 * kPi / sectors;
    for (int sector = 0; sector < sectors; ++sector)
    {
        result.sectorCentres.push_back(-kPi + sector * width); // -pi (nL/nR), ..., -pi/4 (L), 0 (C), pi/4 (R), ...
    }
    result.levelsDb.assign(static_cast<size_t>(sectors), {});
    for (int from = 0; from + block <= length; from += hop)
    {
        std::vector<double> sums(static_cast<size_t>(sectors), 0.0);
        std::vector<int> counts(static_cast<size_t>(sectors), 0);
        for (int sample = from; sample < from + block; ++sample)
        {
            const double l = left[static_cast<size_t>(sample)];
            const double r = right[static_cast<size_t>(sample)];
            const double radius = l * l + r * r;
            if (radius <= 0.0)
            {
                continue;
            }
            double angle = std::atan2(r, l) - kPi / 4.0;
            if (angle <= -kPi)
            {
                angle += 2.0 * kPi;
            }
            // the sector whose centre is nearest (sector 0 is centred on -pi = pi: it takes both ends)
            int sector = static_cast<int>(std::lround((angle + kPi) / width));
            sector %= sectors;
            sums[static_cast<size_t>(sector)] += radius;
            counts[static_cast<size_t>(sector)] += 1;
        }
        for (int sector = 0; sector < sectors; ++sector)
        {
            double level = floorDb;
            if (counts[static_cast<size_t>(sector)] > 0)
            {
                level = std::max(floorDb, toDb(sums[static_cast<size_t>(sector)] / counts[static_cast<size_t>(sector)]));
            }
            result.levelsDb[static_cast<size_t>(sector)].push_back(level);
        }
        result.times.push_back((from + 0.5 * block) / sampleRate);
    }
    return result;
}

LateralDecay getLateralDecay(const std::vector<double>& left, const std::vector<double>& right, int directSample, double sampleRate, double directWindowSeconds)
{
    LateralDecay result;
    const int length = static_cast<int>(std::min(left.size(), right.size()));
    const int window = static_cast<int>(std::lround(directWindowSeconds * sampleRate));
    const int start = std::clamp(directSample, 0, length);
    // the backward sums from the direct sound on, the direct sound itself left out
    std::vector<double> difference(static_cast<size_t>(length - start) + 1, 0.0);
    std::vector<double> energy(static_cast<size_t>(length - start) + 1, 0.0);
    for (int sample = length - 1; sample >= start; --sample)
    {
        double l = left[static_cast<size_t>(sample)];
        double r = right[static_cast<size_t>(sample)];
        if (std::abs(sample - directSample) <= window)
        {
            l = 0.0;
            r = 0.0;
        }
        const size_t index = static_cast<size_t>(sample - start);
        difference[index] = difference[index + 1] + (l - r) * (l - r);
        energy[index] = energy[index + 1] + l * l + r * r;
    }
    const double total = energy.front();
    if (total <= 0.0)
    {
        return result;
    }
    for (size_t index = 0; index + 1 < energy.size(); ++index)
    {
        result.differenceCurveDb.push_back(toDb(difference[index] / total));
        result.energyCurveDb.push_back(toDb(energy[index] / total));
    }
    // the -10 dB point times 6
    if (! result.differenceCurveDb.empty() && result.differenceCurveDb.front() > -10.0)
    {
        for (size_t index = 0; index < result.differenceCurveDb.size(); ++index)
        {
            if (result.differenceCurveDb[index] < -10.0)
            {
                result.tenDbSeconds = 6.0 * static_cast<double>(index) / sampleRate;
                break;
            }
        }
    }
    // Griesinger: the drop at 350 ms, referred to -60 dB
    const size_t at350 = static_cast<size_t>(std::lround(0.35 * sampleRate));
    if (at350 < result.differenceCurveDb.size() && result.differenceCurveDb[at350] < 0.0)
    {
        result.at350msSeconds = 60.0 * 0.35 / -result.differenceCurveDb[at350];
    }
    return result;
}
}
