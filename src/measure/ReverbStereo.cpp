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

namespace
{
// the first-order low-pass of RLE.m: the bilinear transform of 1 / (1 + s / (2 pi fg)) without pre-warping, b = [1 1] / a0
std::vector<double> lowPassFirstOrder(const std::vector<double>& input, double cutoffHz, double sampleRate)
{
    const double a0 = 1.0 + sampleRate / (kPi * cutoffHz);
    const double a1 = (1.0 - sampleRate / (kPi * cutoffHz)) / a0;
    const double b = 1.0 / a0;
    std::vector<double> output(input.size());
    double previousInput = 0.0;
    double previousOutput = 0.0;
    for (size_t sample = 0; sample < input.size(); ++sample)
    {
        output[sample] = b * (input[sample] + previousInput) - a1 * previousOutput;
        previousInput = input[sample];
        previousOutput = output[sample];
    }
    return output;
}

double getEnergy(const std::vector<double>& values)
{
    double energy = 0.0;
    for (const double value : values)
    {
        energy += value * value;
    }
    return energy;
}

// the segment's energy through the 6 kHz low-pass, padded with 0.1 s of zeros (the filter's tail counts)
double getLowPassedEnergy(const std::vector<double>& segment, double sampleRate)
{
    std::vector<double> padded = segment;
    padded.resize(segment.size() + static_cast<size_t>(std::lround(0.1 * sampleRate)), 0.0);
    return getEnergy(lowPassFirstOrder(padded, 6000.0, sampleRate));
}
}

RoomLevelEarly getRoomLevelEarly(const std::vector<double>& left, const std::vector<double>& right, int directSample, double sampleRate, double tau0Seconds)
{
    RoomLevelEarly result;
    constexpr double kWindowSeconds = 0.002;     // t_window
    constexpr double kDirectSeconds = 0.005;     // tD
    constexpr double kLagSeconds = 0.001;        // tau_max
    constexpr int kHop = 10;                     // samples between the segments (RLE.m, 08-Aug-2003)
    constexpr double kLevelLag = 67.0e-6;        // K: seconds of lag per dB of level difference
    constexpr double kLagSpread = 630.0e-6;      // sigma
    constexpr double kGrowth = 0.015;            // TN
    const double fs = sampleRate;
    const int length = static_cast<int>(std::min(left.size(), right.size()));
    const int i0 = std::clamp(directSample, 0, length);
    const int span80 = static_cast<int>(std::lround(0.08 * fs));
    const int regionLength = std::min(length - i0, 4 * span80);
    if (regionLength <= span80)
    {
        return result;
    }
    const std::vector<double> bbLeft(left.begin() + i0, left.begin() + i0 + regionLength);
    const std::vector<double> bbRight(right.begin() + i0, right.begin() + i0 + regionLength);
    const std::vector<double> lpLeft = lowPassFirstOrder(bbLeft, 800.0, fs);
    const std::vector<double> lpRight = lowPassFirstOrder(bbRight, 800.0, fs);

    const int delays = static_cast<int>(std::ceil(kLagSeconds * fs)) - 1;   // the lags 0 ... delays - 1 (RLE.m assumed a time vector from 0)
    const int window = static_cast<int>(std::lround((kWindowSeconds + kLagSeconds) * fs));
    const int directLength = static_cast<int>(std::lround(kDirectSeconds * fs));
    const int lagSamples = static_cast<int>(std::lround(kLagSeconds * fs));
    std::vector<int> ends;
    for (int end = directLength + kHop; end <= span80; end += kHop)
    {
        ends.push_back(end);
    }

    // the lag of a block: the largest of gamma(tau) f(tau) over both directions; f weights the lag the level difference implies
    const auto chooseLag = [&](const std::vector<double>& gammaRl, const std::vector<double>& gammaLr, double levelDifferenceDb)
    {
        double best = -1.0e300;
        double lag = 0.0;
        for (int m = 0; m < delays; ++m)
        {
            const double tauRl = -m / fs;
            const double tauLr = m / fs;
            const double fRl = std::exp(-std::pow(tauRl - levelDifferenceDb * kLevelLag, 2.0) / (2.0 * kLagSpread * kLagSpread));
            const double fLr = std::exp(-std::pow(tauLr - levelDifferenceDb * kLevelLag, 2.0) / (2.0 * kLagSpread * kLagSpread));
            if (gammaRl[static_cast<size_t>(m)] * fRl > best)
            {
                best = gammaRl[static_cast<size_t>(m)] * fRl;
                lag = tauRl;
            }
            if (gammaLr[static_cast<size_t>(m)] * fLr > best)
            {
                best = gammaLr[static_cast<size_t>(m)] * fLr;
                lag = tauLr;
            }
        }
        return lag;
    };

    double numerator = 0.0;
    double rest = 0.0;
    for (const int end : ends)
    {
        const int from = end - window;
        if (from < 0)
        {
            continue;
        }
        // gamma_rl(m) = sum r(n) l(n - m), gamma_lr(m) = sum l(n) r(n - m) over the segment's last window - delays + 1 samples
        std::vector<double> gammaRl(static_cast<size_t>(delays), 0.0);
        std::vector<double> gammaLr(static_cast<size_t>(delays), 0.0);
        const int count = window - delays + 1;
        for (int m = 0; m < delays; ++m)
        {
            for (int k = 0; k < count; ++k)
            {
                const int n = from + delays - 1 + k;
                gammaRl[static_cast<size_t>(m)] += lpRight[static_cast<size_t>(n)] * lpLeft[static_cast<size_t>(n - m)];
                gammaLr[static_cast<size_t>(m)] += lpLeft[static_cast<size_t>(n)] * lpRight[static_cast<size_t>(n - m)];
            }
        }
        // the energies of the segment's window (after the lag part), broadband
        const std::vector<double> segmentLeft(bbLeft.begin() + from + lagSamples - 1, bbLeft.begin() + end);
        const std::vector<double> segmentRight(bbRight.begin() + from + lagSamples - 1, bbRight.begin() + end);
        const double energyLeft = getEnergy(segmentLeft);
        const double energyRight = getEnergy(segmentRight);
        const double levelDifference = 10.0 * std::log10(std::max(energyRight, kTiny) / std::max(energyLeft, kTiny));
        const double tau = chooseLag(gammaRl, gammaLr, levelDifference);
        const double sine = std::min(1.0, std::abs(tau) / tau0Seconds);
        const double time = (end - 1) / fs;
        const double q = 1.0 - std::exp(-(time - kWindowSeconds / 2.0) / kGrowth);
        const double energy = getLowPassedEnergy(segmentLeft, fs) + getLowPassedEnergy(segmentRight, fs);
        numerator += q * energy * sine;
        rest += energy * (1.0 - sine * q);
        result.segmentTimes.push_back(time);
        result.lateralSine.push_back(sine);
        result.segmentEnergy.push_back(energy);
    }
    // the direct sound: its first 5 ms, scaled by the segments' overlap (each sample of the segments is counted window / hop times)
    const double overlap = std::lround(kWindowSeconds * fs) / static_cast<double>(kHop);
    const std::vector<double> directLeft(bbLeft.begin(), bbLeft.begin() + directLength);
    const std::vector<double> directRight(bbRight.begin(), bbRight.begin() + directLength);
    const double direct = overlap * (getLowPassedEnergy(directLeft, fs) + getLowPassedEnergy(directRight, fs));
    result.withDirectDb = toDb(numerator / std::max(direct + rest, kTiny));
    result.withoutDirectDb = toDb(numerator / std::max(rest, kTiny));
    return result;
}
}
