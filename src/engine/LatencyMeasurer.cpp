#include "pluginlab/engine/LatencyMeasurer.h"

#include <cmath>
#include <vector>

namespace pluginlab::engine
{
namespace
{
constexpr float kImpulseAmplitude = 1.0f;
constexpr double kFloorDb = -200.0;
constexpr double kTiny = 1.0e-20;

double toDb(double value)
{
    if (value < kTiny)
    {
        return kFloorDb;
    }
    return std::max(kFloorDb, 20.0 * std::log10(value));
}
}

LatencyResult measureLatency(juce::AudioPluginInstance& instance, double sampleRate, int blockSize, int channels, const LatencyOptions& options)
{
    LatencyResult result;
    instance.setPlayConfigDetails(channels, channels, sampleRate, blockSize);
    instance.prepareToPlay(sampleRate, blockSize);
    result.reportedSamples = instance.getLatencySamples();

    const int totalSamples = options.preDelaySamples + static_cast<int>(sampleRate * options.observeSeconds);
    std::vector<float> peaks; // per sample the largest magnitude over the channels
    peaks.reserve(static_cast<size_t>(totalSamples));
    juce::AudioBuffer<float> block(channels, blockSize);
    juce::MidiBuffer midi;
    for (int start = 0; start < totalSamples; start += blockSize)
    {
        block.clear();
        const int impulseInBlock = options.preDelaySamples - start;
        if (impulseInBlock >= 0 && impulseInBlock < blockSize)
        {
            for (int channel = 0; channel < channels; ++channel)
            {
                block.setSample(channel, impulseInBlock, kImpulseAmplitude);
            }
        }
        instance.processBlock(block, midi);
        for (int sample = 0; sample < blockSize; ++sample)
        {
            float magnitude = 0.0f;
            for (int channel = 0; channel < channels; ++channel)
            {
                magnitude = std::max(magnitude, std::abs(block.getSample(channel, sample)));
            }
            peaks.push_back(magnitude);
        }
    }
    result.reportedAfterAudio = instance.getLatencySamples();

    size_t peakPosition = static_cast<size_t>(options.preDelaySamples);
    float peak = 0.0f;
    for (size_t index = static_cast<size_t>(options.preDelaySamples); index < peaks.size(); ++index)
    {
        if (peaks[index] > peak)
        {
            peak = peaks[index];
            peakPosition = index;
        }
    }
    float beforeImpulse = 0.0f;
    for (size_t index = 0; index < static_cast<size_t>(options.preDelaySamples) && index < peaks.size(); ++index)
    {
        beforeImpulse = std::max(beforeImpulse, peaks[index]);
    }
    float beforePeak = 0.0f;
    for (size_t index = static_cast<size_t>(options.preDelaySamples); index < peakPosition; ++index)
    {
        beforePeak = std::max(beforePeak, peaks[index]);
    }
    result.found = peak > options.minimumPeak;
    result.measuredSamples = static_cast<int>(peakPosition) - options.preDelaySamples;
    result.outputBeforeImpulseDbfs = toDb(beforeImpulse);
    if (result.found)
    {
        result.outputBeforePeakDb = toDb(beforePeak / peak);
    }
    return result;
}
}
