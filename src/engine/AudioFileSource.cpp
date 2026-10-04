#include "pluginlab/engine/AudioFileSource.h"

#include <cmath>

namespace pluginlab::engine
{
namespace
{
constexpr int kMaximumChannels = 2;
}

bool AudioFileSource::load(const juce::File& file, double targetSampleRate, juce::String& error)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
    {
        error = "Cannot read the audio file " + file.getFullPathName();
        return false;
    }
    if (reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
    {
        error = "The audio file is empty: " + file.getFullPathName();
        return false;
    }

    const int channels = juce::jmin(static_cast<int>(reader->numChannels), kMaximumChannels);
    const int length = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> original(channels, length);
    if (! reader->read(&original, 0, length, 0, true, channels > 1))
    {
        error = "Cannot read the samples of " + file.getFullPathName();
        return false;
    }

    const double ratio = reader->sampleRate / targetSampleRate;
    if (std::abs(ratio - 1.0) < 1.0e-9)
    {
        m_data = original;
    }
    else
    {
        const int convertedLength = static_cast<int>(std::ceil(static_cast<double>(length) / ratio));
        m_data.setSize(channels, convertedLength);
        for (int channel = 0; channel < channels; ++channel)
        {
            juce::LagrangeInterpolator interpolator;
            interpolator.process(ratio, original.getReadPointer(channel), m_data.getWritePointer(channel), convertedLength);
        }
    }

    m_file = file;
    m_name = file.getFileName();
    m_sampleRate = targetSampleRate;
    m_regionStart = 0;
    m_regionEnd = m_data.getNumSamples();
    rewind();
    return true;
}

void AudioFileSource::setRegion(juce::int64 startSample, juce::int64 endSample)
{
    const juce::int64 length = m_data.getNumSamples();
    if (startSample == 0 && endSample == 0)
    {
        m_regionStart = 0;
        m_regionEnd = length;
    }
    else
    {
        m_regionStart = juce::jlimit<juce::int64>(0, length - 1, startSample);
        m_regionEnd = juce::jlimit<juce::int64>(m_regionStart + 1, length, endSample);
    }
    rewind();
}

void AudioFileSource::setRegionSeconds(double startSeconds, double endSeconds)
{
    setRegion(static_cast<juce::int64>(startSeconds * m_sampleRate), static_cast<juce::int64>(endSeconds * m_sampleRate));
}

juce::int64 AudioFileSource::getLengthSamples() const
{
    return m_data.getNumSamples();
}

juce::int64 AudioFileSource::getRegionStart() const
{
    return m_regionStart;
}

juce::int64 AudioFileSource::getRegionEnd() const
{
    return m_regionEnd;
}

double AudioFileSource::getSampleRate() const
{
    return m_sampleRate;
}

juce::File AudioFileSource::getFile() const
{
    return m_file;
}

juce::String AudioFileSource::getName() const
{
    return m_name;
}

void AudioFileSource::rewind()
{
    m_position = m_regionStart;
    m_completedPasses = 0;
}

int AudioFileSource::getCompletedPasses() const
{
    return m_completedPasses;
}

juce::int64 AudioFileSource::getSamplesLeftInPass() const
{
    return m_regionEnd - m_position;
}

void AudioFileSource::readInto(juce::AudioBuffer<float>& output, int startInOutput, int numSamples)
{
    if (m_data.getNumSamples() == 0)
    {
        return;
    }
    const int sourceChannels = m_data.getNumChannels();
    int written = 0;
    while (written < numSamples)
    {
        const int available = static_cast<int>(m_regionEnd - m_position);
        const int count = juce::jmin(available, numSamples - written);
        for (int channel = 0; channel < output.getNumChannels(); ++channel)
        {
            const int sourceChannel = juce::jmin(channel, sourceChannels - 1);
            output.addFrom(channel, startInOutput + written, m_data, sourceChannel, static_cast<int>(m_position), count);
        }
        m_position += count;
        written += count;
        if (m_position >= m_regionEnd)
        {
            m_position = m_regionStart; // the next pass follows at once
            ++m_completedPasses;
        }
    }
}
}
