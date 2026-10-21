#include <cmath>
#include <cstdlib>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestEnvironment.h"
#include "EasyPanel.h"
#include "HostSettings.h"
#include "PageServices.h"
#include "TestPluginPaths.h"
#include "pluginlab/engine/MeasurementEngine.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kMatchWaitMs = 10 * 60 * 1000;   // two targets of about 15 s each, plus the checks, in Debug on a loaded machine
constexpr int kTestEqGain = 0;                 // the Test EQ: gain, frequency, Q (normalised)
constexpr int kTestEqFrequency = 1;
constexpr int kTestEqQ = 2;
constexpr float kMyGain = 0.625f;              // +6 dB
constexpr float kMyFrequency = 0.45f;
constexpr float kMyQ = 0.15f;
constexpr double kTrimRangeDb = 12.0;
}

// W10.4: the musician's walkthrough on the page Easy (docs/design/W10-plan.md 3.4): my EQ set as it sounds good, two others to compare, "Make
// them the same" (the match in a process of its own, PluginLabHost --match), how close in words, the level sliders, switching, presets. The same
// page is in the application and in the plugin (BenchmarkTests checks that the plugin's editor shows it first).
class EasyTests : public juce::UnitTest
{
public:
    EasyTests()
        : juce::UnitTest("Easy page (W10.4)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("how close in words: the same up to 0.25 dB, audibly close up to 1 dB, different above");
        expectEquals(pluginlab::host::EasyPanel::describeCloseness(0.1), juce::String("the same"));
        expectEquals(pluginlab::host::EasyPanel::describeCloseness(0.6), juce::String("audibly close"));
        expectEquals(pluginlab::host::EasyPanel::describeCloseness(2.0), juce::String("different"));

        beginTest("the walkthrough: my Test EQ, the Reference EQ and the two-band Test EQ made the same, levels set, switching, presets saved");
        // a settings folder of the test's own, with the mappings of the repository (the musician's mappings would be in the user's folder)
        const juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pluginlab_easy_test");
        folder.deleteRecursively();
        folder.createDirectory();
        testenvironment::set("PLUGINLAB_SETTINGS_FOLDER", folder.getFullPathName());
        pluginlab::host::HostSettings settings;
        const juce::File mappings = settings.getMappingFolder();
        mappings.createDirectory();
        for (const juce::File& mapping : juce::File(PLUGINLAB_MAPPING_DIR).findChildFiles(juce::File::findFiles, false, "*.json"))
        {
            mapping.copyFileTo(mappings.getChildFile(mapping.getFileName()));
        }
        pluginlab::host::setHelperExecutable(juce::File(PLUGINLAB_HOST_EXECUTABLE));

        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        pluginlab::engine::MeasurementEngine engine;
        engine.prepare(kSampleRate, kBlockSize);
        juce::String error;
        engine.addSlot(nullptr, "dry", error);
        const int mine = load(formatManager, engine, testpaths::getEqPlugin());
        const int reference = load(formatManager, engine, testpaths::getReferencePlugin("Eq"));
        const int twoBands = load(formatManager, engine, testpaths::getEqTwoBandsPlugin());
        if (mine < 0 || reference < 0 || twoBands < 0)
        {
            return;
        }
        // 1. my EQ, set as it sounds good
        engine.getPlugin(mine)->setParameterNormalised(kTestEqGain, kMyGain);
        engine.getPlugin(mine)->setParameterNormalised(kTestEqFrequency, kMyFrequency);
        engine.getPlugin(mine)->setParameterNormalised(kTestEqQ, kMyQ);
        const float referenceBefore = engine.getPlugin(reference)->getParameter(3).normalisedValue;

        pluginlab::host::EasyPanel page(engine, settings);
        page.setSize(1100, 700);
        expectEquals(page.getMasterSlot(), mine, "the first loaded plugin is mine");
        logMessage("before:\n" + page.describe());
        expect(page.describe().contains("not matched yet"));

        // 3. make them the same
        page.startMatch();
        expect(page.isMatching(), "the match runs");
        const juce::uint32 started = juce::Time::getMillisecondCounter();
        while (page.isMatching() && juce::Time::getMillisecondCounter() - started < static_cast<juce::uint32>(kMatchWaitMs))
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        }
        expect(! page.isMatching(), "the match ended");
        const juce::String result = page.describe();
        logMessage("after " + juce::String((juce::Time::getMillisecondCounter() - started) / 1000) + " s:\n" + result);

        // 4. how close, in words; the levels set by the match; the found values applied
        const juce::StringArray lines = juce::StringArray::fromLines(result);
        expectEquals(lines.size(), 4, "mine, two others, without EQ");
        for (int line = 1; line <= 2 && line < lines.size(); ++line)
        {
            expect(lines[line].contains(": the same") || lines[line].contains(": audibly close"), lines[line]);
        }
        expect(std::abs(engine.getPlugin(reference)->getParameter(3).normalisedValue - referenceBefore) > 1.0e-4f, "the found values are applied (the Reference EQ's gain)");
        for (const int slot : {reference, twoBands})
        {
            expect(std::abs(engine.getSlotTrimDb(slot)) <= kTrimRangeDb, "a level within the slider's range");
        }
        expectEquals(static_cast<double>(engine.getSlotTrimDb(mine)), 0.0, "mine keeps its level");

        // switching: the listen button of the second row is the audible slot
        engine.setActiveSlot(reference);
        expect(page.describe().fromFirstOccurrenceOf("\n", false, false).upToFirstOccurrenceOf("\n", false, false).contains("[listening]"));

        // 5. presets
        page.savePresets();
        const juce::Array<juce::File> presets = settings.getPresetFolder().findChildFiles(juce::File::findFiles, true, "*.state");
        expectEquals(presets.size(), 2, "a preset for each of the two others");
        folder.deleteRecursively();
    }

private:
    int load(juce::AudioPluginFormatManager& formatManager, pluginlab::engine::MeasurementEngine& engine, const juce::File& file)
    {
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, file);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (scan.descriptions.isEmpty())
        {
            return -1;
        }
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
            pluginlab::hosting::HostedPlugin::load(formatManager, scan.descriptions[0], kSampleRate, kBlockSize, error);
        expect(plugin != nullptr, error);
        return plugin != nullptr ? engine.addSlot(std::move(plugin), scan.descriptions[0].name, error) : -1;
    }
};

static EasyTests easyTests;
