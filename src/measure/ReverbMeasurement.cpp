#include "pluginlab/measure/Reverb.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::measure
{
namespace
{
constexpr double kDiffuse = 0.9;
constexpr double kShortestDecaySeconds = 0.1;
constexpr double kEarlySeconds = 0.08;
}

ReverbMeasurement measureReverbResponse(const Device& device, const ReverbMeasurementSettings& settings)
{
    ReverbMeasurement result;
    const double fs = settings.sampleRate;
    const int length = static_cast<int>(settings.renderSeconds * fs);
    const double height = std::pow(10.0, settings.levelDbfs / 20.0);
    juce::AudioBuffer<float> input(settings.channels, length);
    input.clear();
    for (int channel = 0; channel < settings.channels; ++channel)
    {
        input.setSample(channel, 0, static_cast<float>(height));
    }
    const juce::AudioBuffer<float> output = device(input, fs);
    if (output.getNumChannels() == 0)
    {
        return result;
    }
    std::vector<std::vector<double>> responses(static_cast<size_t>(output.getNumChannels()));
    for (int channel = 0; channel < output.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < output.getNumSamples(); ++sample)
        {
            responses[static_cast<size_t>(channel)].push_back(output.getSample(channel, sample) / height);
        }
    }
    const std::vector<double>& first = responses.front();
    ReverbSettings reverb;
    reverb.sampleRate = fs;
    reverb.latencySamples = settings.latencySamples;
    result.decay = analyseReverb(first, reverb);
    const int onset = static_cast<int>(result.decay.directSample + result.decay.preDelaySeconds * fs);
    const int end = result.decay.truncation.sample;
    TextureSettings texture;
    texture.sampleRate = fs;
    result.texture = analyseTexture(first, onset, end, texture);
    result.hasReverb = result.texture.mixingTimeSeconds >= 0.0 && result.texture.mixingTimeSeconds <= settings.diffuseWithinSeconds
                       && result.decay.broadband.t30 > kShortestDecaySeconds;
    if (! result.hasReverb || responses.size() < 2)
    {
        return result;
    }
    result.stereo = true;
    const std::vector<double>& second = responses[1];
    const int direct = static_cast<int>(result.decay.directSample);
    result.iaccEarly = getIacc(first, second, direct, 0.0, kEarlySeconds, fs);
    result.iaccLate = getIacc(first, second, direct, kEarlySeconds, std::max(kEarlySeconds, (end - direct) / fs), fs);
    const Coherence coherence = getCoherence(first, second, direct + static_cast<int>(kEarlySeconds * fs), end, fs);
    double sum = 0.0;
    int bins = 0;
    for (size_t bin = 0; bin < coherence.frequencies.size(); ++bin)
    {
        if (coherence.frequencies[bin] >= 200.0 && coherence.frequencies[bin] <= 8000.0)
        {
            sum += coherence.coherence[bin];
            ++bins;
        }
    }
    result.meanCoherence = sum / std::max(1, bins);
    result.lateral = getLateralDecay(first, second, direct, fs);
    return result;
}
}
