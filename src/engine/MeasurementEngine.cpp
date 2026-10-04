#include "pluginlab/engine/MeasurementEngine.h"

#include <algorithm>

namespace pluginlab::engine
{
void MeasurementEngine::prepare(double sampleRate, int maxBlockSize)
{
    const juce::ScopedLock lock(m_lock);
    m_sampleRate = sampleRate;
    m_maxBlockSize = maxBlockSize;
    m_input.setSize(kChannels, maxBlockSize);
    m_chunk.setSize(kChannels, maxBlockSize);
    m_slotBuffers.clear();
    m_slots.clear();
    m_fadeLength = static_cast<int>(sampleRate * m_crossfadeMs / 1000.0f);
}

double MeasurementEngine::getSampleRate() const
{
    return m_sampleRate;
}

int MeasurementEngine::getMaxBlockSize() const
{
    return m_maxBlockSize;
}

bool MeasurementEngine::addFile(const juce::File& file, int passes, juce::String& error)
{
    auto source = std::make_unique<AudioFileSource>();
    if (! source->load(file, m_sampleRate, error))
    {
        return false;
    }
    return addFile(std::move(source), passes);
}

bool MeasurementEngine::addFile(std::unique_ptr<AudioFileSource> source, int passes)
{
    FileItem item;
    item.source = std::move(source);
    item.passes = passes;
    const juce::ScopedLock lock(m_lock);
    m_files.push_back(std::move(item));
    return true;
}

void MeasurementEngine::clearFiles()
{
    const juce::ScopedLock lock(m_lock);
    m_files.clear();
    m_currentFile = 0;
    m_finished = false;
}

int MeasurementEngine::getNumFiles() const
{
    const juce::ScopedLock lock(m_lock);
    return static_cast<int>(m_files.size());
}

void MeasurementEngine::setWrapList(bool wrap)
{
    const juce::ScopedLock lock(m_lock);
    m_wrapList = wrap;
}

bool MeasurementEngine::isFinished() const
{
    const juce::ScopedLock lock(m_lock);
    return m_finished;
}

int MeasurementEngine::addSlot(std::unique_ptr<hosting::HostedPlugin> plugin, const juce::String& name, juce::String& error)
{
    auto slot = std::make_unique<RenderSlot>(std::move(plugin), name);
    if (! slot->prepare(m_sampleRate, m_maxBlockSize, kChannels, error)) // slow (measures, warms up): not under the lock
    {
        return -1;
    }
    const juce::ScopedLock lock(m_lock);
    m_slots.push_back(std::move(slot));
    m_slotBuffers.emplace_back(kChannels, m_maxBlockSize);
    alignSlots();
    return static_cast<int>(m_slots.size()) - 1;
}

// Caller holds the lock. Every slot is delayed by the difference to the slowest one.
void MeasurementEngine::alignSlots()
{
    int slowest = 0;
    for (const std::unique_ptr<RenderSlot>& slot : m_slots)
    {
        slowest = std::max(slowest, slot->getLatency().measuredSamples);
    }
    for (const std::unique_ptr<RenderSlot>& slot : m_slots)
    {
        slot->setCompensation(slowest - slot->getLatency().measuredSamples);
    }
}

void MeasurementEngine::clearSlots()
{
    const juce::ScopedLock lock(m_lock);
    m_slots.clear();
    m_slotBuffers.clear();
    m_activeSlot = 0;
    m_previousSlot = 0;
    m_fadeLeft = 0;
}

int MeasurementEngine::getNumSlots() const
{
    const juce::ScopedLock lock(m_lock);
    return static_cast<int>(m_slots.size());
}

SlotInfo MeasurementEngine::getSlotInfo(int index) const
{
    const juce::ScopedLock lock(m_lock);
    SlotInfo info;
    if (index < 0 || index >= static_cast<int>(m_slots.size()))
    {
        return info;
    }
    const RenderSlot& slot = *m_slots[static_cast<size_t>(index)];
    info.name = slot.getName();
    info.hasPlugin = m_slots[static_cast<size_t>(index)]->getPlugin() != nullptr;
    info.latencyFound = slot.getLatency().found;
    info.measuredLatency = slot.getLatency().measuredSamples;
    info.reportedLatency = slot.getLatency().reportedSamples;
    info.compensation = slot.getCompensation();
    return info;
}

hosting::HostedPlugin* MeasurementEngine::getPlugin(int index)
{
    const juce::ScopedLock lock(m_lock);
    if (index < 0 || index >= static_cast<int>(m_slots.size()))
    {
        return nullptr;
    }
    return m_slots[static_cast<size_t>(index)]->getPlugin();
}

int MeasurementEngine::getLatencyOfEngine() const
{
    const juce::ScopedLock lock(m_lock);
    int slowest = 0;
    for (const std::unique_ptr<RenderSlot>& slot : m_slots)
    {
        slowest = std::max(slowest, slot->getLatency().measuredSamples);
    }
    return slowest;
}

void MeasurementEngine::setActiveSlot(int index)
{
    const juce::ScopedLock lock(m_lock);
    if (index < 0 || index >= static_cast<int>(m_slots.size()) || index == m_activeSlot)
    {
        return;
    }
    m_previousSlot = m_activeSlot;
    m_activeSlot = index;
    m_fadeLeft = m_fadeLength;
}

int MeasurementEngine::getActiveSlot() const
{
    const juce::ScopedLock lock(m_lock);
    return m_activeSlot;
}

void MeasurementEngine::setCrossfadeMs(float milliseconds)
{
    const juce::ScopedLock lock(m_lock);
    m_crossfadeMs = milliseconds;
    m_fadeLength = static_cast<int>(m_sampleRate * milliseconds / 1000.0f);
}

void MeasurementEngine::restart()
{
    const juce::ScopedLock lock(m_lock);
    for (FileItem& item : m_files)
    {
        item.source->rewind();
    }
    m_currentFile = 0;
    m_finished = false;
    for (const std::unique_ptr<RenderSlot>& slot : m_slots)
    {
        slot->restart();
    }
}

// Caller holds the lock. The input of one chunk: the files of the list in order, every one for its number of passes.
void MeasurementEngine::fillInput(int numSamples)
{
    m_input.clear();
    int filled = 0;
    while (filled < numSamples)
    {
        if (m_files.empty() || m_finished)
        {
            return; // silence
        }
        FileItem& item = m_files[m_currentFile];
        int count = numSamples - filled;
        if (item.passes > 0)
        {
            count = static_cast<int>(juce::jmin<juce::int64>(count, item.source->getSamplesLeftInPass()));
        }
        item.source->readInto(m_input, filled, count);
        filled += count;
        if (item.passes > 0 && item.source->getCompletedPasses() >= item.passes)
        {
            item.source->rewind();
            ++m_currentFile;
            if (m_currentFile >= m_files.size())
            {
                m_currentFile = 0;
                m_finished = ! m_wrapList;
            }
        }
    }
}

// Caller holds the lock; the slot buffers are filled
void MeasurementEngine::processChunk(juce::AudioBuffer<float>& output, int start, int numSamples)
{
    juce::AudioBuffer<float> chunkInput(m_input.getArrayOfWritePointers(), kChannels, numSamples);
    for (size_t index = 0; index < m_slots.size(); ++index)
    {
        juce::AudioBuffer<float> slotOutput(m_slotBuffers[index].getArrayOfWritePointers(), kChannels, numSamples);
        m_slots[index]->process(chunkInput, slotOutput);
    }

    const juce::AudioBuffer<float>& current = m_slotBuffers[static_cast<size_t>(m_activeSlot)];
    const juce::AudioBuffer<float>& previous = m_slotBuffers[static_cast<size_t>(m_previousSlot)];
    for (int sample = 0; sample < numSamples; ++sample)
    {
        float newWeight = 1.0f;
        if (m_fadeLeft > 0)
        {
            newWeight = 1.0f - static_cast<float>(m_fadeLeft) / static_cast<float>(m_fadeLength);
            --m_fadeLeft;
        }
        for (int channel = 0; channel < kChannels; ++channel)
        {
            const float now = current.getSample(channel, sample);
            const float before = previous.getSample(channel, sample);
            output.setSample(channel, start + sample, before + (now - before) * newWeight);
        }
    }
}

void MeasurementEngine::processBlock(juce::AudioBuffer<float>& output)
{
    output.clear();
    const juce::ScopedTryLock lock(m_lock);
    if (! lock.isLocked() || m_slots.empty())
    {
        return;
    }
    const int total = output.getNumSamples();
    for (int start = 0; start < total; start += m_maxBlockSize)
    {
        const int count = juce::jmin(m_maxBlockSize, total - start);
        fillInput(count);
        processChunk(output, start, count);
    }
}

void MeasurementEngine::processAllSlots(int numSamples, std::vector<juce::AudioBuffer<float>>& slotOutputs)
{
    const juce::ScopedLock lock(m_lock);
    fillInput(numSamples);
    juce::AudioBuffer<float> chunkInput(m_input.getArrayOfWritePointers(), kChannels, numSamples);
    slotOutputs.resize(m_slots.size());
    for (size_t index = 0; index < m_slots.size(); ++index)
    {
        slotOutputs[index].setSize(kChannels, numSamples, false, false, true);
        m_slots[index]->process(chunkInput, slotOutputs[index]);
    }
}
}
