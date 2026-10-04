#pragma once

#include <memory>
#include <vector>

#include "pluginlab/engine/AudioFileSource.h"
#include "pluginlab/engine/RenderSlot.h"

namespace pluginlab::engine
{
struct FileInfo
{
    juce::String name;
    double lengthSeconds = 0.0;
    int passes = 1;
    double regionStartSeconds = 0.0;
    double regionEndSeconds = 0.0;
};

struct SlotInfo
{
    juce::String name;
    bool hasPlugin = false;
    bool latencyFound = false;
    int measuredLatency = 0;
    int reportedLatency = 0;
    int compensation = 0;
};

// The audio engine of the host. A list of audio files is played (every file for a number of passes over its region, then the next file)
// through several slots at the same time. All slots always process the same input; one slot is audible. Switching moves a short
// crossfade between the outputs, nothing is stopped or started, so the plugins never see a gap. All slots are delayed to the latency of
// the slowest one (measured, not reported) so that a switch does not jump in time.
//
// Threads: the audio thread calls processBlock() (and nothing else); everything else is called from one other (the message) thread.
// Changing the list of slots or files holds a lock for a moment: a block that arrives then is silent.
class MeasurementEngine
{
public:
    static constexpr int kChannels = 2;
    static constexpr float kDefaultCrossfadeMs = 10.0f;

    // maxBlockSize: the largest block processBlock() is called with (larger blocks are cut)
    void prepare(double sampleRate, int maxBlockSize);
    double getSampleRate() const;

    // ---- files (message thread) ----
    // passes: how often the region is played before the next file follows (0: for ever)
    bool addFile(const juce::File& file, int passes, juce::String& error);
    bool addFile(std::unique_ptr<AudioFileSource> source, int passes);
    void clearFiles();
    void removeFile(int index);
    int getNumFiles() const;
    FileInfo getFileInfo(int index) const;
    void setFilePasses(int index, int passes);
    // The loop region of the file in seconds (0, 0: the whole file)
    void setFileRegionSeconds(int index, double startSeconds, double endSeconds);
    // After the last file the list starts again (default, for listening) or the engine plays silence and isFinished() becomes true.
    void setWrapList(bool wrap);
    bool isFinished() const;

    // ---- slots (message thread) ----
    // Adds a slot (nullptr plugin: the dry reference): prepares it, measures its latency and aligns all slots. Returns its index, -1 on error.
    int addSlot(std::unique_ptr<hosting::HostedPlugin> plugin, const juce::String& name, juce::String& error);
    void clearSlots();
    void removeSlot(int index);
    int getNumSlots() const;
    SlotInfo getSlotInfo(int index) const;
    // The plugin of the slot (message thread only), nullptr for the dry slot.
    hosting::HostedPlugin* getPlugin(int index);

    // The audible slot; the change is a crossfade.
    void setActiveSlot(int index);
    int getActiveSlot() const;
    void setCrossfadeMs(float milliseconds);

    // Back to the first sample of the first file, all slots restarted (their plugins forget what they processed).
    void restart();

    // ---- audio ----
    // Fills the output (kChannels channels) with the audible slot. Audio thread.
    void processBlock(juce::AudioBuffer<float>& output);

    // Offline use: processes numSamples (at most the maximum block) and gives the output of every slot, aligned (compensated).
    void processAllSlots(int numSamples, std::vector<juce::AudioBuffer<float>>& slotOutputs);

    // The largest latency of all slots: the output of a slot is that much later than the input of the engine.
    int getLatencyOfEngine() const;
    int getMaxBlockSize() const;

private:
    struct FileItem
    {
        std::unique_ptr<AudioFileSource> source;
        int passes = 1;
    };

    void fillInput(int numSamples);
    void alignSlots();
    void processChunk(juce::AudioBuffer<float>& output, int start, int numSamples);

    double m_sampleRate = 0.0;
    int m_maxBlockSize = 0;
    float m_crossfadeMs = kDefaultCrossfadeMs;

    mutable juce::CriticalSection m_lock;
    std::vector<FileItem> m_files;
    std::vector<std::unique_ptr<RenderSlot>> m_slots;
    bool m_wrapList = true;
    bool m_finished = false;
    size_t m_currentFile = 0;

    juce::AudioBuffer<float> m_input;
    std::vector<juce::AudioBuffer<float>> m_slotBuffers;
    juce::AudioBuffer<float> m_chunk;

    int m_activeSlot = 0;
    int m_previousSlot = 0;
    int m_fadeLength = 0;
    int m_fadeLeft = 0;
};
}
