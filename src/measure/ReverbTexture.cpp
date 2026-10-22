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
constexpr double kGaussianOutside = 0.31731050786291415; // erfc(1 / sqrt(2)): the share of Gaussian samples outside one standard deviation
constexpr double kNormalizedDownToDb = -40.0;            // h / sqrt(EDC) is used while the curve is above this (later it is noise of the end)
constexpr double kEchoWithinDb = 20.0;                   // Griesinger: reflections within 20 dB of the block's largest
constexpr double kHistogramRange = 4.0;                  // the histogram from -4 to +4 standard deviations
constexpr double kTiny = 1.0e-300;

int getOrderAtLeast(int samples)
{
    int order = 0;
    while ((1 << order) < samples)
    {
        ++order;
    }
    return order;
}

// third-octave smoothing of a power spectrum (the mean power within +-1/6 octave of each bin)
std::vector<double> smoothThirdOctave(const std::vector<double>& power, double binHz)
{
    std::vector<double> prefix(power.size() + 1, 0.0);
    for (size_t bin = 0; bin < power.size(); ++bin)
    {
        prefix[bin + 1] = prefix[bin] + power[bin];
    }
    const double half = std::pow(2.0, 1.0 / 6.0);
    std::vector<double> smoothed(power.size(), 0.0);
    for (size_t bin = 1; bin < power.size(); ++bin)
    {
        const double frequency = bin * binHz;
        const size_t low = std::max<size_t>(1, static_cast<size_t>(frequency / half / binHz));
        const size_t high = std::min(power.size() - 1, static_cast<size_t>(std::ceil(frequency * half / binHz)));
        smoothed[bin] = (prefix[high + 1] - prefix[low]) / static_cast<double>(high + 1 - low);
    }
    return smoothed;
}
}

Texture analyseTexture(const std::vector<double>& response, int start, int end, const TextureSettings& settings)
{
    Texture result;
    const double fs = settings.sampleRate;
    end = std::min(end, static_cast<int>(response.size()));
    if (end - start < settings.histogramBlock * 2)
    {
        return result;
    }

    // the echo density profile: a Hann window of windowSeconds (normalized to a sum of 1), moved in steps
    const int window = std::max(3, static_cast<int>(settings.windowSeconds * fs) | 1);
    std::vector<double> weights(static_cast<size_t>(window));
    double weightSum = 0.0;
    for (int index = 0; index < window; ++index)
    {
        weights[static_cast<size_t>(index)] = 0.5 - 0.5 * std::cos(2.0 * kPi * (index + 1) / (window + 1));
        weightSum += weights[static_cast<size_t>(index)];
    }
    for (double& weight : weights)
    {
        weight /= weightSum;
    }
    const int step = std::max(1, static_cast<int>(settings.stepSeconds * fs));
    for (int from = start; from + window <= end; from += step)
    {
        double variance = 0.0;
        for (int index = 0; index < window; ++index)
        {
            const double value = response[static_cast<size_t>(from + index)];
            variance += weights[static_cast<size_t>(index)] * value * value;
        }
        const double deviation = std::sqrt(variance);
        double outside = 0.0;
        for (int index = 0; index < window; ++index)
        {
            if (std::abs(response[static_cast<size_t>(from + index)]) > deviation)
            {
                outside += weights[static_cast<size_t>(index)];
            }
        }
        const double time = (from + window / 2 - start) / fs;
        result.densityTimes.push_back(time);
        result.density.push_back(outside / kGaussianOutside);
        if (result.mixingTimeSeconds < 0.0 && result.density.back() >= settings.densityMixed)
        {
            result.mixingTimeSeconds = time;
        }
    }

    // Griesinger's echo count per block
    const int block = std::max(1, static_cast<int>(settings.windowSeconds * fs));
    for (int from = start; from + block <= end; from += block)
    {
        double largest = 0.0;
        for (int sample = from; sample < from + block; ++sample)
        {
            largest = std::max(largest, std::abs(response[static_cast<size_t>(sample)]));
        }
        const double threshold = largest * std::pow(10.0, -kEchoWithinDb / 20.0);
        int count = 0;
        for (int sample = from; sample < from + block; ++sample)
        {
            if (std::abs(response[static_cast<size_t>(sample)]) >= threshold && largest > 0.0)
            {
                ++count;
            }
        }
        result.echoTimes.push_back((from + block / 2 - start) / fs);
        result.echoesPerSecond.push_back(count / (block / fs));
    }

    // h / sqrt(EDC): the decay removed, while the curve is above -40 dB
    std::vector<double> remaining(static_cast<size_t>(end - start) + 1, 0.0);
    for (int sample = end - 1; sample >= start; --sample)
    {
        const double value = response[static_cast<size_t>(sample)];
        remaining[static_cast<size_t>(sample - start)] = remaining[static_cast<size_t>(sample - start) + 1] + value * value;
    }
    const double total = std::max(remaining.front(), kTiny);
    std::vector<double> normalized;
    for (int sample = start; sample < end; ++sample)
    {
        const double energy = remaining[static_cast<size_t>(sample - start)];
        if (10.0 * std::log10(std::max(energy / total, kTiny)) < kNormalizedDownToDb)
        {
            break;
        }
        normalized.push_back(response[static_cast<size_t>(sample)] / std::sqrt(energy));
    }

    // the short-time histogram and the kurtosis per block
    double lateSum = 0.0;
    int lateBlocks = 0;
    const int histogramBlock = settings.histogramBlock;
    for (size_t from = 0; from + static_cast<size_t>(histogramBlock) <= normalized.size(); from += static_cast<size_t>(histogramBlock))
    {
        double mean = 0.0;
        for (int index = 0; index < histogramBlock; ++index)
        {
            mean += normalized[from + static_cast<size_t>(index)];
        }
        mean /= histogramBlock;
        double second = 0.0;
        double fourth = 0.0;
        for (int index = 0; index < histogramBlock; ++index)
        {
            const double centred = normalized[from + static_cast<size_t>(index)] - mean;
            second += centred * centred;
            fourth += centred * centred * centred * centred;
        }
        second /= histogramBlock;
        fourth /= histogramBlock;
        const double excess = fourth / std::max(second * second, kTiny) - 3.0;
        const double time = (static_cast<double>(from) + 0.5 * histogramBlock) / fs;
        result.kurtosisTimes.push_back(time);
        result.kurtosis.push_back(excess);
        std::vector<double> histogram(static_cast<size_t>(settings.histogramBins), 0.0);
        const double deviation = std::sqrt(second);
        for (int index = 0; index < histogramBlock; ++index)
        {
            const double position = (normalized[from + static_cast<size_t>(index)] - mean) / std::max(deviation, kTiny);
            const int bin = static_cast<int>(std::floor((position + kHistogramRange) / (2.0 * kHistogramRange) * settings.histogramBins));
            if (bin >= 0 && bin < settings.histogramBins)
            {
                histogram[static_cast<size_t>(bin)] += 1.0 / histogramBlock;
            }
        }
        result.histograms.push_back(histogram);
        // the late blocks: after the mixing time, or the second half for a response that never mixes
        double late = result.mixingTimeSeconds;
        if (late < 0.0)
        {
            late = 0.5 * static_cast<double>(normalized.size()) / fs;
        }
        if (time >= late)
        {
            lateSum += excess;
            ++lateBlocks;
        }
    }
    if (lateBlocks > 0)
    {
        result.lateKurtosis = lateSum / lateBlocks;
    }

    // the normalized autocorrelation of h / sqrt(EDC) after the mixing time, by FFT
    const size_t mixed = static_cast<size_t>(std::max(0.0, result.mixingTimeSeconds) * fs);
    if (mixed + 2 < normalized.size())
    {
        const int count = static_cast<int>(normalized.size() - mixed);
        const int maximumLag = std::min(count - 1, static_cast<int>(settings.autocorrelationSeconds * fs));
        const int order = getOrderAtLeast(2 * count);
        const int size = 1 << order;
        juce::dsp::FFT fft(order);
        std::vector<juce::dsp::Complex<float>> time(static_cast<size_t>(size)), spectrum(static_cast<size_t>(size));
        for (int index = 0; index < count; ++index)
        {
            time[static_cast<size_t>(index)] = static_cast<float>(normalized[mixed + static_cast<size_t>(index)]);
        }
        fft.perform(time.data(), spectrum.data(), false);
        for (juce::dsp::Complex<float>& value : spectrum)
        {
            value = std::norm(value);
        }
        fft.perform(spectrum.data(), time.data(), true);
        const double zero = std::max(static_cast<double>(time[0].real()), kTiny);
        const int shortest = std::max(1, static_cast<int>(settings.shortestLagSeconds * fs));
        for (int lag = 0; lag <= maximumLag; ++lag)
        {
            const double value = time[static_cast<size_t>(lag)].real() / zero;
            result.autocorrelation.push_back(value);
            if (lag >= shortest && value > result.repetitionStrength)
            {
                result.repetitionStrength = value;
                result.repetitionLagSeconds = lag / fs;
            }
        }
    }

    // the coloration: the late spectrum's fine structure around its third-octave smoothing, 100 Hz ... 10 kHz
    const int colorStart = start + static_cast<int>(mixed);
    const int colorLength = std::min(end - colorStart, static_cast<int>(settings.colorationSeconds * fs));
    if (colorLength > 64)
    {
        const int order = getOrderAtLeast(colorLength);
        const int size = 1 << order;
        juce::dsp::FFT fft(order);
        std::vector<juce::dsp::Complex<float>> time(static_cast<size_t>(size)), spectrum(static_cast<size_t>(size));
        for (int index = 0; index < colorLength; ++index)
        {
            time[static_cast<size_t>(index)] = static_cast<float>(response[static_cast<size_t>(colorStart + index)]);
        }
        fft.perform(time.data(), spectrum.data(), false);
        std::vector<double> power(static_cast<size_t>(size / 2 + 1));
        for (size_t bin = 0; bin < power.size(); ++bin)
        {
            power[bin] = std::norm(std::complex<double>(spectrum[bin].real(), spectrum[bin].imag()));
        }
        const double binHz = fs / size;
        const std::vector<double> smoothed = smoothThirdOctave(power, binHz);
        double sum = 0.0;
        double squares = 0.0;
        int bins = 0;
        for (size_t bin = 1; bin < power.size(); ++bin)
        {
            const double frequency = bin * binHz;
            if (frequency < 100.0 || frequency > 10000.0 || smoothed[bin] <= 0.0 || power[bin] <= 0.0)
            {
                continue;
            }
            const double fine = 10.0 * std::log10(power[bin] / smoothed[bin]);
            sum += fine;
            squares += fine * fine;
            ++bins;
        }
        if (bins > 1)
        {
            const double mean = sum / bins;
            result.colorationDb = std::sqrt(std::max(0.0, squares / bins - mean * mean));
        }
    }
    return result;
}

DecayRelief getDecayRelief(const std::vector<double>& response, int start, int end, double sampleRate, int frameSize, int hop)
{
    DecayRelief relief;
    end = std::min(end, static_cast<int>(response.size()));
    const int order = getOrderAtLeast(frameSize);
    frameSize = 1 << order;
    const int bins = frameSize / 2 + 1;
    juce::dsp::FFT fft(order);
    std::vector<double> window(static_cast<size_t>(frameSize));
    for (int index = 0; index < frameSize; ++index)
    {
        window[static_cast<size_t>(index)] = 0.5 - 0.5 * std::cos(2.0 * kPi * index / frameSize);
    }
    std::vector<std::vector<double>> power;
    std::vector<juce::dsp::Complex<float>> time(static_cast<size_t>(frameSize)), spectrum(static_cast<size_t>(frameSize));
    for (int from = start; from + frameSize <= end; from += hop)
    {
        for (int index = 0; index < frameSize; ++index)
        {
            time[static_cast<size_t>(index)] = static_cast<float>(response[static_cast<size_t>(from + index)] * window[static_cast<size_t>(index)]);
        }
        fft.perform(time.data(), spectrum.data(), false);
        std::vector<double> frame(static_cast<size_t>(bins));
        for (int bin = 0; bin < bins; ++bin)
        {
            frame[static_cast<size_t>(bin)] = std::norm(std::complex<double>(spectrum[static_cast<size_t>(bin)].real(), spectrum[static_cast<size_t>(bin)].imag()));
        }
        power.push_back(frame);
        relief.times.push_back((from + 0.5 * frameSize - start) / sampleRate);
    }
    for (int bin = 0; bin < bins; ++bin)
    {
        relief.frequencies.push_back(bin * sampleRate / frameSize);
    }
    if (power.empty())
    {
        return relief;
    }
    // backward integration per bin, in dB re the first frame
    std::vector<double> sums(static_cast<size_t>(bins), 0.0);
    relief.levelsDb.assign(power.size(), std::vector<float>(static_cast<size_t>(bins), 0.0f));
    std::vector<std::vector<double>> integrated(power.size(), std::vector<double>(static_cast<size_t>(bins), 0.0));
    for (size_t frame = power.size(); frame-- > 0;)
    {
        for (int bin = 0; bin < bins; ++bin)
        {
            sums[static_cast<size_t>(bin)] += power[frame][static_cast<size_t>(bin)];
            integrated[frame][static_cast<size_t>(bin)] = sums[static_cast<size_t>(bin)];
        }
    }
    relief.t60.assign(static_cast<size_t>(bins), 0.0);
    for (int bin = 0; bin < bins; ++bin)
    {
        const double first = std::max(integrated.front()[static_cast<size_t>(bin)], kTiny);
        std::vector<double> curve(power.size());
        for (size_t frame = 0; frame < power.size(); ++frame)
        {
            const double level = 10.0 * std::log10(std::max(integrated[frame][static_cast<size_t>(bin)] / first, kTiny));
            curve[frame] = level;
            relief.levelsDb[frame][static_cast<size_t>(bin)] = static_cast<float>(level);
        }
        relief.t60[static_cast<size_t>(bin)] = getDecayTimes(curve, sampleRate / hop).t30;
    }
    return relief;
}
}
