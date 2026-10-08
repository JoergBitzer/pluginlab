#include "pluginlab/measure/Gain.h"

#include <cmath>

#include "pluginlab/signals/Signals.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kDegreesPerRadian = 180.0 / 3.14159265358979323846;

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(ratio, 1.0e-30));
}
}

GainResult measureGain(const Device& device, const GainSettings& settings)
{
    GainResult result;
    result.settings = settings;
    result.measureStart = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    result.measureLength = getWholePeriodLength(settings.frequencyHz, settings.sampleRate, settings.measureSeconds);

    // the stimulus: a sine of the level (dBFS rms = dBFS peak for a sine), the same on all channels, starting with phase 0
    signals::SineSettings sine;
    sine.sampleRate = settings.sampleRate;
    sine.frequencyHz = settings.frequencyHz;
    sine.levelDbfsPeak = settings.levelDbfs;
    sine.length = result.measureStart + result.measureLength;
    const juce::AudioBuffer<float> input = signals::makeSine(sine, settings.channels);
    const juce::AudioBuffer<float> output = device(input, settings.sampleRate);

    // input and output through the same standard low-pass filter; levels over the same whole-period window
    const StandardLowPass lowPass(settings.sampleRate);
    double smallest = 0.0;
    double largest = 0.0;
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        const std::vector<double> in = lowPass.process(input.getReadPointer(channel), input.getNumSamples());
        const std::vector<double> out = lowPass.process(output.getReadPointer(channel), output.getNumSamples());
        ChannelGain gain;
        // the level of the test signal as generated; the gain compares input and output through the same filter (its ripple cancels)
        std::vector<double> generated(input.getReadPointer(channel), input.getReadPointer(channel) + input.getNumSamples());
        gain.inputLevelDbfs = rmsToDbfs(getRms(generated, result.measureStart, result.measureLength));
        const double inputRms = getRms(in, result.measureStart, result.measureLength);
        const double outputRms = getRms(out, result.measureStart, result.measureLength);
        gain.outputLevelDbfs = rmsToDbfs(outputRms);
        gain.gainDb = toDb(outputRms / inputRms);
        const std::complex<double> inputTone = getToneAmplitude(in, result.measureStart, result.measureLength, settings.frequencyHz, settings.sampleRate);
        const std::complex<double> outputTone = getToneAmplitude(out, result.measureStart, result.measureLength, settings.frequencyHz, settings.sampleRate);
        gain.selectiveGainDb = toDb(std::abs(outputTone) / std::abs(inputTone));
        gain.phaseDegrees = std::arg(outputTone / inputTone) * kDegreesPerRadian;
        if (channel == 0)
        {
            smallest = gain.gainDb;
            largest = gain.gainDb;
        }
        smallest = std::min(smallest, gain.gainDb);
        largest = std::max(largest, gain.gainDb);
        result.channels.push_back(gain);
    }
    result.matchingDb = largest - smallest;
    return result;
}
}
