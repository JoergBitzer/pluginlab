#include "pluginlab/engine/OfflineRenderer.h"

namespace pluginlab::engine
{
namespace
{
constexpr int kIndexDigits = 2;
const juce::String kFileExtension = ".wav";
constexpr int kMaximumBlocks = 100000000; // a guard against a list that never ends

}

juce::String makeFileNamePart(const juce::String& text)
{
    juce::String result;
    for (const juce::juce_wchar character : text)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character) || character == '-' || character == '_')
        {
            result += juce::String::charToString(character);
            continue;
        }
        result += "_";
    }
    return result;
}

OfflineRenderResult renderOffline(MeasurementEngine& engine, const juce::File& folder, const OfflineRenderOptions& options)
{
    OfflineRenderResult result;
    const int numberOfSlots = engine.getNumSlots();
    if (numberOfSlots == 0 || engine.getNumFiles() == 0)
    {
        result.message = "Nothing to render: the engine needs at least one slot and one file";
        return result;
    }
    if (! folder.createDirectory())
    {
        result.message = "Cannot create the folder " + folder.getFullPathName();
        return result;
    }

    juce::WavAudioFormat wav;
    std::vector<std::unique_ptr<juce::AudioFormatWriter>> writers;
    for (int index = 0; index < numberOfSlots; ++index)
    {
        const juce::File file = folder.getChildFile(juce::String(index).paddedLeft('0', kIndexDigits) + "_"
                                                    + makeFileNamePart(engine.getSlotInfo(index).name) + kFileExtension);
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
        {
            result.message = "Cannot write " + file.getFullPathName();
            return result;
        }
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wav.createWriterFor(stream.get(), engine.getSampleRate(), MeasurementEngine::kChannels, options.bitsPerSample, {}, 0));
        if (writer == nullptr)
        {
            result.message = "Cannot create a WAV writer for " + file.getFullPathName();
            return result;
        }
        stream.release(); // the writer owns the stream now
        writers.push_back(std::move(writer));
        result.files.push_back(file);
    }

    engine.setWrapList(false);
    engine.restart();

    const int latency = engine.getLatencyOfEngine();
    const int blockSize = engine.getMaxBlockSize();
    const int tailSamples = static_cast<int>(options.tailSeconds * engine.getSampleRate());
    int silentSamplesAfterEnd = 0;
    juce::int64 skipped = 0;
    std::vector<juce::AudioBuffer<float>> outputs;
    int blocks = 0;
    while (silentSamplesAfterEnd < latency + tailSamples && blocks < kMaximumBlocks)
    {
        const bool finishedBefore = engine.isFinished();
        engine.processAllSlots(blockSize, outputs);
        if (finishedBefore)
        {
            silentSamplesAfterEnd += blockSize;
        }
        // cut the latency off the start: the first samples of the outputs are the delay of the slots
        int skipNow = 0;
        if (skipped < latency)
        {
            skipNow = static_cast<int>(juce::jmin<juce::int64>(latency - skipped, blockSize));
            skipped += skipNow;
        }
        const int count = blockSize - skipNow;
        for (int index = 0; index < numberOfSlots && count > 0; ++index)
        {
            const juce::AudioBuffer<float>& output = outputs[static_cast<size_t>(index)];
            const float* channels[MeasurementEngine::kChannels] = {output.getReadPointer(0, skipNow), output.getReadPointer(1, skipNow)};
            writers[static_cast<size_t>(index)]->writeFromFloatArrays(channels, MeasurementEngine::kChannels, count);
        }
        result.samplesPerFile += count;
        ++blocks;
    }
    writers.clear(); // flushes and closes the files

    engine.setWrapList(true);
    result.ok = true;
    result.message = "Rendered " + juce::String(numberOfSlots) + " files of " + juce::String(result.samplesPerFile) + " samples";
    return result;
}
}
