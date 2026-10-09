#include "pluginlab/measure/Intermodulation.h"

#include <cmath>
#include <complex>
#include <limits>

#include "pluginlab/signals/Signals.h"

namespace pluginlab::measure
{
namespace
{
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

// The frequency-domain band-pass (AES17 5.2.10): the rms level of all bins within +- bandwidth / 2 of the centre (amplitude^2 / 2 per bin)
double getBandRms(const std::vector<std::complex<double>>& spectrum, int length, double sampleRate, double centreHz, double bandwidthHz)
{
    const double binWidth = sampleRate / length;
    const int first = std::max(1, static_cast<int>(std::ceil((centreHz - 0.5 * bandwidthHz) / binWidth)));
    const int last = std::min(length / 2 - 1, static_cast<int>(std::floor((centreHz + 0.5 * bandwidthHz) / binWidth)));
    double power = 0.0;
    for (int bin = first; bin <= last; ++bin)
    {
        power += 2.0 * std::norm(spectrum[static_cast<size_t>(bin)]) / (static_cast<double>(length) * length);
    }
    return std::sqrt(power);
}

struct TwoToneRun
{
    juce::AudioBuffer<float> output;
    int start = 0;
};

TwoToneRun runTwoTone(const Device& device, double sampleRate, double firstHz, double secondHz, double ratio, double levelDbfs, double settleSeconds,
                      int length, int channels)
{
    signals::TwoToneSettings tones;
    tones.sampleRate = sampleRate;
    tones.firstHz = firstHz;
    tones.secondHz = secondHz;
    tones.amplitudeRatio = ratio;
    tones.levelDbfsPeak = levelDbfs;
    TwoToneRun run;
    run.start = static_cast<int>(std::round(settleSeconds * sampleRate));
    tones.length = run.start + length;
    run.output = device(signals::makeTwoTone(tones, channels), sampleRate);
    return run;
}

std::vector<double> getChannel(const juce::AudioBuffer<float>& buffer, int channel)
{
    const float* data = buffer.getReadPointer(channel);
    return std::vector<double>(data, data + buffer.getNumSamples());
}
}

DifferenceFrequencyResult measureDifferenceFrequency(const Device& device, const DifferenceFrequencySettings& settings)
{
    DifferenceFrequencyResult result;
    result.settings = settings;
    result.windowSamples = getPowerOfTwoAtLeast(settings.measureSeconds * settings.sampleRate);
    const int length = result.windowSamples;
    result.upperHz = getCoherentFrequency(settings.upperToneHz, settings.sampleRate, length);
    result.lowerHz = getCoherentFrequency(settings.upperToneHz - settings.spacingHz, settings.sampleRate, length);
    const TwoToneRun run = runTwoTone(device, settings.sampleRate, result.lowerHz, result.upperHz, 1.0, settings.levelDbfs, settings.settleSeconds, length,
                                      settings.channels);
    result.windowStart = run.start;
    const double nyquist = 0.5 * settings.sampleRate;
    const double difference = result.upperHz - result.lowerHz;
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<std::complex<double>> spectrum = getSpectrum(getChannel(run.output, channel), run.start, length);
        const auto band = [&](double centreHz) { return getBandRms(spectrum, length, settings.sampleRate, centreHz, settings.bandwidthHz); };
        ChannelDifferenceFrequency measured;
        const double fundamental = band(result.lowerHz);
        const double second = band(difference);
        const double lowerThird = band(result.lowerHz - difference);
        double upperThird = 0.0;
        measured.upperThirdOrderDb = std::numeric_limits<double>::quiet_NaN();
        if (result.upperHz + difference < nyquist)
        {
            upperThird = band(result.upperHz + difference);
            measured.upperThirdOrderDb = toDb(upperThird / fundamental);
        }
        measured.lowerFundamentalDbfs = rmsToDbfs(fundamental);
        measured.secondOrderDb = toDb(second / fundamental);
        measured.lowerThirdOrderDb = toDb(lowerThird / fundamental);
        const double products = std::sqrt(second * second + lowerThird * lowerThird + upperThird * upperThird);
        measured.ratioDb = toDb(products / fundamental);
        measured.ratioPercent = 100.0 * products / fundamental;
        result.channels.push_back(measured);
    }
    return result;
}

ModulationResult measureModulation(const Device& device, const ModulationSettings& settings)
{
    ModulationResult result;
    result.settings = settings;
    result.windowSamples = getPowerOfTwoAtLeast(settings.measureSeconds * settings.sampleRate);
    const int length = result.windowSamples;
    result.lowHz = getCoherentFrequency(settings.lowToneHz, settings.sampleRate, length);
    result.highHz = getCoherentFrequency(settings.highToneHz, settings.sampleRate, length);
    const TwoToneRun run = runTwoTone(device, settings.sampleRate, result.lowHz, result.highHz, settings.amplitudeRatio, settings.levelDbfs,
                                      settings.settleSeconds, length, settings.channels);
    result.windowStart = run.start;
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<std::complex<double>> spectrum = getSpectrum(getChannel(run.output, channel), run.start, length);
        const auto band = [&](double centreHz) { return getBandRms(spectrum, length, settings.sampleRate, centreHz, settings.bandwidthHz); };
        ChannelModulation measured;
        const double fundamental = band(result.highHz);
        measured.upperFundamentalDbfs = rmsToDbfs(fundamental);
        double first = 0.0;
        double all = 0.0;
        for (int order = 1; order <= settings.sidebandOrders; ++order)
        {
            const double lower = band(result.highHz - order * result.lowHz);
            const double upper = band(result.highHz + order * result.lowHz);
            measured.lowerSidebandDb.push_back(toDb(lower / fundamental));
            measured.upperSidebandDb.push_back(toDb(upper / fundamental));
            all += lower * lower + upper * upper;
            if (order == 1)
            {
                first = lower * lower + upper * upper;
            }
        }
        measured.ratioDb = toDb(std::sqrt(first) / fundamental);
        measured.ratioPercent = 100.0 * std::sqrt(first) / fundamental;
        measured.allSidebandsDb = toDb(std::sqrt(all) / fundamental);
        result.channels.push_back(measured);
    }
    return result;
}
}
