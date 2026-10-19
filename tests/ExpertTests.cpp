#include <cmath>
#include <cstdlib>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ExpertPanel.h"
#include "HostSettings.h"
#include "PageServices.h"
#include "TestPluginPaths.h"
#include "pluginlab/engine/MeasurementEngine.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kWaitMs = 15 * 60 * 1000;      // all units with the linearity, twice (CLI and page), in Debug on a loaded machine
constexpr double kNullBelowDb = -100.0;      // the gain plugin at 0 dB against the dry slot
}

// W10.5: the page Expert (docs/design/W10-plan.md): every unit runs from the page on a test plugin and agrees with its command-line result
// (PluginLabHost --measure); the null test against the dry slot.
class ExpertTests : public juce::UnitTest
{
public:
    ExpertTests()
        : juce::UnitTest("Expert page (W10.5)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("every unit from the page agrees with --measure on the gain plugin; the null test against the dry slot");
        const juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pluginlab_expert_test");
        folder.deleteRecursively();
        folder.createDirectory();
        setenv("PLUGINLAB_SETTINGS_FOLDER", folder.getFullPathName().toRawUTF8(), 1);
        pluginlab::host::setHelperExecutable(juce::File(PLUGINLAB_HOST_EXECUTABLE));

        // the command line
        const juce::File cliReport = folder.getChildFile("cli.md");
        juce::ChildProcess cli;
        expect(cli.start(juce::StringArray{PLUGINLAB_HOST_EXECUTABLE, "--measure", testpaths::getGainPlugin().getFullPathName(), cliReport.getFullPathName()}),
               "--measure starts");
        cli.waitForProcessToFinish(kWaitMs);
        const juce::var cliJson = juce::JSON::parse(juce::File(cliReport.getFullPathName() + ".json").loadFileAsString());
        const juce::Array<juce::var>* plugins = cliJson.getProperty("plugins", {}).getArray();
        expect(plugins != nullptr && plugins->size() == 1, "the --measure result as JSON");
        if (plugins == nullptr || plugins->isEmpty())
        {
            return;
        }
        const pluginlab::engine::MeasurementSummary expected = pluginlab::engine::parseMeasurementJson((*plugins)[0]);

        // the page: the dry slot and the gain plugin at its defaults, every unit (with the linearity), the null test against dry
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        pluginlab::engine::MeasurementEngine engine;
        engine.prepare(kSampleRate, kBlockSize);
        juce::String error;
        engine.addSlot(nullptr, "dry", error);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getGainPlugin());
        expect(! scan.descriptions.isEmpty());
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
            pluginlab::hosting::HostedPlugin::load(formatManager, scan.descriptions[0], kSampleRate, kBlockSize, error);
        const int gain = engine.addSlot(std::move(plugin), "gain", error);
        pluginlab::host::HostSettings settings;
        pluginlab::host::ExpertPanel page(engine, settings);
        page.setSize(1100, 700);
        page.setSlotChosen(0, false);
        page.setSlotChosen(gain, true);
        for (const juce::String& key : pluginlab::engine::getMeasurementUnitKeys())
        {
            page.setUnitChosen(key, true);
        }
        page.setNullMaster(0);
        page.startMeasurement();
        const juce::uint32 started = juce::Time::getMillisecondCounter();
        while (page.isMeasuring() && juce::Time::getMillisecondCounter() - started < static_cast<juce::uint32>(kWaitMs))
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        }
        expect(! page.isMeasuring(), "the measurement ended");
        logMessage("page measurement: " + juce::String((juce::Time::getMillisecondCounter() - started) / 1000) + " s");

        // the gain plugin's column (the null master, the dry slot, is measured too)
        const pluginlab::engine::MeasurementSummary* measured = nullptr;
        for (const pluginlab::engine::MeasurementSummary& summary : page.getResults())
        {
            if (summary.deviceName.endsWith("gain"))
            {
                measured = &summary;
            }
        }
        expect(measured != nullptr, "the gain plugin measured");
        if (measured == nullptr)
        {
            return;
        }
        expectEquals(static_cast<int>(measured->rows.size()), static_cast<int>(expected.rows.size()), "the same rows");
        for (const pluginlab::engine::MeasurementRow& row : expected.rows)
        {
            const pluginlab::engine::MeasurementRow* found = pluginlab::engine::findRow(*measured, row.quantity);
            expect(found != nullptr, "the page has " + row.quantity);
            if (found != nullptr)
            {
                expectEquals(found->text, row.text, row.quantity);
            }
        }

        // the null test: the gain plugin at its default (0 dB) against the dry slot
        juce::String depth;
        for (int row = 0; row < page.getNumResultRows(); ++row)
        {
            if (page.getCellText(row, 0).startsWith("null depth"))
            {
                for (int column = 1; column <= static_cast<int>(page.getResults().size()); ++column)
                {
                    if (page.getResults()[static_cast<size_t>(column - 1)].deviceName.endsWith("gain"))
                    {
                        depth = page.getCellText(row, column);
                    }
                }
            }
        }
        logMessage("null depth of the gain plugin against dry: " + depth);
        expect(depth == "identical" || (depth.isNotEmpty() && depth.getDoubleValue() < kNullBelowDb), "a deep null: " + depth);
        expect(page.getLastReport().loadFileAsString().contains("## Null test against"), "the report has the null test");
        folder.deleteRecursively();
    }
};

static ExpertTests expertTests;
