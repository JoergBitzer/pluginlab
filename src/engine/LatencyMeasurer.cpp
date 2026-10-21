#include "pluginlab/engine/LatencyMeasurer.h"

#include <algorithm>
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

// the largest sample in [position - 2, position + 2] of the response (indices counted from the impulse)
float getNear(const std::vector<float>& peaks, int preDelay, int position)
{
    float largest = 0.0f;
    for (int index = position - 2; index <= position + 2; ++index)
    {
        const int at = preDelay + index;
        if (at >= preDelay && at < static_cast<int>(peaks.size()))
        {
            largest = std::max(largest, peaks[static_cast<size_t>(at)]);
        }
    }
    return largest;
}

// the peak far after the reported latency: the dry path at the reported latency, a second echo at the same distance, or a time no latency has
void classifyEcho(LatencyResult& result, const std::vector<float>& peaks, float peak, double sampleRate, const LatencyOptions& options)
{
    const int reported = result.reportedAfterAudio;
    const int distance = result.measuredSamples - reported;
    if (! result.found || distance < static_cast<int>(options.minimumEchoSeconds * sampleRate))
    {
        return;
    }
    const int preDelay = options.preDelaySamples;
    const float dry = getNear(peaks, preDelay, std::max(0, reported) + 2); // the dry path within 4 samples after the latency
    if (toDb(dry / peak) >= -options.dryBelowDb)
    {
        result.echoReason = "the dry path arrives at the reported latency";
    }
    else if (toDb(getNear(peaks, preDelay, result.measuredSamples + distance) / peak) >= -options.secondEchoBelowDb)
    {
        result.echoReason = "a second echo follows at the same distance";
    }
    else if (distance >= static_cast<int>(options.implausibleLatencySeconds * sampleRate))
    {
        result.echoReason = "later than any plausible latency (" + juce::String(options.implausibleLatencySeconds * 1000.0, 0) + " ms)";
    }
    else
    {
        return;
    }
    result.peakIsEcho = true;
    result.echoSamples = result.measuredSamples;
    result.measuredSamples = reported;
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
    classifyEcho(result, peaks, peak, sampleRate, options);
    result.outputBeforeImpulseDbfs = toDb(beforeImpulse);
    if (result.found)
    {
        result.outputBeforePeakDb = toDb(beforePeak / peak);
    }
    return result;
}
}
