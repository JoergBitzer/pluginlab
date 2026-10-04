#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace pluginlab::engine
{
// An audio file in memory, at the sample rate of the engine, played as a series of passes over a region (the whole file unless a loop
// region is set). A pass that ends is followed by the next pass without a gap (sample exact). Mono files are copied to all channels.
class AudioFileSource
{
public:
    // Reads the whole file and converts it to targetSampleRate. Returns false and sets error if the file cannot be read.
    bool load(const juce::File& file, double targetSampleRate, juce::String& error);

    // The loop region in samples of the loaded (converted) file; end is exclusive. An end that is 0 or not after the start means: up to the
    // end of the file, so 0, 0 selects the whole file.
    void setRegion(juce::int64 startSample, juce::int64 endSample);
    // The same in seconds of the file
    void setRegionSeconds(double startSeconds, double endSeconds);

    juce::int64 getLengthSamples() const;
    juce::int64 getRegionStart() const;
    juce::int64 getRegionEnd() const;
    double getSampleRate() const;
    juce::String getName() const;
    juce::File getFile() const;

    // Back to the first sample of the region, no pass completed.
    void rewind();

    // How many passes over the region are complete.
    int getCompletedPasses() const;

    // Adds numSamples samples to the output buffer starting at the sample startInOutput (the buffer is not cleared): the source keeps
    // playing, so the passes follow each other. The output has any number of channels.
    void readInto(juce::AudioBuffer<float>& output, int startInOutput, int numSamples);

    // The samples that are left in the current pass.
    juce::int64 getSamplesLeftInPass() const;

private:
    juce::File m_file;
    juce::String m_name;
    double m_sampleRate = 0.0;
    juce::AudioBuffer<float> m_data;
    juce::int64 m_regionStart = 0;
    juce::int64 m_regionEnd = 0;
    juce::int64 m_position = 0;
    int m_completedPasses = 0;
};
}
