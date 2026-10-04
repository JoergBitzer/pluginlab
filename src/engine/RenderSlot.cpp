#include "pluginlab/engine/RenderSlot.h"

namespace pluginlab::engine
{
namespace
{
constexpr double kWarmUpSeconds = 0.25;
}

RenderSlot::RenderSlot(std::unique_ptr<hosting::HostedPlugin> plugin, const juce::String& name)
    : m_plugin(std::move(plugin)), m_name(name)
{
}

const juce::String& RenderSlot::getName() const
{
    return m_name;
}

hosting::HostedPlugin* RenderSlot::getPlugin()
{
    return m_plugin.get();
}

bool RenderSlot::prepare(double sampleRate, int maxBlockSize, int engineChannels, juce::String& error)
{
    m_sampleRate = sampleRate;
    m_maxBlockSize = maxBlockSize;
    m_engineChannels = engineChannels;
    m_latency = LatencyResult();
    m_latency.found = true; // the dry slot has no latency
    m_pluginChannels = engineChannels;

    if (m_plugin != nullptr)
    {
        juce::AudioPluginInstance& instance = m_plugin->getInstance();
        m_pluginChannels = ChannelAdapter::chooseLayout(instance, engineChannels);
        if (m_pluginChannels == 0)
        {
            error = m_name + " runs neither with mono nor with stereo audio";
            return false;
        }
        m_latency = measureLatency(instance, sampleRate, maxBlockSize, m_pluginChannels);
    }
    m_adapter.prepare(engineChannels, m_pluginChannels, maxBlockSize);
    restart();
    return true;
}

void RenderSlot::restart()
{
    if (m_plugin != nullptr)
    {
        juce::AudioPluginInstance& instance = m_plugin->getInstance();
        instance.releaseResources();
        instance.setPlayConfigDetails(m_pluginChannels, m_pluginChannels, m_sampleRate, m_maxBlockSize);
        instance.prepareToPlay(m_sampleRate, m_maxBlockSize);
        warmUp();
    }
    setCompensation(m_compensation); // clears the delay line
}

// Silence through the plugin: filters and smoothers of the plugin settle before the first real block
void RenderSlot::warmUp()
{
    juce::AudioBuffer<float> silence(m_engineChannels, m_maxBlockSize);
    const int blocks = static_cast<int>(m_sampleRate * kWarmUpSeconds / m_maxBlockSize) + 1;
    for (int block = 0; block < blocks; ++block)
    {
        silence.clear();
        process(silence, silence);
    }
}

LatencyResult RenderSlot::getLatency() const
{
    return m_latency;
}

void RenderSlot::setCompensation(int samples)
{
    m_compensation = samples;
    m_delayLines.assign(static_cast<size_t>(m_engineChannels), std::vector<float>(static_cast<size_t>(samples), 0.0f));
    m_delayPosition = 0;
}

int RenderSlot::getCompensation() const
{
    return m_compensation;
}

void RenderSlot::process(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output)
{
    if (&input != &output)
    {
        for (int channel = 0; channel < m_engineChannels; ++channel)
        {
            output.copyFrom(channel, 0, input, channel, 0, input.getNumSamples());
        }
    }
    if (m_plugin != nullptr)
    {
        juce::AudioPluginInstance& instance = m_plugin->getInstance();
        m_midi.clear();
        if (m_adapter.isTransparent())
        {
            instance.processBlock(output, m_midi);
        }
        else
        {
            juce::AudioBuffer<float>& pluginBlock = m_adapter.enter(output);
            instance.processBlock(pluginBlock, m_midi);
            m_adapter.leave(output);
        }
    }
    delayOutput(output);
}

void RenderSlot::delayOutput(juce::AudioBuffer<float>& output)
{
    if (m_compensation <= 0)
    {
        return;
    }
    const int samples = output.getNumSamples();
    for (int channel = 0; channel < m_engineChannels; ++channel)
    {
        std::vector<float>& line = m_delayLines[static_cast<size_t>(channel)];
        float* data = output.getWritePointer(channel);
        int position = m_delayPosition;
        for (int sample = 0; sample < samples; ++sample)
        {
            const float delayed = line[static_cast<size_t>(position)];
            line[static_cast<size_t>(position)] = data[sample];
            data[sample] = delayed;
            position = (position + 1) % m_compensation;
        }
    }
    m_delayPosition = (m_delayPosition + samples) % m_compensation;
}
}
