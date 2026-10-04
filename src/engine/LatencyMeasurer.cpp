#include "pluginlab/engine/LatencyMeasurer.h"

namespace pluginlab::engine
{
namespace
{
constexpr float kImpulseAmplitude = 1.0f;
constexpr double kSecondsToObserve = 1.0;
constexpr float kMinimumPeak = 1.0e-4f; // below this nothing came out
}

LatencyResult measureLatency(juce::AudioPluginInstance& instance, double sampleRate, int blockSize, int channels)
{
    LatencyResult result;
    instance.setPlayConfigDetails(channels, channels, sampleRate, blockSize);
    instance.prepareToPlay(sampleRate, blockSize);
    result.reportedSamples = instance.getLatencySamples();

    const int totalSamples = static_cast<int>(sampleRate * kSecondsToObserve);
    juce::AudioBuffer<float> block(channels, blockSize);
    juce::MidiBuffer midi;
    float peak = 0.0f;
    int peakPosition = 0;
    for (int start = 0; start < totalSamples; start += blockSize)
    {
        block.clear();
        if (start == 0)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                block.setSample(channel, 0, kImpulseAmplitude);
            }
        }
        instance.processBlock(block, midi);
        for (int channel = 0; channel < channels; ++channel)
        {
            const float* data = block.getReadPointer(channel);
            for (int sample = 0; sample < blockSize; ++sample)
            {
                const float magnitude = std::abs(data[sample]);
                if (magnitude > peak)
                {
                    peak = magnitude;
                    peakPosition = start + sample;
                }
            }
        }
    }
    result.found = peak > kMinimumPeak;
    result.measuredSamples = peakPosition;
    return result;
}
}
