#include <memory>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "LoaderProcessor.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginProbe.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/hosting/PluginValidator.h"
#include "TestPluginPaths.h"

namespace
{
// Plugin files to try, separated by ';'. Not set in CI: the plugins belong to the developer, nothing of them is committed.
const juce::String kPluginsVariable = "PLUGINLAB_REALWORLD_PLUGINS";
// Optional: the quick check (as for files with many plugins) is run for the plugins of the files whose name contains this text.
const juce::String kProbeNameVariable = "PLUGINLAB_REALWORLD_PROBE";
constexpr int kProbeTimeoutMs = 60000;
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr int kChannels = 2;
constexpr int kSecondsToProcess = 3;
constexpr float kNoiseLevel = 0.1f;
}

// The robustness harness for third-party plugins: each plugin is loaded into a loader, processed with noise, its state is saved and
// restored into a second loader that is processed again, as a DAW does when it saves and reopens a project. A crash ends the test
// program, which is the finding.
class RealWorldTests : public juce::UnitTest
{
public:
    RealWorldTests()
        : juce::UnitTest("RealWorldPlugins", "pluginlab")
    {
    }

    void runTest() override
    {
        const juce::StringArray files = juce::StringArray::fromTokens(juce::SystemStats::getEnvironmentVariable(kPluginsVariable, {}), ";", "");
        if (files.isEmpty())
        {
            beginTest("skipped: " + kPluginsVariable + " is not set");
            return;
        }

        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        for (const juce::String& path : files)
        {
            testPlugin(formatManager, juce::File(path));
        }
    }

private:
    static void prepare(LoaderProcessor& loader)
    {
        loader.setPlayConfigDetails(kChannels, kChannels, kSampleRate, kBlockSize);
        loader.prepareToPlay(kSampleRate, kBlockSize);
    }

    static void process(LoaderProcessor& loader)
    {
        juce::Random random(1);
        juce::AudioBuffer<float> buffer(kChannels, kBlockSize);
        juce::MidiBuffer midi;
        const int blocks = static_cast<int>(kSampleRate * kSecondsToProcess / kBlockSize);
        for (int block = 0; block < blocks; ++block)
        {
            for (int channel = 0; channel < kChannels; ++channel)
            {
                for (int sample = 0; sample < kBlockSize; ++sample)
                {
                    buffer.setSample(channel, sample, kNoiseLevel * (random.nextFloat() * 2.0f - 1.0f));
                }
            }
            loader.processBlock(buffer, midi);
        }
    }

    void testPlugin(juce::AudioPluginFormatManager& formatManager, const juce::File& file)
    {
        beginTest(file.getFileName());
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner(testpaths::getScannerExecutable()).scanFile(file); // in a scanner process: a plugin file may crash while it is scanned
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName() + ": " + scan.message);
        if (scan.descriptions.isEmpty())
        {
            return;
        }

        const juce::String probeName = juce::SystemStats::getEnvironmentVariable(kProbeNameVariable, {});
        if (probeName.isNotEmpty())
        {
            for (const juce::PluginDescription& description : scan.descriptions)
            {
                if (! description.name.containsIgnoreCase(probeName))
                {
                    continue;
                }
                const juce::int64 started = juce::Time::getMillisecondCounter();
                const pluginlab::hosting::ValidationResult result = pluginlab::hosting::probePlugin(
                    testpaths::getScannerExecutable(), file, description.createIdentifierString(), kProbeTimeoutMs);
                logMessage("quick check of " + description.name + ": " + pluginlab::hosting::toString(result.status) + " in "
                           + juce::String(static_cast<int>(juce::Time::getMillisecondCounter() - started)) + " ms: " + result.message);
            }
            return;
        }

        juce::MemoryBlock state;
        {
            LoaderProcessor loader;
            prepare(loader);
            juce::String error;
            if (! loader.loadPlugin(scan.descriptions[0], error))
            {
                logMessage("not loaded: " + error);
                return;
            }
            logMessage("loaded, processing");
            process(loader);
            loader.getStateInformation(state);
            logMessage("state saved: " + juce::String(state.getSize()) + " bytes");
        }
        logMessage("first loader deleted");
        {
            LoaderProcessor loader;
            prepare(loader);
            loader.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            expect(loader.getHostedPlugin() != nullptr, "the state must bring the plugin back");
            logMessage("state restored, processing");
            process(loader);
        }
        logMessage("second loader deleted");
    }
};

static RealWorldTests realWorldTests;
