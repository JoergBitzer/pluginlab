#include "pluginlab/engine/ChannelAdapter.h"

#include <vector>

namespace pluginlab::engine
{
int ChannelAdapter::chooseLayout(juce::AudioPluginInstance& instance, int outerChannels)
{
    const std::vector<juce::AudioChannelSet> candidates = {
        juce::AudioChannelSet::canonicalChannelSet(outerChannels), juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono()};
    for (const juce::AudioChannelSet& candidate : candidates)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(candidate);
        layout.outputBuses.add(candidate);
        if (instance.checkBusesLayoutSupported(layout) && instance.setBusesLayout(layout))
        {
            return candidate.size();
        }
    }
    return 0;
}

void ChannelAdapter::prepare(int outerChannels, int pluginChannels, int maxBlockSize)
{
    m_outerChannels = outerChannels;
    m_pluginChannels = pluginChannels;
    m_storage.setSize(pluginChannels, maxBlockSize);
}

bool ChannelAdapter::isTransparent() const
{
    return m_outerChannels == m_pluginChannels;
}

juce::AudioBuffer<float>& ChannelAdapter::enter(const juce::AudioBuffer<float>& outer)
{
    const int samples = outer.getNumSamples();
    m_block.setDataToReferTo(m_storage.getArrayOfWritePointers(), m_pluginChannels, samples);
    for (int pluginChannel = 0; pluginChannel < m_pluginChannels; ++pluginChannel)
    {
        m_block.clear(pluginChannel, 0, samples);
        for (int channel = 0; channel < m_outerChannels; ++channel)
        {
            const bool contributes = m_pluginChannels == 1 || m_outerChannels == 1 || channel == pluginChannel;
            if (contributes)
            {
                m_block.addFrom(pluginChannel, 0, outer, channel, 0, samples);
            }
        }
        if (m_pluginChannels == 1)
        {
            m_block.applyGain(pluginChannel, 0, samples, 1.0f / static_cast<float>(m_outerChannels));
        }
    }
    return m_block;
}

void ChannelAdapter::leave(juce::AudioBuffer<float>& outer)
{
    const int samples = outer.getNumSamples();
    for (int channel = 0; channel < m_outerChannels; ++channel)
    {
        const int pluginChannel = juce::jmin(channel, m_pluginChannels - 1);
        outer.copyFrom(channel, 0, m_block, pluginChannel, 0, samples);
    }
}
}
