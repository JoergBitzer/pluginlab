#include <cstdlib>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestEnvironment.h"
#include "BenchmarkProcessor.h"
#include "ExpertPanel.h"
#include "HostSettings.h"
#include "MeasurementPresets.h"
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
constexpr int kWaitMs = 10 * 60 * 1000;
}

// W10.6: measurement presets (docs/design/W10-plan.md 3.5): JSON in the settings folder, the shipped teaching presets, and a preset saved in
// the application that loads in the plugin and gives the same results.
class PresetTests : public juce::UnitTest
{
public:
    PresetTests()
        : juce::UnitTest("Measurement presets (W10.6)", "pluginlab")
    {
    }

    void runTest() override
    {
        const juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pluginlab_preset_test");
        folder.deleteRecursively();
        folder.createDirectory();
        testenvironment::set("PLUGINLAB_SETTINGS_FOLDER", folder.getFullPathName());
        pluginlab::host::HostSettings settings;

        beginTest("the shipped teaching presets are written when missing; a preset survives its JSON");
        {
            const std::vector<pluginlab::host::MeasurementPreset> presets = pluginlab::host::loadMeasurementPresets(settings);
            expectEquals(static_cast<int>(presets.size()), static_cast<int>(pluginlab::host::getShippedMeasurementPresets().size()));
            pluginlab::host::MeasurementPreset preset;
            preset.name = "Round trip";
            preset.note = "a note";
            preset.units = {"response", "noise"};
            preset.includeDry = true;
            preset.nullMaster = "dry";
            const std::optional<pluginlab::host::MeasurementPreset> back = pluginlab::host::MeasurementPreset::fromJson(juce::JSON::parse(juce::JSON::toString(preset.toJson())));
            expect(back.has_value());
            if (back)
            {
                expectEquals(back->name, preset.name);
                expect(back->units == preset.units);
                expect(back->includeDry && back->nullMaster == "dry");
            }
            expect(! pluginlab::host::MeasurementPreset::fromJson(juce::JSON::parse("{\"name\": \"no format\"}")).has_value(), "not a preset");
        }

        beginTest("a preset saved in the application loads in the plugin and gives the same results");
        pluginlab::host::setHelperExecutable(juce::File(PLUGINLAB_HOST_EXECUTABLE));
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getEqPlugin());
        expect(! scan.descriptions.isEmpty());
        if (scan.descriptions.isEmpty())
        {
            return;
        }
        // the application: its engine with two Test EQs, the second a little different
        pluginlab::engine::MeasurementEngine application;
        application.prepare(kSampleRate, kBlockSize);
        juce::String error;
        application.addSlot(nullptr, "dry", error);
        load(formatManager, application, scan.descriptions[0], 0.6f);
        load(formatManager, application, scan.descriptions[0], 0.65f);
        std::vector<pluginlab::engine::MeasurementSummary> inApplication;
        {
            pluginlab::host::ExpertPanel page(application, settings);
            page.setSize(1100, 700);
            for (const juce::String& key : pluginlab::engine::getMeasurementUnitKeys())
            {
                page.setUnitChosen(key, key == "response" || key == "gain");
            }
            page.setNullMaster(1);
            expect(pluginlab::host::saveMeasurementPreset(settings, page.makePreset("Two EQs, response", "for the test")));
            inApplication = measure(page);
        }

        // the plugin: its processor's engine with the same two plugins, a page of its own, the preset chosen by name
        BenchmarkProcessor processor;
        pluginlab::host::setHelperExecutable(juce::File(PLUGINLAB_HOST_EXECUTABLE)); // the processor set the PluginLabHost of its bundle
        processor.prepareToPlay(kSampleRate, kBlockSize);
        load(formatManager, processor.getEngine(), scan.descriptions[0], 0.6f);
        load(formatManager, processor.getEngine(), scan.descriptions[0], 0.65f);
        pluginlab::host::ExpertPanel pluginPage(processor.getEngine(), settings);
        pluginPage.setSize(1100, 700);
        expect(pluginPage.selectPreset("Two EQs, response"), "the preset is found in the plugin");
        expect(pluginPage.getChosenUnits() == juce::StringArray({"gain", "response"}), pluginPage.getChosenUnits().joinIntoString(", "));
        expectEquals(pluginPage.getNullMaster(), 1, "the first plugin is the master");
        const std::vector<pluginlab::engine::MeasurementSummary> inPlugin = measure(pluginPage);

        expectEquals(static_cast<int>(inPlugin.size()), static_cast<int>(inApplication.size()), "the same slots");
        for (size_t slot = 0; slot < inPlugin.size() && slot < inApplication.size(); ++slot)
        {
            expectEquals(static_cast<int>(inPlugin[slot].rows.size()), static_cast<int>(inApplication[slot].rows.size()));
            for (size_t row = 0; row < inPlugin[slot].rows.size() && row < inApplication[slot].rows.size(); ++row)
            {
                expectEquals(inPlugin[slot].rows[row].text, inApplication[slot].rows[row].text, inPlugin[slot].rows[row].quantity);
            }
        }
        folder.deleteRecursively();
    }

private:
    void load(juce::AudioPluginFormatManager& formatManager, pluginlab::engine::MeasurementEngine& engine, const juce::PluginDescription& description, float gain)
    {
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin = pluginlab::hosting::HostedPlugin::load(formatManager, description, kSampleRate, kBlockSize, error);
        expect(plugin != nullptr, error);
        if (plugin != nullptr)
        {
            plugin->setParameterNormalised(0, gain);
            engine.addSlot(std::move(plugin), description.name, error);
        }
    }

    std::vector<pluginlab::engine::MeasurementSummary> measure(pluginlab::host::ExpertPanel& page)
    {
        page.startMeasurement();
        const juce::uint32 started = juce::Time::getMillisecondCounter();
        while (page.isMeasuring() && juce::Time::getMillisecondCounter() - started < static_cast<juce::uint32>(kWaitMs))
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        }
        expect(! page.isMeasuring(), "the measurement ended");
        return page.getResults();
    }
};

static PresetTests presetTests;
