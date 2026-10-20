#include <cmath>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdlib>

#include "BenchmarkProcessor.h"
#include "ComparePanel.h"
#include "pluginlab/ui/ParameterTableComponent.h"
#include "TestPluginPaths.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kGainIndex = 0;
constexpr float kGainNormalisedPlus12Db = 0.75f;  // the gain test plugin: -24 ... 24 dB
constexpr float kGainPlus12Db = 3.98107171f;
constexpr float kInputLevel = 0.1f;
constexpr float kTolerance = 0.002f;
constexpr int kLatencyPluginDelay = 64;
constexpr int kBlocksToSettle = 200;               // the engine's alignment delay and the crossfade must be through
constexpr int kRestoreWaitMs = 3000;
constexpr int kEditorWidth = 1100;                 // the editor's default size
constexpr int kEditorHeight = 760;
constexpr int kCompareTab = 2; // Easy, Plugins, EQ Compare, Matcher, Expert
constexpr int kExpertTab = 4;
}

// W10.2: the processor of Jade Benchmark, the plugin product (docs/design/W10-plan.md): the DAW's track through the slots, the latency the DAW
// is told, the state in the project. The plugin wrapper of a DAW is tested by pluginval.
class BenchmarkTests : public juce::UnitTest
{
public:
    BenchmarkTests()
        : juce::UnitTest("Jade Benchmark (W10.2)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the dry slot passes the track through");
        {
            BenchmarkProcessor processor;
            processor.prepareToPlay(kSampleRate, kBlockSize);
            expectWithinAbsoluteError(processSteadyLevel(processor), kInputLevel, kTolerance);
            expectEquals(processor.getLatencySamples(), 0);
        }

        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const pluginlab::hosting::PluginScanResult gain = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getGainPlugin());
        const pluginlab::hosting::PluginScanResult latency = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getLatencyPlugin());
        expect(! gain.descriptions.isEmpty() && ! latency.descriptions.isEmpty(), "the test plugins must be scannable");
        if (gain.descriptions.isEmpty() || latency.descriptions.isEmpty())
        {
            return;
        }

        beginTest("a plugin in the audible slot processes the track; switching back to dry is a crossfade to the original");
        juce::MemoryBlock state;
        {
            BenchmarkProcessor processor;
            processor.prepareToPlay(kSampleRate, kBlockSize);
            const int slot = addSlot(processor, gain.descriptions[0]);
            expect(slot > 0, "the gain plugin in a slot");
            processor.getEngine().getPlugin(slot)->setParameterNormalised(kGainIndex, kGainNormalisedPlus12Db);
            processor.getEngine().setActiveSlot(slot);
            expectWithinAbsoluteError(processSteadyLevel(processor), kInputLevel * kGainPlus12Db, kTolerance * kGainPlus12Db);
            processor.getEngine().setActiveSlot(0);
            expectWithinAbsoluteError(processSteadyLevel(processor), kInputLevel, kTolerance);

            beginTest("the latency the DAW is told follows the slowest slot");
            addSlot(processor, latency.descriptions[0]);
            expectEquals(processor.getLatencySamples(), kLatencyPluginDelay);

            processor.getEngine().setActiveSlot(slot);
            processor.getStateInformation(state);
            expect(state.getSize() > 0, "a state");
        }

        beginTest("the state in the project: slots, their plugins' settings and the audible slot come back");
        {
            BenchmarkProcessor restored;
            restored.prepareToPlay(kSampleRate, kBlockSize);
            restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            // the restore runs on the message thread: let it run if this is not it
            const juce::uint32 started = juce::Time::getMillisecondCounter();
            while (restored.getEngine().getNumSlots() < 3 && juce::Time::getMillisecondCounter() - started < static_cast<juce::uint32>(kRestoreWaitMs))
            {
                if (juce::MessageManager* messages = juce::MessageManager::getInstanceWithoutCreating())
                {
                    messages->runDispatchLoopUntil(50);
                }
            }
            expectEquals(restored.getEngine().getNumSlots(), 3, "dry, gain, latency");
            if (restored.getEngine().getNumSlots() == 3)
            {
                expectEquals(restored.getEngine().getActiveSlot(), 1, "the audible slot");
                expectWithinAbsoluteError(restored.getEngine().getPlugin(1)->getParameter(kGainIndex).normalisedValue, kGainNormalisedPlus12Db, 1.0e-4f,
                                          "the gain plugin's setting");
                expectEquals(restored.getLatencySamples(), kLatencyPluginDelay);
                expectWithinAbsoluteError(processSteadyLevel(restored), kInputLevel * kGainPlus12Db, kTolerance * kGainPlus12Db);
            }
        }

        beginTest("the editor: on the page Compare the parameter table fits next to the live curves (its slider column is visible)");
        {
            // the pages read the settings folder: one of the test's own
            const juce::File settingsFolder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pluginlab_benchmark_test_settings");
            settingsFolder.createDirectory();
            setenv("PLUGINLAB_SETTINGS_FOLDER", settingsFolder.getFullPathName().toRawUTF8(), 1);
            BenchmarkProcessor processor;
            processor.prepareToPlay(kSampleRate, kBlockSize);
            const int slot = addSlot(processor, gain.descriptions[0]);
            processor.getEngine().setActiveSlot(slot);
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            editor->setSize(kEditorWidth, kEditorHeight);
            if (auto* tabs = findChild<juce::TabbedComponent>(*editor))
            {
                expectEquals(tabs->getTabNames()[0], juce::String("Easy"), "the musician's page first (W10.4)");
                writePicture(*editor, "pluginlab_benchmark_easy.png");
                tabs->setCurrentTabIndex(kExpertTab);
                writePicture(*editor, "pluginlab_benchmark_expert.png");
                tabs->setCurrentTabIndex(kCompareTab);
            }
            if (auto* compare = findChild<pluginlab::host::ComparePanel>(*editor))
            {
                compare->slotsChanged();
            }
            auto* table = findChild<pluginlab::ui::ParameterTableComponent>(*editor);
            expect(table != nullptr, "a parameter table");
            if (auto* list = table != nullptr ? findChild<juce::TableListBox>(*table) : nullptr)
            {
                const int columns = list->getHeader().getTotalWidth();
                logMessage("parameter table " + juce::String(list->getWidth()) + " px wide, its columns " + juce::String(columns) + " px");
                expectLessOrEqual(columns, list->getWidth());
            }
            const juce::File picture = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pluginlab_benchmark_compare.png");
            juce::FileOutputStream stream(picture);
            if (stream.openedOk())
            {
                stream.setPosition(0);
                stream.truncate();
                juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), stream);
                logMessage("picture: " + picture.getFullPathName());
            }
            editor.reset();
        }

        beginTest("a state that is not ours is ignored");
        {
            BenchmarkProcessor processor;
            processor.prepareToPlay(kSampleRate, kBlockSize);
            const juce::MemoryBlock garbage("not a state", 11);
            processor.setStateInformation(garbage.getData(), static_cast<int>(garbage.getSize()));
            expectEquals(processor.getEngine().getNumSlots(), 1);
        }
    }

private:
    void writePicture(juce::Component& component, const juce::String& fileName)
    {
        const juce::File picture = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(fileName);
        juce::FileOutputStream stream(picture);
        if (stream.openedOk())
        {
            stream.setPosition(0);
            stream.truncate();
            juce::PNGImageFormat().writeImageToStream(component.createComponentSnapshot(component.getLocalBounds()), stream);
            logMessage("picture: " + picture.getFullPathName());
        }
    }

    template <typename Type>
    static Type* findChild(juce::Component& parent)
    {
        for (juce::Component* child : parent.getChildren())
        {
            if (auto* found = dynamic_cast<Type*>(child))
            {
                return found;
            }
            if (auto* deeper = findChild<Type>(*child))
            {
                return deeper;
            }
        }
        return nullptr;
    }

    int addSlot(BenchmarkProcessor& processor, const juce::PluginDescription& description)
    {
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
            pluginlab::hosting::HostedPlugin::load(processor.getFormatManager(), description, kSampleRate, kBlockSize, error);
        expect(plugin != nullptr, error);
        if (plugin == nullptr)
        {
            return -1;
        }
        const int slot = processor.getEngine().addSlot(std::move(plugin), description.name, error);
        processor.slotsChanged();
        return slot;
    }

    // a constant input through many blocks (the alignment and the crossfade pass); the level of the last block's last sample
    float processSteadyLevel(BenchmarkProcessor& processor)
    {
        juce::AudioBuffer<float> buffer(2, kBlockSize);
        juce::MidiBuffer midi;
        for (int block = 0; block < kBlocksToSettle; ++block)
        {
            for (int channel = 0; channel < 2; ++channel)
            {
                juce::FloatVectorOperations::fill(buffer.getWritePointer(channel), kInputLevel, kBlockSize);
            }
            processor.processBlock(buffer, midi);
        }
        return buffer.getSample(0, kBlockSize - 1);
    }
};

static BenchmarkTests benchmarkTests;
