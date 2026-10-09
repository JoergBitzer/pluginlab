#include "pluginlab/engine/PluginDevice.h"

#include "PluginRig.h"

namespace pluginlab::engine
{
namespace
{
detail::Signal toSignal(const juce::AudioBuffer<float>& buffer)
{
    detail::Signal signal;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* data = buffer.getReadPointer(channel);
        signal.emplace_back(data, data + buffer.getNumSamples());
    }
    return signal;
}

juce::AudioBuffer<float> toBuffer(const detail::Signal& signal)
{
    const int length = static_cast<int>(detail::getLength(signal));
    juce::AudioBuffer<float> buffer(static_cast<int>(signal.size()), length);
    for (size_t channel = 0; channel < signal.size(); ++channel)
    {
        buffer.copyFrom(static_cast<int>(channel), 0, signal[channel].data(), length);
    }
    return buffer;
}
}

measure::Device makePluginDevice(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, std::vector<float> setting,
                                 PluginDeviceOptions options)
{
    return [&formatManager, description, setting, options](const juce::AudioBuffer<float>& input, double sampleRate)
    {
        FingerprintSettings settings;
        settings.settleSeconds = options.settleSeconds;
        const detail::Bench bench(formatManager, description, settings);
        return toBuffer(bench.render(sampleRate, options.blockSize, setting, toSignal(input)));
    };
}

int getPluginChannels(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate)
{
    const FingerprintSettings settings;
    const detail::Rig rig(formatManager, description, sampleRate, detail::kReferenceBlock, settings);
    if (! rig.isValid())
    {
        return 0;
    }
    return rig.getChannels();
}

std::vector<float> getSetting(juce::AudioPluginInstance& instance)
{
    std::vector<float> setting;
    for (juce::AudioProcessorParameter* parameter : instance.getParameters())
    {
        setting.push_back(parameter->getValue());
    }
    return setting;
}

std::vector<float> getDefaultSetting(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate)
{
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> instance = formatManager.createPluginInstance(description, sampleRate, detail::kReferenceBlock, error);
    if (instance == nullptr)
    {
        return {};
    }
    return getSetting(*instance);
}
}
