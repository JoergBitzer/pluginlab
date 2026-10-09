#include "pluginlab/measure/Noise.h"

#include <cmath>
#include <complex>

#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Designs.h"
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

WeightedLevels getLevels(const std::vector<std::complex<double>>& spectrum, int length, double sampleRate, const std::vector<double>& extraGain = {})
{
    WeightedLevels levels;
    levels.unweightedDbfs = rmsToDbfs(getWeightedRms(spectrum, length, sampleRate, Weighting::Unweighted, extraGain));
    levels.bandDbfs = rmsToDbfs(getWeightedRms(spectrum, length, sampleRate, Weighting::Band, extraGain));
    levels.ccirRmsDbfs = rmsToDbfs(getWeightedRms(spectrum, length, sampleRate, Weighting::CcirRms, extraGain));
    levels.aWeightedDbfs = rmsToDbfs(getWeightedRms(spectrum, length, sampleRate, Weighting::A, extraGain));
    return levels;
}

// The spectrum of each channel's output after the standard low-pass filter, window [start, start + length)
std::vector<std::vector<std::complex<double>>> getOutputSpectra(const juce::AudioBuffer<float>& output, double sampleRate, int start, int length)
{
    const StandardLowPass lowPass(sampleRate);
    std::vector<std::vector<std::complex<double>>> spectra;
    for (int channel = 0; channel < output.getNumChannels(); ++channel)
    {
        spectra.push_back(getSpectrum(lowPass.process(output.getReadPointer(channel), output.getNumSamples()), start, length));
    }
    return spectra;
}
}

double WeightedLevels::get(Weighting weighting) const
{
    switch (weighting)
    {
        case Weighting::Unweighted:
            return unweightedDbfs;
        case Weighting::Band:
            return bandDbfs;
        case Weighting::CcirRms:
            return ccirRmsDbfs;
        case Weighting::A:
            return aWeightedDbfs;
    }
    return unweightedDbfs;
}

double getWeightedRms(const std::vector<std::complex<double>>& spectrum, int length, double sampleRate, Weighting weighting, const std::vector<double>& extraGain)
{
    const int half = length / 2;
    double power = 0.0;
    for (int bin = 0; bin <= half; ++bin)
    {
        // a real signal: every bin but DC and Nyquist stands for two (the negative frequency)
        double share = 2.0;
        if (bin == 0 || bin == half)
        {
            share = 1.0;
        }
        double gain = getWeightingGain(weighting, bin * sampleRate / length);
        if (! extraGain.empty())
        {
            gain *= extraGain[static_cast<size_t>(bin)];
        }
        power += share * gain * gain * std::norm(spectrum[static_cast<size_t>(bin)]);
    }
    return std::sqrt(power) / length;
}

IdleNoiseResult measureIdleNoise(const Device& device, const NoiseSettings& settings)
{
    IdleNoiseResult result;
    result.settings = settings;
    result.windowSamples = getPowerOfTwoAtLeast(settings.measureSeconds * settings.sampleRate);
    const int start = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    juce::AudioBuffer<float> silence(settings.channels, start + result.windowSamples);
    silence.clear();
    const juce::AudioBuffer<float> output = device(silence, settings.sampleRate);
    for (const std::vector<std::complex<double>>& spectrum : getOutputSpectra(output, settings.sampleRate, start, result.windowSamples))
    {
        result.channels.push_back(getLevels(spectrum, result.windowSamples, settings.sampleRate));
    }
    return result;
}

DynamicRangeResult measureDynamicRange(const Device& device, const NoiseSettings& settings)
{
    DynamicRangeResult result;
    result.settings = settings;
    result.windowSamples = getPowerOfTwoAtLeast(settings.measureSeconds * settings.sampleRate);
    const int length = result.windowSamples;
    result.frequencyHz = getCoherentFrequency(settings.testFrequencyHz, settings.sampleRate, length);
    const int start = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    signals::SineSettings sine;
    sine.sampleRate = settings.sampleRate;
    sine.frequencyHz = result.frequencyHz;
    sine.levelDbfsPeak = settings.testLevelDbfs;
    sine.length = start + length;
    const juce::AudioBuffer<float> output = device(signals::makeSine(sine, settings.channels), settings.sampleRate);

    // the standard notch (5.2.8) as its magnitude on the bins; on the coherent test frequency it is zero
    const reference::BiquadCoefficients notch = reference::designRbj(reference::FilterType::Notch, settings.sampleRate, result.frequencyHz, 0.0, settings.notchQ);
    std::vector<double> notchGain;
    for (int bin = 0; bin <= length / 2; ++bin)
    {
        notchGain.push_back(std::abs(reference::getBiquadResponse(notch, bin * settings.sampleRate / length, settings.sampleRate)));
    }
    for (const std::vector<std::complex<double>>& spectrum : getOutputSpectra(output, settings.sampleRate, start, length))
    {
        DynamicRangeResult::Channel channel;
        channel.residual = getLevels(spectrum, length, settings.sampleRate, notchGain);
        channel.dynamicRange.unweightedDbfs = settings.maximumOutputDbfs - channel.residual.unweightedDbfs;
        channel.dynamicRange.bandDbfs = settings.maximumOutputDbfs - channel.residual.bandDbfs;
        channel.dynamicRange.ccirRmsDbfs = settings.maximumOutputDbfs - channel.residual.ccirRmsDbfs;
        channel.dynamicRange.aWeightedDbfs = settings.maximumOutputDbfs - channel.residual.aWeightedDbfs;
        result.channels.push_back(channel);
    }
    return result;
}

MainsResult measureMainsProducts(const Device& device, const MainsSettings& settings)
{
    MainsResult result;
    result.settings = settings;
    const int samplesPerSecond = static_cast<int>(std::round(settings.sampleRate));
    jassert(std::abs(settings.sampleRate - samplesPerSecond) < 1.0e-9);
    result.windowSamples = settings.measureSeconds * samplesPerSecond;
    const int start = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    juce::AudioBuffer<float> silence(settings.channels, start + result.windowSamples);
    silence.clear();
    const juce::AudioBuffer<float> output = device(silence, settings.sampleRate);
    double bandwidth = settings.bandwidthHz;
    if (bandwidth <= 0.0)
    {
        bandwidth = 0.5 * settings.mainsHz;
    }
    const double binWidth = 1.0 / settings.measureSeconds;
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const float* samples = output.getReadPointer(channel);
        const std::vector<double> data(samples, samples + output.getNumSamples());
        MainsResult::Channel measured;
        double total = 0.0;
        for (int multiple = 1; multiple <= settings.harmonics; ++multiple)
        {
            const double centre = multiple * settings.mainsHz;
            // the frequency-domain band-pass (5.2.10): the bins within +- bandwidth / 2 (open at the edges: at most half the mains frequency wide)
            const int first = static_cast<int>(std::floor((centre - 0.5 * bandwidth) / binWidth)) + 1;
            const int last = static_cast<int>(std::ceil((centre + 0.5 * bandwidth) / binWidth)) - 1;
            double power = 0.0;
            for (int bin = std::max(first, 1); bin <= last; ++bin)
            {
                power += 0.5 * std::norm(getToneAmplitude(data, start, result.windowSamples, bin * binWidth, settings.sampleRate));
            }
            measured.lineDbfs.push_back(rmsToDbfs(std::sqrt(power)));
            total += power;
        }
        measured.totalDbfs = rmsToDbfs(std::sqrt(total));
        result.channels.push_back(measured);
    }
    return result;
}
}
