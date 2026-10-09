#include "pluginlab/measure/Crosstalk.h"

#include <cmath>

#include "pluginlab/signals/Signals.h"

namespace pluginlab::measure
{
namespace
{
double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}
}

std::vector<double> getCrosstalkFrequencies()
{
    std::vector<double> frequencies;
    for (double frequency = 20.0; frequency < kUpperBandEdgeHz; frequency *= 2.0)
    {
        frequencies.push_back(frequency);
    }
    frequencies.push_back(kUpperBandEdgeHz);
    return frequencies;
}

CrosstalkResult measureCrosstalk(const Device& device, const CrosstalkSettings& settings)
{
    CrosstalkResult result;
    result.settings = settings;
    result.frequencyHz = settings.frequencies;
    if (result.frequencyHz.empty())
    {
        result.frequencyHz = getCrosstalkFrequencies();
    }
    const size_t channels = static_cast<size_t>(settings.channels);
    const size_t count = result.frequencyHz.size();
    result.selectiveDb.assign(channels, std::vector<std::vector<double>>(channels, std::vector<double>(count, 0.0)));
    result.broadbandDb = result.selectiveDb;
    const StandardLowPass lowPass(settings.sampleRate);
    const int start = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    for (size_t index = 0; index < count; ++index)
    {
        const double frequency = result.frequencyHz[index];
        const int length = getWholePeriodLength(frequency, settings.sampleRate, settings.measureSeconds);
        signals::SineSettings sine;
        sine.sampleRate = settings.sampleRate;
        sine.frequencyHz = frequency;
        sine.levelDbfsPeak = settings.levelDbfs;
        sine.length = start + length;
        const juce::AudioBuffer<float> tone = signals::makeSine(sine, 1);
        for (size_t driven = 0; driven < channels; ++driven)
        {
            juce::AudioBuffer<float> input(settings.channels, sine.length);
            input.clear();
            input.copyFrom(static_cast<int>(driven), 0, tone, 0, 0, sine.length);
            const juce::AudioBuffer<float> output = device(input, settings.sampleRate);
            std::vector<double> selective(channels);
            std::vector<double> broadband(channels);
            for (size_t channel = 0; channel < channels; ++channel)
            {
                const std::vector<double> out = lowPass.process(output.getReadPointer(static_cast<int>(channel)), output.getNumSamples());
                selective[channel] = std::abs(getToneAmplitude(out, start, length, frequency, settings.sampleRate));
                broadband[channel] = getRms(out, start, length);
            }
            for (size_t receiving = 0; receiving < channels; ++receiving)
            {
                result.selectiveDb[driven][receiving][index] = toDb(selective[receiving] / selective[driven]);
                result.broadbandDb[driven][receiving][index] = toDb(broadband[receiving] / broadband[driven]);
                if (receiving != driven)
                {
                    result.worstSelectiveDb = std::max(result.worstSelectiveDb, result.selectiveDb[driven][receiving][index]);
                }
            }
        }
    }
    return result;
}
}
