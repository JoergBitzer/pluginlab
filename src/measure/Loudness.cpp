#include "pluginlab/measure/Loudness.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "pluginlab/measure/Dynamics.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kOffset = -0.691;
constexpr double kAbsoluteGate = -70.0;
constexpr double kRelativeGate = -10.0;
constexpr double kRangeRelativeGate = -20.0;
constexpr double kBlockSeconds = 0.4;
constexpr double kShortTermSeconds = 3.0;
constexpr double kHopSeconds = 0.1;

struct Section
{
    double b0, b1, b2, a1, a2;
};

// the pre-filter (high shelf) and the RLB high-pass from their analog prototypes (the values that give BS.1770's coefficients at 48 kHz)
std::vector<Section> getKWeighting(double sampleRate)
{
    std::vector<Section> sections;
    {
        const double f0 = 1681.974450955533;
        const double gainDb = 3.999843853973347;
        const double q = 0.7071752369554196;
        const double k = std::tan(kPi * f0 / sampleRate);
        const double vh = std::pow(10.0, gainDb / 20.0);
        const double vb = std::pow(vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;
        sections.push_back({(vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0, 2.0 * (k * k - 1.0) / a0,
                            (1.0 - k / q + k * k) / a0});
    }
    {
        const double f0 = 38.13547087602444;
        const double q = 0.5003270373238773;
        const double k = std::tan(kPi * f0 / sampleRate);
        const double a0 = 1.0 + k / q + k * k;
        sections.push_back({1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0});
    }
    return sections;
}

double toLoudness(double weightedMeanSquare)
{
    if (weightedMeanSquare <= 0.0)
    {
        return -1000.0;
    }
    return kOffset + 10.0 * std::log10(weightedMeanSquare);
}

// the summed mean squares of the K-weighted channels in windows of `window` samples every `hop`
std::vector<double> getBlockPowers(const std::vector<std::vector<double>>& weighted, int window, int hop)
{
    std::vector<double> powers;
    if (weighted.empty())
    {
        return powers;
    }
    const int length = static_cast<int>(weighted.front().size());
    std::vector<std::vector<double>> prefix;
    for (const std::vector<double>& channel : weighted)
    {
        std::vector<double> sums(channel.size() + 1, 0.0);
        for (size_t sample = 0; sample < channel.size(); ++sample)
        {
            sums[sample + 1] = sums[sample] + channel[sample] * channel[sample];
        }
        prefix.push_back(sums);
    }
    for (int start = 0; start + window <= length; start += hop)
    {
        double power = 0.0;
        for (const std::vector<double>& sums : prefix)
        {
            power += (sums[static_cast<size_t>(start + window)] - sums[static_cast<size_t>(start)]) / window;
        }
        powers.push_back(power);
    }
    return powers;
}

double getPercentile(std::vector<double> values, double share)
{
    std::sort(values.begin(), values.end());
    const double position = share * static_cast<double>(values.size() - 1);
    const size_t below = static_cast<size_t>(std::floor(position));
    const size_t above = std::min(values.size() - 1, below + 1);
    return values[below] + (position - static_cast<double>(below)) * (values[above] - values[below]);
}
}

std::vector<double> applyKWeighting(const std::vector<double>& signal, double sampleRate)
{
    std::vector<double> output = signal;
    for (const Section& section : getKWeighting(sampleRate))
    {
        double z1 = 0.0;
        double z2 = 0.0;
        for (double& value : output)
        {
            const double input = value;
            value = section.b0 * input + z1;
            z1 = section.b1 * input - section.a1 * value + z2;
            z2 = section.b2 * input - section.a2 * value;
        }
    }
    return output;
}

LoudnessResult measureLoudness(const std::vector<std::vector<double>>& channels, double sampleRate)
{
    LoudnessResult result;
    if (channels.empty() || channels.front().empty())
    {
        return result;
    }
    std::vector<std::vector<double>> weighted;
    for (const std::vector<double>& channel : channels)
    {
        weighted.push_back(applyKWeighting(channel, sampleRate));
    }
    const int block = static_cast<int>(std::lround(kBlockSeconds * sampleRate));
    const int hop = static_cast<int>(std::lround(kHopSeconds * sampleRate)); // 75 % overlap of the 400 ms blocks

    // the integrated loudness: absolute gate, then relative gate
    const std::vector<double> powers = getBlockPowers(weighted, block, hop);
    for (const double power : powers)
    {
        result.momentaryLufs.push_back(toLoudness(power));
    }
    double sum = 0.0;
    int count = 0;
    for (const double power : powers)
    {
        if (toLoudness(power) > kAbsoluteGate)
        {
            sum += power;
            ++count;
        }
    }
    if (count > 0)
    {
        const double relative = toLoudness(sum / count) + kRelativeGate;
        double gatedSum = 0.0;
        int gatedCount = 0;
        for (const double power : powers)
        {
            const double loudness = toLoudness(power);
            if (loudness > kAbsoluteGate && loudness > relative)
            {
                gatedSum += power;
                ++gatedCount;
            }
        }
        if (gatedCount > 0)
        {
            result.integratedLufs = toLoudness(gatedSum / gatedCount);
        }
    }

    // the loudness range (EBU Tech 3342)
    const std::vector<double> shortTerm = getBlockPowers(weighted, static_cast<int>(std::lround(kShortTermSeconds * sampleRate)), hop);
    std::vector<double> absolutelyGated;
    double shortSum = 0.0;
    for (const double power : shortTerm)
    {
        const double loudness = toLoudness(power);
        result.shortTermLufs.push_back(loudness);
        if (loudness > kAbsoluteGate)
        {
            absolutelyGated.push_back(loudness);
            shortSum += power;
        }
    }
    if (! absolutelyGated.empty())
    {
        const double relative = toLoudness(shortSum / static_cast<double>(absolutelyGated.size())) + kRangeRelativeGate;
        std::vector<double> gated;
        for (const double loudness : absolutelyGated)
        {
            if (loudness > relative)
            {
                gated.push_back(loudness);
            }
        }
        if (gated.size() > 1)
        {
            result.loudnessRangeLu = getPercentile(gated, 0.95) - getPercentile(gated, 0.10);
        }
    }

    // peaks and the crest factor (the loudest channel)
    for (const std::vector<double>& channel : channels)
    {
        result.samplePeakDbfs = std::max(result.samplePeakDbfs, getSamplePeakDbfs(channel));
        result.truePeakDbfs = std::max(result.truePeakDbfs, getTruePeakDbfs(channel));
        double squares = 0.0;
        for (const double value : channel)
        {
            squares += value * value;
        }
        const double rms = 10.0 * std::log10(std::max(squares / static_cast<double>(channel.size()), 1.0e-30));
        result.crestFactorDb = std::max(result.crestFactorDb, getSamplePeakDbfs(channel) - rms);
    }
    return result;
}

GainCurve getGainCurve(const std::vector<double>& input, const std::vector<double>& output, int lagSamples, double sampleRate, double windowSeconds, double floorDb)
{
    GainCurve curve;
    const int window = std::max(1, static_cast<int>(std::lround(windowSeconds * sampleRate)));
    size_t usable = 0;
    if (lagSamples >= 0 && output.size() >= static_cast<size_t>(lagSamples))
    {
        usable = std::min(input.size(), output.size() - static_cast<size_t>(lagSamples));
    }
    const int length = static_cast<int>(usable);
    std::vector<double> energies;
    std::vector<double> crosses;
    for (int start = 0; start + window <= length; start += window)
    {
        double energy = 0.0;
        double cross = 0.0;
        for (int sample = start; sample < start + window; ++sample)
        {
            const double x = input[static_cast<size_t>(sample)];
            energy += x * x;
            cross += x * output[static_cast<size_t>(sample + lagSamples)];
        }
        energies.push_back(energy);
        crosses.push_back(cross);
    }
    double loudest = 0.0;
    for (const double energy : energies)
    {
        loudest = std::max(loudest, energy);
    }
    const double floor = loudest * std::pow(10.0, -floorDb / 10.0);
    for (size_t index = 0; index < energies.size(); ++index)
    {
        curve.times.push_back((static_cast<double>(index) + 0.5) * window / sampleRate);
        curve.inputLevelDbfs.push_back(10.0 * std::log10(std::max(energies[index] / window, 1.0e-30)));
        double gain = std::numeric_limits<double>::quiet_NaN();
        if (energies[index] > floor && energies[index] > 0.0 && crosses[index] > 0.0)
        {
            gain = 20.0 * std::log10(crosses[index] / energies[index]);
        }
        curve.gainDb.push_back(gain);
    }
    return curve;
}
}
