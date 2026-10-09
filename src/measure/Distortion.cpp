#include "pluginlab/measure/Distortion.h"

#include <cmath>
#include <limits>

#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/signals/Signals.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kPassbandLowHz = 20.0;          // AES17 3.4: the passband from 20 Hz to the upper band-edge frequency
constexpr double kTwoPi = 6.28318530717958647692;

int getPowerOfTwoAtLeast(double samples)
{
    int size = 1;
    while (size < samples)
    {
        size *= 2;
    }
    return size;
}

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}

double getMeanRemovedRms(const std::vector<double>& data, int start, int length)
{
    double mean = 0.0;
    for (int index = start; index < start + length; ++index)
    {
        mean += data[static_cast<size_t>(index)];
    }
    mean /= length;
    double sum = 0.0;
    for (int index = start; index < start + length; ++index)
    {
        const double value = data[static_cast<size_t>(index)] - mean;
        sum += value * value;
    }
    return std::sqrt(sum / length);
}
}

DistortionResult measureDistortion(const Device& device, const DistortionSettings& settings)
{
    DistortionResult result;
    result.settings = settings;
    result.windowSamples = getPowerOfTwoAtLeast(settings.measureSeconds * settings.sampleRate);
    result.frequencyHz = getCoherentFrequency(settings.frequencyHz, settings.sampleRate, result.windowSamples);
    const int start = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    const int length = result.windowSamples;

    signals::SineSettings sine;
    sine.sampleRate = settings.sampleRate;
    sine.frequencyHz = result.frequencyHz;
    sine.levelDbfsPeak = settings.levelDbfs;
    sine.length = start + length;
    const juce::AudioBuffer<float> input = signals::makeSine(sine, settings.channels);
    const juce::AudioBuffer<float> output = device(input, settings.sampleRate);

    const StandardLowPass lowPass(settings.sampleRate);
    const double binWidth = settings.sampleRate / length;
    const int fundamentalBin = static_cast<int>(std::round(result.frequencyHz / binWidth));
    const double upperEdge = std::min(kUpperBandEdgeHz, 0.5 * settings.sampleRate);
    const int lowestBin = static_cast<int>(std::ceil(kPassbandLowHz / binWidth));
    const int highestBin = std::min(static_cast<int>(std::floor(upperEdge / binWidth)), length / 2);
    const reference::BiquadCoefficients notch = reference::designRbj(reference::FilterType::Notch, settings.sampleRate, result.frequencyHz, 0.0, settings.notchQ);

    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<double> out = lowPass.process(output.getReadPointer(channel), output.getNumSamples());
        ChannelDistortion distortion;
        // frequency domain (AES17 annex A.2/A.3.6): synchronous window, no window function; every harmonic on its own bin
        const std::vector<std::complex<double>> spectrum = getSpectrum(out, start, length);
        const auto amplitude = [&spectrum, length](int bin) { return 2.0 * std::abs(spectrum[static_cast<size_t>(bin)]) / length; };
        const double fundamental = amplitude(fundamentalBin);
        distortion.fundamentalDbfs = rmsToDbfs(fundamental / std::sqrt(2.0));
        double harmonicPower = 0.0;
        for (int order = 2; order <= settings.maximumHarmonic && order * fundamentalBin <= highestBin; ++order)
        {
            const double level = amplitude(order * fundamentalBin);
            distortion.harmonicOrders.push_back(order);
            distortion.harmonicDb.push_back(toDb(level / fundamental));
            harmonicPower += level * level;
        }
        distortion.thdDb = 10.0 * std::log10(std::max(harmonicPower / (fundamental * fundamental), 1.0e-30));
        distortion.thdPercent = 100.0 * std::sqrt(harmonicPower) / fundamental;
        double total = 0.0;
        double residual = 0.0;
        for (int bin = lowestBin; bin <= highestBin; ++bin)
        {
            const double power = std::norm(spectrum[static_cast<size_t>(bin)]);
            total += power;
            if (bin != fundamentalBin)
            {
                residual += power;
            }
        }
        distortion.thdnDb = 10.0 * std::log10(std::max(residual / total, 1.0e-30));

        // AES17 6.3.1: the standard notch removes the fundamental; residual and total as rms (DC removed: the passband starts at 20 Hz)
        reference::BiquadCascade notchFilter({notch}, settings.sampleRate);
        std::vector<double> notched(out.size());
        for (size_t index = 0; index < out.size(); ++index)
        {
            notched[index] = notchFilter.processSample(0, out[index]);
        }
        distortion.thdnNotchDb = toDb(getMeanRemovedRms(notched, start, length) / getMeanRemovedRms(out, start, length));
        result.channels.push_back(distortion);
    }
    return result;
}

std::vector<DistortionResult> measureDistortionVsLevel(const Device& device, const DistortionSettings& settings, const std::vector<double>& levelsDbfs)
{
    std::vector<DistortionResult> results;
    for (const double level : levelsDbfs)
    {
        DistortionSettings step = settings;
        step.levelDbfs = level;
        results.push_back(measureDistortion(device, step));
    }
    return results;
}

std::vector<DistortionResult> measureDistortionVsFrequency(const Device& device, const DistortionSettings& settings, const std::vector<double>& frequencies)
{
    std::vector<DistortionResult> results;
    for (const double frequency : frequencies)
    {
        DistortionSettings step = settings;
        step.frequencyHz = frequency;
        results.push_back(measureDistortion(device, step));
    }
    return results;
}

namespace
{
// The window of a harmonic response: flat (1) over the inner half on both sides of the harmonic's position, raised-cosine tapers to 0 at the ends.
// The two sides differ in length (the harmonics lie closer together towards higher orders), so a plain Hann window would not have its maximum at the
// harmonic (order 2: weight 0.84, -1.5 dB).
double getHarmonicWindow(int offset, int before, int after)
{
    const int distance = offset - before;
    int side = after;
    if (distance < 0)
    {
        side = before;
    }
    const double position = std::abs(static_cast<double>(distance)) / std::max(side, 1);
    if (position <= 0.5)
    {
        return 1.0;
    }
    return 0.5 + 0.5 * std::cos(kTwoPi * (position - 0.5));
}
}

HarmonicResponse measureSweptHarmonics(const Device& device, const SweepResponseSettings& settings, int maximumHarmonic, const std::vector<double>& frequencies)
{
    HarmonicResponse result;
    result.frequencyHz = frequencies;
    result.maximumHarmonic = maximumHarmonic;
    const SweptImpulses impulses = measureSweptImpulses(device, settings);
    result.validToHz = impulses.validToHz;
    const double sampleRate = impulses.sampleRate;
    const double rate = impulses.rateSeconds;
    for (const SweptChannel& swept : impulses.channels)
    {
        HarmonicResponse::Channel channel;
        const int size = static_cast<int>(swept.impulse.size());
        // the n-th harmonic impulse response lies L ln(n) before the linear one; its window reaches half way to the neighbouring harmonics
        const auto harmonicSegment = [&](int order, int& firstTime)
        {
            const double centre = swept.peak - rate * std::log(static_cast<double>(order)) * sampleRate;
            const int before = static_cast<int>(0.5 * rate * std::log((order + 1.0) / order) * sampleRate);
            const int after = static_cast<int>(0.5 * rate * std::log(order / (order - 1.0)) * sampleRate);
            const int first = static_cast<int>(std::round(centre)) - before;
            std::vector<double> segment;
            const int total = before + after;
            for (int offset = 0; offset <= total; ++offset)
            {
                const int index = ((first + offset) % size + size) % size;
                segment.push_back(getHarmonicWindow(offset, before, after) * swept.impulse[static_cast<size_t>(index)]);
            }
            firstTime = first;
            return segment;
        };
        std::vector<std::vector<double>> segments;
        std::vector<int> firstTimes;
        for (int order = 2; order <= maximumHarmonic; ++order)
        {
            int firstTime = 0;
            segments.push_back(harmonicSegment(order, firstTime));
            firstTimes.push_back(firstTime);
        }
        channel.harmonicDb.assign(static_cast<size_t>(maximumHarmonic - 1), std::vector<double>());
        for (const double frequency : frequencies)
        {
            const double fundamental = std::abs(getDtft(swept.segment, swept.firstTime, frequency, sampleRate)
                                                / getDtft(impulses.referenceSegment, impulses.referenceFirstTime, frequency, sampleRate));
            double power = 0.0;
            for (int order = 2; order <= maximumHarmonic; ++order)
            {
                const double harmonicFrequency = order * frequency;
                double level = std::numeric_limits<double>::quiet_NaN();
                if (harmonicFrequency <= result.validToHz)
                {
                    const size_t index = static_cast<size_t>(order - 2);
                    const double magnitude = std::abs(getDtft(segments[index], firstTimes[index], harmonicFrequency, sampleRate)
                                                      / getDtft(impulses.referenceSegment, impulses.referenceFirstTime, harmonicFrequency, sampleRate));
                    level = toDb(magnitude / fundamental);
                    power += (magnitude / fundamental) * (magnitude / fundamental);
                }
                channel.harmonicDb[static_cast<size_t>(order - 2)].push_back(level);
            }
            channel.thdDb.push_back(10.0 * std::log10(std::max(power, 1.0e-30)));
        }
        result.channels.push_back(channel);
    }
    return result;
}
}
