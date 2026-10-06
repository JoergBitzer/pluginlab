#include "pluginlab/reference/LinearProcessor.h"

namespace pluginlab::reference
{
namespace
{
template <typename Sample>
void processBuffer(LinearProcessor& processor, juce::AudioBuffer<Sample>& buffer)
{
    if (buffer.getNumChannels() != processor.getNumChannels())
    {
        processor.setNumChannels(buffer.getNumChannels());
    }
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        Sample* data = buffer.getWritePointer(channel);
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            data[index] = static_cast<Sample>(processor.processSample(channel, static_cast<double>(data[index])));
        }
    }
}
}

LinearProcessor::LinearProcessor(double sampleRate)
    : m_sampleRate(sampleRate)
{
}

double LinearProcessor::getSampleRate() const
{
    return m_sampleRate;
}

void LinearProcessor::setNumChannels(int channels)
{
    m_channels = channels;
    prepareChannels(channels);
}

int LinearProcessor::getNumChannels() const
{
    return m_channels;
}

void LinearProcessor::process(juce::AudioBuffer<float>& buffer)
{
    processBuffer(*this, buffer);
}

void LinearProcessor::process(juce::AudioBuffer<double>& buffer)
{
    processBuffer(*this, buffer);
}
}
