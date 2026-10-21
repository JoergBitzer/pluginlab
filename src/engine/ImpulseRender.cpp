#include "pluginlab/engine/ImpulseRender.h"

#include <algorithm>
#include <cmath>

#include "PluginRig.h"

namespace pluginlab::engine
{
namespace
{
double toLinear(double dbfs)
{
    return std::pow(10.0, dbfs / 20.0);
}
}

ImpulseRender renderImpulse(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                            const ImpulseRenderOptions& options)
{
    ImpulseRender result;
    FingerprintSettings settings;
    settings.settleSeconds = options.settleSeconds;
    detail::Rig rig(formatManager, description, options.sampleRate, options.blockSize, settings);
    if (! rig.isValid())
    {
        result.error = rig.getError();
        if (result.error.isEmpty())
        {
            result.error = "the plugin supports no usable channel layout";
        }
        return result;
    }
    rig.setTempo(options.tempoBpm);
    rig.prepare();
    rig.apply(detail::makePokeValues(setting, settings));
    rig.process(detail::makeSilence(options.blockSize));
    rig.apply(setting);
    rig.settle();

    result.channels = rig.getChannels();
    result.output.assign(static_cast<size_t>(result.channels), {});
    result.beforeImpulse.assign(static_cast<size_t>(result.channels), 0.0f);

    // the silence before the impulse: what comes out is the plugin's own signal
    const detail::Signal before = rig.process(detail::makeSilence(options.preDelaySamples));
    for (size_t channel = 0; channel < before.size(); ++channel)
    {
        for (const float value : before[channel])
        {
            result.beforeImpulse[channel] = std::max(result.beforeImpulse[channel], std::abs(value));
        }
    }

    const float height = static_cast<float>(toLinear(options.levelDbfs));
    const float silence = static_cast<float>(toLinear(options.silenceDbfs));
    const float limit = static_cast<float>(toLinear(options.safetyLimitDbfs));
    const int holdSamples = static_cast<int>(options.holdSeconds * options.sampleRate);
    const juce::int64 maximumSamples = static_cast<juce::int64>(options.maxSeconds * options.sampleRate);
    int quietSamples = 0;
    juce::int64 rendered = 0;
    while (rendered < maximumSamples)
    {
        // one block: the impulse in its first sample (the first block only), silence otherwise; the impulse on the chosen channels
        detail::Signal block(2, std::vector<float>(static_cast<size_t>(options.blockSize), 0.0f));
        if (rendered == 0)
        {
            for (size_t channel = 0; channel < block.size(); ++channel)
            {
                if (options.impulseChannel < 0 || options.impulseChannel == static_cast<int>(channel))
                {
                    block[channel][0] = height;
                }
            }
        }
        const detail::Signal output = rig.process(block);
        bool stop = false;
        for (size_t sample = 0; sample < static_cast<size_t>(options.blockSize); ++sample)
        {
            float largest = 0.0f;
            for (size_t channel = 0; channel < output.size(); ++channel)
            {
                const float value = output[channel][sample];
                result.output[channel].push_back(value);
                largest = std::max(largest, std::abs(value));
            }
            if (largest > limit || ! std::isfinite(largest))
            {
                result.overflowed = true;
                stop = true;
                break;
            }
            if (largest < silence)
            {
                ++quietSamples;
            }
            else
            {
                quietSamples = 0;
            }
        }
        rendered += options.blockSize;
        if (stop)
        {
            break;
        }
        if (quietSamples >= holdSamples)
        {
            result.decayed = true;
            break;
        }
    }
    if (! result.output.empty())
    {
        result.seconds = static_cast<double>(result.output.front().size()) / options.sampleRate;
    }
    return result;
}
}
