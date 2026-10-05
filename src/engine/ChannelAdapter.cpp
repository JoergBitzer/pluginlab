#include "pluginlab/engine/ChannelAdapter.h"

#include <vector>

namespace pluginlab::engine
{
// The main buses get the candidate layout. Other buses (a side-chain input, extra outputs) are switched off if the plugin allows it,
// else they keep their default layout (then the plugin needs more channels in its buffer: getProcessingChannels()).
int ChannelAdapter::chooseLayout(juce::AudioPluginInstance& instance, int outerChannels)
{
    const std::vector<juce::AudioChannelSet> candidates = {
        juce::AudioChannelSet::canonicalChannelSet(outerChannels), juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono()};
    juce::AudioProcessor::BusesLayout base = instance.getBusesLayout();
    if (base.inputBuses.isEmpty() || base.outputBuses.isEmpty())
    {
        base.inputBuses.clear();
        base.outputBuses.clear();
        base.inputBuses.add(juce::AudioChannelSet::disabled());
        base.outputBuses.add(juce::AudioChannelSet::disabled());
    }
    for (const juce::AudioChannelSet& candidate : candidates)
    {
        for (const bool keepOtherBuses : {false, true})
        {
            juce::AudioProcessor::BusesLayout layout = base;
            for (int bus = 0; bus < layout.inputBuses.size(); ++bus)
            {
                if (bus == 0)
                {
                    layout.inputBuses.getReference(bus) = candidate;
                }
                else if (! keepOtherBuses)
                {
                    layout.inputBuses.getReference(bus) = juce::AudioChannelSet::disabled();
                }
            }
            for (int bus = 0; bus < layout.outputBuses.size(); ++bus)
            {
                if (bus == 0)
                {
                    layout.outputBuses.getReference(bus) = candidate;
                }
                else if (! keepOtherBuses)
                {
                    layout.outputBuses.getReference(bus) = juce::AudioChannelSet::disabled();
                }
            }
            if (instance.checkBusesLayoutSupported(layout) && instance.setBusesLayout(layout))
            {
                return candidate.size();
            }
        }
    }
    return 0;
}

int ChannelAdapter::getProcessingChannels(const juce::AudioPluginInstance& instance)
{
    return juce::jmax(instance.getTotalNumInputChannels(), instance.getTotalNumOutputChannels());
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
