#include <memory>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "LoaderEditor.h"
#include "LoaderProcessor.h"
#include "TestPluginPaths.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/LoaderState.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kChannels = 2;
constexpr int kGainIndex = 0;
constexpr float kGainNormalisedPlus12Db = 0.75f;
constexpr float kInputLevel = 0.1f;
constexpr float kGainPlus12Db = 3.98107171f;
constexpr float kTolerance = 0.002f;
}

// LoaderProcessor is the processor of the loader plugin. It gets the state of a session that names the gain plugin and must then
// process the audio through it, as a DAW would do (the plugin wrapper of a host is tested by pluginval).
class LoaderTests : public juce::UnitTest
{
public:
    LoaderTests()
        : juce::UnitTest("LoaderProcessor", "pluginlab")
    {
    }

    void runTest() override
    {
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);

        const pluginlab::hosting::PluginScanResult vst3 =
            pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getGainPlugin());
        expect(! vst3.descriptions.isEmpty(), "the VST3 gain plugin must be scannable: " + vst3.message);
        if (vst3.descriptions.isEmpty())
        {
            return;
        }
        testLoaderWith("VST3", formatManager, vst3.descriptions[0]);

        if (pluginlab::hosting::isVst2Supported())
        {
            const pluginlab::hosting::PluginScanResult vst2 =
                pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getGainPluginVst2());
            expect(! vst2.descriptions.isEmpty(), "the VST2 gain plugin must be scannable: " + vst2.message);
            if (! vst2.descriptions.isEmpty())
            {
                testLoaderWith("VST2", formatManager, vst2.descriptions[0]);
            }
        }

        beginTest("an empty loader passes the audio through");
        LoaderProcessor emptyLoader;
        prepare(emptyLoader);
        expectEquals(processConstantBlock(emptyLoader), kInputLevel);

        beginTest("a plugin that cannot be loaded leaves the loader empty: the audio passes through");
        LoaderProcessor loader;
        prepare(loader);
        juce::PluginDescription missing = vst3.descriptions[0];
        missing.fileOrIdentifier = testpaths::getTestPluginFolder().getChildFile("DoesNotExist.vst3").getFullPathName();
        const juce::MemoryBlock state = pluginlab::hosting::createLoaderState(missing, juce::MemoryBlock());
        loader.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        expect(loader.getHostedPlugin() == nullptr);
        expectEquals(processConstantBlock(loader), kInputLevel);

        beginTest("an instrument is refused with a message and the loader stays empty");
        juce::PluginDescription instrument = vst3.descriptions[0];
        instrument.isInstrument = true;
        juce::String refusal;
        expect(! loader.loadPlugin(instrument, refusal));
        expect(refusal.containsIgnoreCase("instrument"), "the message must say why: " + refusal);
        expect(loader.getHostedPlugin() == nullptr);

        beginTest("a state that is not a loader state is ignored");
        const juce::MemoryBlock garbage("not a loader state", 18);
        loader.setStateInformation(garbage.getData(), static_cast<int>(garbage.getSize()));
        expect(loader.getHostedPlugin() == nullptr);
    }

private:
    void prepare(LoaderProcessor& loader)
    {
        loader.setPlayConfigDetails(kChannels, kChannels, kSampleRate, kBlockSize);
        loader.prepareToPlay(kSampleRate, kBlockSize);
    }

    // processes a block of a constant level and returns the level of the first sample of the first channel
    float processConstantBlock(LoaderProcessor& loader)
    {
        juce::AudioBuffer<float> buffer(kChannels, kBlockSize);
        for (int channel = 0; channel < kChannels; ++channel)
        {
            juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), kInputLevel, kBlockSize);
        }
        juce::MidiBuffer midi;
        loader.processBlock(buffer, midi);
        return buffer.getSample(0, 0);
    }

    void testLoaderWith(const juce::String& formatName, juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& gainDescription)
    {
        beginTest(formatName + ": the loader loads the gain plugin from its state and processes the audio through it");

        // the state of the gain plugin with a gain of +12 dB, made with the plugin itself
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> gainPlugin =
            pluginlab::hosting::HostedPlugin::load(formatManager, gainDescription, kSampleRate, kBlockSize, error);
        expect(gainPlugin != nullptr, "the gain plugin did not load: " + error);
        if (gainPlugin == nullptr)
        {
            return;
        }
        gainPlugin->setParameterNormalised(kGainIndex, kGainNormalisedPlus12Db);
        juce::MemoryBlock gainState;
        gainPlugin->getInstance().getStateInformation(gainState);
        expect(gainState.getSize() > 0, "the gain plugin returned no state");
        gainPlugin.reset();

        LoaderProcessor loader;
        prepare(loader);
        const juce::MemoryBlock state = pluginlab::hosting::createLoaderState(gainDescription, gainState);
        loader.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        expect(loader.getHostedPlugin() != nullptr, "the loader did not load the plugin of the state");

        const float output = processConstantBlock(loader);
        expectWithinAbsoluteError(output, kInputLevel * kGainPlus12Db, kTolerance);

        beginTest(formatName + ": the loader saves which plugin it hosts, with its state");
        juce::MemoryBlock saved;
        loader.getStateInformation(saved);
        juce::PluginDescription savedDescription;
        juce::MemoryBlock savedHostedState;
        const bool parsed = pluginlab::hosting::parseLoaderState(saved.getData(), static_cast<int>(saved.getSize()), savedDescription, savedHostedState);
        expect(parsed, "the saved state is not a loader state");
        if (parsed)
        {
            expectEquals(savedDescription.pluginFormatName, gainDescription.pluginFormatName);
            expectEquals(savedDescription.fileOrIdentifier, gainDescription.fileOrIdentifier);
            expect(savedHostedState.getSize() > 0);
        }

        beginTest(formatName + ": the editor shows the editor of the loaded plugin, drops it before the plugin changes and can be deleted before or after the loader");
        {
            std::unique_ptr<juce::AudioProcessorEditor> editorBase(loader.createEditor());
            auto* editor = dynamic_cast<LoaderEditor*>(editorBase.get());
            expect(editor != nullptr, "the loader created no LoaderEditor");
            if (editor != nullptr)
            {
                expect(! editor->isShowingHostedEditor(), "a plugin that came with the session must not open its window by itself");
                editor->openHostedEditorWindow();
                expect(editor->isShowingHostedEditor(), "after asking for it the window of the plugin must be open");
                loader.unloadPlugin();
                expect(! editor->isShowingHostedEditor(), "after unloading no hosted editor may be left");
                juce::String loadError;
                expect(loader.loadPlugin(gainDescription, loadError), "loading again failed: " + loadError);
                expect(editor->isShowingHostedEditor(), "after loading again the editor must be shown");
                expect(loader.loadPlugin(gainDescription, loadError), "replacing the plugin failed: " + loadError);
                expect(editor->isShowingHostedEditor());
            }
            editorBase.reset(); // the editor goes first: it must take its hosted editor with it
            expect(loader.getHostedPlugin() != nullptr);
        }

        beginTest(formatName + ": the loader unloads the plugin and passes the audio through again");
        loader.unloadPlugin();
        expect(loader.getHostedPlugin() == nullptr);
        expectEquals(processConstantBlock(loader), kInputLevel);
    }
};

static LoaderTests loaderTests;
