#include "pluginlab/measure/Reverb.h"

#include <algorithm>
#include <cmath>

#include <juce_core/juce_core.h>

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kBlockSeconds = 0.01; // the interrupted noise's energy blocks

double getStandardDeviation(const std::vector<double>& values, double& mean)
{
    mean = 0.0;
    for (const double value : values)
    {
        mean += value;
    }
    mean /= std::max<size_t>(1, values.size());
    double squares = 0.0;
    for (const double value : values)
    {
        squares += (value - mean) * (value - mean);
    }
    return std::sqrt(squares / std::max<size_t>(1, values.size()));
}
}

TimeVariance analyseTimeVariance(const std::vector<double>& output, int count, int periodSamples, const VarianceSettings& settings)
{
    TimeVariance result;
    const double fs = settings.sampleRate;
    std::vector<std::vector<double>> responses;
    for (int index = 0; index < count; ++index)
    {
        const size_t start = static_cast<size_t>(index) * static_cast<size_t>(periodSamples);
        if (start + static_cast<size_t>(periodSamples) > output.size())
        {
            break;
        }
        responses.emplace_back(output.begin() + static_cast<std::ptrdiff_t>(start), output.begin() + static_cast<std::ptrdiff_t>(start + static_cast<size_t>(periodSamples)));
    }
    if (responses.empty())
    {
        return result;
    }
    // the decay of every response alone
    ReverbSettings reverb;
    reverb.sampleRate = fs;
    reverb.latencySamples = settings.latencySamples;
    const ReverbAnalysis first = analyseReverb(responses.front(), reverb);
    for (const std::vector<double>& response : responses)
    {
        const int start = static_cast<int>(first.directSample);
        const Truncation truncation = findTruncation(response, start, fs);
        result.t30PerResponse.push_back(getDecayTimes(getDecayCurve(response, start, truncation), fs).t30);
    }
    result.t30Spread = getStandardDeviation(result.t30PerResponse, result.t30Mean);

    // the strongest early peaks of the first response (local maxima of |h| after the onset, at least peakSearch apart), followed through all
    const std::vector<double>& reference = responses.front();
    const int onset = static_cast<int>(first.directSample + first.preDelaySeconds * fs);
    const int earlyEnd = std::min(static_cast<int>(reference.size()) - settings.peakSearch - 1, onset + static_cast<int>(settings.earlySeconds * fs));
    std::vector<int> candidates;
    for (int sample = std::max(onset, settings.peakSearch + 1); sample < earlyEnd; ++sample)
    {
        const double value = std::abs(reference[static_cast<size_t>(sample)]);
        bool isPeak = value > 0.0;
        for (int offset = -settings.peakSearch; offset <= settings.peakSearch && isPeak; ++offset)
        {
            if (offset != 0 && std::abs(reference[static_cast<size_t>(sample + offset)]) > value)
            {
                isPeak = false;
            }
        }
        if (isPeak)
        {
            candidates.push_back(sample);
        }
    }
    std::sort(candidates.begin(), candidates.end(), [&reference](int a, int b)
              { return std::abs(reference[static_cast<size_t>(a)]) > std::abs(reference[static_cast<size_t>(b)]); });
    if (static_cast<int>(candidates.size()) > settings.peaks)
    {
        candidates.resize(static_cast<size_t>(settings.peaks));
    }
    for (const int position : candidates)
    {
        // in every response: the largest |h| within +-peakSearch of the position, its signed value and its place
        std::vector<double> values;
        std::vector<double> places;
        for (const std::vector<double>& response : responses)
        {
            int best = position;
            for (int offset = -settings.peakSearch; offset <= settings.peakSearch; ++offset)
            {
                if (std::abs(response[static_cast<size_t>(position + offset)]) > std::abs(response[static_cast<size_t>(best)]))
                {
                    best = position + offset;
                }
            }
            values.push_back(response[static_cast<size_t>(best)]);
            places.push_back(best);
        }
        PeakSpread peak;
        peak.timeSeconds = position / fs;
        const double deviation = getStandardDeviation(values, peak.meanValue);
        peak.valueSpread = deviation / std::max(std::abs(peak.meanValue), 1.0e-30);
        double meanPlace = 0.0;
        peak.timeSpreadSamples = getStandardDeviation(places, meanPlace);
        result.largestValueSpread = std::max(result.largestValueSpread, peak.valueSpread);
        result.peaks.push_back(peak);
    }
    result.timeVariant = result.largestValueSpread > settings.spreadVariant;
    return result;
}

std::vector<float> makeInterruptedNoise(int bursts, int burstSamples, int periodSamples, float level, int seed)
{
    std::vector<float> signal(static_cast<size_t>(bursts) * static_cast<size_t>(periodSamples), 0.0f);
    juce::Random random(seed);
    for (int burst = 0; burst < bursts; ++burst)
    {
        for (int sample = 0; sample < burstSamples; ++sample)
        {
            // Gaussian (Box-Muller)
            const double u1 = std::max(1.0e-12, static_cast<double>(random.nextDouble()));
            const double u2 = random.nextDouble();
            signal[static_cast<size_t>(burst) * static_cast<size_t>(periodSamples) + static_cast<size_t>(sample)] =
                level * static_cast<float>(std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2));
        }
    }
    return signal;
}

double getInterruptedNoiseT30(const std::vector<double>& output, int bursts, int burstSamples, int periodSamples, double sampleRate)
{
    const int block = std::max(1, static_cast<int>(kBlockSeconds * sampleRate));
    const int decayBlocks = (periodSamples - burstSamples) / block;
    // the steady level: the last half of the burst; then the decay after the switch-off, both averaged over the bursts in energy
    std::vector<double> energy(static_cast<size_t>(std::max(0, decayBlocks)), 0.0);
    double steady = 0.0;
    int used = 0;
    for (int burst = 0; burst < bursts; ++burst)
    {
        const size_t base = static_cast<size_t>(burst) * static_cast<size_t>(periodSamples);
        if (base + static_cast<size_t>(periodSamples) > output.size())
        {
            break;
        }
        for (int sample = burstSamples / 2; sample < burstSamples; ++sample)
        {
            const double value = output[base + static_cast<size_t>(sample)];
            steady += value * value / (burstSamples - burstSamples / 2);
        }
        for (int index = 0; index < decayBlocks; ++index)
        {
            double sum = 0.0;
            for (int sample = 0; sample < block; ++sample)
            {
                const double value = output[base + static_cast<size_t>(burstSamples + index * block + sample)];
                sum += value * value;
            }
            energy[static_cast<size_t>(index)] += sum / block;
        }
        ++used;
    }
    if (used == 0 || steady <= 0.0)
    {
        return 0.0;
    }
    std::vector<double> curve;
    for (const double value : energy)
    {
        curve.push_back(10.0 * std::log10(std::max(value / steady, 1.0e-30)));
    }
    return getDecayTimes(curve, 1.0 / kBlockSeconds).t30;
}
}
