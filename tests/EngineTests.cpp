#include <cmath>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/AudioFileSource.h"
#include "pluginlab/engine/LatencyMeasurer.h"
#include "pluginlab/engine/MeasurementEngine.h"
#include "pluginlab/engine/OfflineRenderer.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 256;
constexpr int kChannels = 2;
constexpr int kGainIndex = 0;
constexpr float kGainNormalisedPlus12Db = 0.75f;
constexpr float kPi = 3.14159265f;
constexpr double kToneHz = 440.0;
constexpr float kToneLevel = 0.5f;
constexpr int kLatencyPluginDelay = 64;
constexpr int kLiarPluginDelay = 100;
constexpr int kImpulsePosition = 10;
}

// The audio engine: files and loops, latency measurement, alignment of the slots, gapless switching, rendering to files.
class EngineTests : public juce::UnitTest
{
public:
    EngineTests()
        : juce::UnitTest("MeasurementEngine", "pluginlab")
    {
    }

    void runTest() override
    {
        pluginlab::hosting::addHeadlessFormats(m_formats);
        testFileSource();
        testLatencyMeasurement();
        testAlignment();
        testSwitching();
        testOfflineRendering();
    }

private:
    // a WAV file with the given samples (one channel or two identical ones)
    juce::File writeWav(const juce::TemporaryFile& temporary, const std::vector<float>& samples, double sampleRate, int channels)
    {
        const juce::File file = temporary.getFile();
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream = file.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), sampleRate, static_cast<unsigned int>(channels), 32, {}, 0));
        stream.release();
        juce::AudioBuffer<float> buffer(channels, static_cast<int>(samples.size()));
        for (int channel = 0; channel < channels; ++channel)
        {
            for (size_t index = 0; index < samples.size(); ++index)
            {
                buffer.setSample(channel, static_cast<int>(index), samples[index]);
            }
        }
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        return file;
    }

    static std::vector<float> makeTone(int length, double sampleRate)
    {
        std::vector<float> samples(static_cast<size_t>(length));
        for (int index = 0; index < length; ++index)
        {
            samples[static_cast<size_t>(index)] = kToneLevel * std::sin(2.0f * kPi * static_cast<float>(kToneHz * index / sampleRate));
        }
        return samples;
    }

    std::unique_ptr<pluginlab::hosting::HostedPlugin> loadPlugin(const juce::File& file)
    {
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(m_formats, file);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (scan.descriptions.isEmpty())
        {
            return nullptr;
        }
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
            pluginlab::hosting::HostedPlugin::load(m_formats, scan.descriptions[0], kSampleRate, kBlockSize, error);
        expect(plugin != nullptr, "cannot load " + file.getFullPathName() + ": " + error);
        return plugin;
    }

    void testFileSource()
    {
        beginTest("a file is played in passes that follow each other without a gap, mono becomes stereo");
        const juce::TemporaryFile temporary(".wav");
        std::vector<float> ramp;
        for (int index = 0; index < 100; ++index)
        {
            ramp.push_back(static_cast<float>(index) / 100.0f);
        }
        const juce::File file = writeWav(temporary, ramp, kSampleRate, 1);
        pluginlab::engine::AudioFileSource source;
        juce::String error;
        expect(source.load(file, kSampleRate, error), error);
        source.setRegion(10, 20); // 10 samples: 0.10 ... 0.19
        juce::AudioBuffer<float> out(kChannels, 25);
        out.clear();
        source.readInto(out, 0, 25);
        for (int index = 0; index < 25; ++index)
        {
            const float expected = static_cast<float>(10 + index % 10) / 100.0f;
            expectWithinAbsoluteError(out.getSample(0, index), expected, 1.0e-6f);
            expectWithinAbsoluteError(out.getSample(1, index), expected, 1.0e-6f);
        }
        expectEquals(source.getCompletedPasses(), 2);

        beginTest("a file is converted to the sample rate of the engine without changing the pitch");
        const juce::TemporaryFile toneFile(".wav");
        const juce::File tone = writeWav(toneFile, makeTone(44100, 44100.0), 44100.0, 1);
        pluginlab::engine::AudioFileSource converted;
        expect(converted.load(tone, kSampleRate, error), error);
        expectWithinAbsoluteError(static_cast<double>(converted.getLengthSamples()), 48000.0, 2.0);
        juce::AudioBuffer<float> buffer(kChannels, static_cast<int>(kSampleRate));
        buffer.clear();
        converted.readInto(buffer, 0, buffer.getNumSamples());
        int crossings = 0;
        for (int index = 1; index < buffer.getNumSamples(); ++index)
        {
            if (buffer.getSample(0, index - 1) < 0.0f && buffer.getSample(0, index) >= 0.0f)
            {
                ++crossings;
            }
        }
        expectWithinAbsoluteError(static_cast<double>(crossings), kToneHz, 2.0); // one second: 440 periods
    }

    void testLatencyMeasurement()
    {
        beginTest("the latency of a plugin is measured: the one that reports it right, the one that lies, the one without latency");
        const struct
        {
            juce::File file;
            int delay;
            int reported;
        } cases[] = {{testpaths::getLatencyPlugin(), kLatencyPluginDelay, kLatencyPluginDelay},
                     {testpaths::getLatencyLiarPlugin(), kLiarPluginDelay, 0},
                     {testpaths::getGainPlugin(), 0, 0}};
        for (const auto& item : cases)
        {
            std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin = loadPlugin(item.file);
            if (plugin == nullptr)
            {
                continue;
            }
            const pluginlab::engine::LatencyResult latency =
                pluginlab::engine::measureLatency(plugin->getInstance(), kSampleRate, kBlockSize, kChannels);
            expect(latency.found);
            expectEquals(latency.measuredSamples, item.delay, item.file.getFileName() + ": measured");
            expectEquals(latency.reportedSamples, item.reported, item.file.getFileName() + ": reported");
        }
    }

    // an engine with the dry slot and both latency plugins, and a file with one impulse
    void prepareEngine(pluginlab::engine::MeasurementEngine& engine, const juce::File& impulseFile)
    {
        engine.prepare(kSampleRate, kBlockSize);
        juce::String error;
        expect(engine.addFile(impulseFile, 1, error), error);
        expect(engine.addSlot(nullptr, "dry", error) == 0, error);
        expect(engine.addSlot(loadPlugin(testpaths::getLatencyPlugin()), "latency64", error) == 1, error);
        expect(engine.addSlot(loadPlugin(testpaths::getLatencyLiarPlugin()), "liar100", error) == 2, error);
    }

    static int findPeak(const juce::AudioBuffer<float>& buffer)
    {
        int position = 0;
        float peak = 0.0f;
        for (int index = 0; index < buffer.getNumSamples(); ++index)
        {
            if (std::abs(buffer.getSample(0, index)) > peak)
            {
                peak = std::abs(buffer.getSample(0, index));
                position = index;
            }
        }
        return position;
    }

    void testAlignment()
    {
        beginTest("all slots are time aligned to the slowest one (measured, not reported)");
        const juce::TemporaryFile temporary(".wav");
        std::vector<float> impulse(1000, 0.0f);
        impulse[kImpulsePosition] = 1.0f;
        pluginlab::engine::MeasurementEngine engine;
        prepareEngine(engine, writeWav(temporary, impulse, kSampleRate, 1));
        expectEquals(engine.getLatencyOfEngine(), kLiarPluginDelay);
        expectEquals(engine.getSlotInfo(0).compensation, kLiarPluginDelay);
        expectEquals(engine.getSlotInfo(1).compensation, kLiarPluginDelay - kLatencyPluginDelay);
        expectEquals(engine.getSlotInfo(2).compensation, 0);
        expectEquals(engine.getSlotInfo(2).reportedLatency, 0);
        expectEquals(engine.getSlotInfo(2).measuredLatency, kLiarPluginDelay);

        engine.restart();
        std::vector<juce::AudioBuffer<float>> outputs;
        juce::AudioBuffer<float> all(kChannels, 2 * kBlockSize);
        all.clear();
        std::vector<int> peaks(3, -1);
        for (int block = 0; block < 2; ++block)
        {
            engine.processAllSlots(kBlockSize, outputs);
            for (size_t slot = 0; slot < outputs.size(); ++slot)
            {
                const int peak = findPeak(outputs[slot]);
                if (outputs[slot].getSample(0, peak) > 0.5f)
                {
                    peaks[slot] = block * kBlockSize + peak;
                }
            }
        }
        for (size_t slot = 0; slot < peaks.size(); ++slot)
        {
            expectEquals(peaks[slot], kImpulsePosition + kLiarPluginDelay, "slot " + juce::String(static_cast<int>(slot)));
        }
    }

    void testSwitching()
    {
        beginTest("switching between slots is gapless: no silent block, no jump bigger than the signal itself allows");
        const juce::TemporaryFile temporary(".wav");
        const juce::File toneFile = writeWav(temporary, makeTone(static_cast<int>(kSampleRate), kSampleRate), kSampleRate, 1);
        pluginlab::engine::MeasurementEngine engine;
        engine.prepare(kSampleRate, kBlockSize);
        juce::String error;
        expect(engine.addFile(toneFile, 0, error), error); // for ever
        expect(engine.addSlot(nullptr, "dry", error) == 0, error);
        expect(engine.addSlot(loadPlugin(testpaths::getGainPlugin()), "gain +12 dB", error) == 1, error);
        engine.getPlugin(1)->setParameterNormalised(kGainIndex, kGainNormalisedPlus12Db);

        juce::AudioBuffer<float> block(kChannels, kBlockSize);
        float previous = 0.0f;
        float largestStep = 0.0f;
        int silentBlocks = 0;
        const int blocks = 400;
        for (int index = 0; index < blocks; ++index)
        {
            if (index % 50 == 25)
            {
                const bool useGainSlot = (index / 50) % 2 == 0;
                if (useGainSlot)
                {
                    engine.setActiveSlot(1);
                }
                else
                {
                    engine.setActiveSlot(0);
                } // alternate often
            }
            engine.processBlock(block);
            if (block.getRMSLevel(0, 0, kBlockSize) < 0.05f)
            {
                ++silentBlocks;
            }
            for (int sample = 0; sample < kBlockSize; ++sample)
            {
                if (index > 0 || sample > 0)
                {
                    largestStep = juce::jmax(largestStep, std::abs(block.getSample(0, sample) - previous));
                }
                previous = block.getSample(0, sample);
            }
        }
        logMessage("largest step between two samples while switching: " + juce::String(largestStep));
        expectEquals(silentBlocks, 0);
        // a sine of amplitude 0.5 * 4 (+12 dB) has a largest step of 2 pi f / fs * 2 = 0.115; the crossfade adds little. A hard switch
        // would jump by about 1.5.
        expect(largestStep < 0.2f, "largest step " + juce::String(largestStep));
        expectEquals(engine.getActiveSlot(), 0);
    }

    void testOfflineRendering()
    {
        beginTest("offline rendering writes one aligned file per slot");
        const juce::TemporaryFile temporary(".wav");
        const std::vector<float> tone = makeTone(5000, kSampleRate);
        pluginlab::engine::MeasurementEngine engine;
        engine.prepare(kSampleRate, kBlockSize);
        juce::String error;
        expect(engine.addFile(writeWav(temporary, tone, kSampleRate, 1), 1, error), error);
        expect(engine.addSlot(nullptr, "dry", error) == 0, error);
        expect(engine.addSlot(loadPlugin(testpaths::getLatencyPlugin()), "latency 64/x", error) == 1, error);

        const juce::TemporaryFile folder(".dir");
        pluginlab::engine::OfflineRenderOptions options;
        options.tailSeconds = 0.01;
        const pluginlab::engine::OfflineRenderResult result = pluginlab::engine::renderOffline(engine, folder.getFile(), options);
        expect(result.ok, result.message);
        expectEquals(static_cast<int>(result.files.size()), 2);
        if (result.files.size() != 2)
        {
            return;
        }
        expect(result.files[1].getFileName().contains("latency_64_x"), result.files[1].getFileName());

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        for (const juce::File& file : result.files)
        {
            const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
            expect(reader != nullptr);
            if (reader == nullptr)
            {
                continue;
            }
            expectEquals(static_cast<int>(reader->lengthInSamples), static_cast<int>(result.samplesPerFile));
            juce::AudioBuffer<float> data(kChannels, static_cast<int>(tone.size()));
            reader->read(&data, 0, static_cast<int>(tone.size()), 0, true, true);
            float largestDifference = 0.0f;
            for (size_t index = 0; index < tone.size(); ++index)
            {
                largestDifference = juce::jmax(largestDifference, std::abs(data.getSample(0, static_cast<int>(index)) - tone[index]));
            }
            // 24 bit files: aligned to the input and equal to it (the test plugins do not change the level)
            expect(largestDifference < 1.0e-5f, file.getFileName() + ": largest difference to the input " + juce::String(largestDifference));
        }
    }

    juce::AudioPluginFormatManager m_formats;
};

static EngineTests engineTests;
