#include <memory>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "TestPluginPaths.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kExpectedParameterCount = 4;
constexpr int kGainIndex = 0;
constexpr int kModeIndex = 2;
constexpr float kGainNormalisedPlus12Db = 0.75f;
constexpr float kModeNormalisedC = 1.0f;
}

class HostedPluginTests : public juce::UnitTest
{
public:
    HostedPluginTests()
        : juce::UnitTest("HostedPlugin", "pluginlab")
    {
    }

    void runTest() override
    {
        juce::AudioPluginFormatManager formatManager;
        formatManager.addFormat(std::make_unique<juce::VST3PluginFormatHeadless>());

        juce::VST3PluginFormatHeadless format;
        const pluginlab::hosting::PluginScanResult scanned =
            pluginlab::hosting::PluginScanner::scanFileInProcess(format, testpaths::getGainPlugin());
        expect(scanned.status == pluginlab::hosting::ScanStatus::Ok, "the test plugin must be scannable: " + scanned.message);
        if (scanned.descriptions.isEmpty())
        {
            return;
        }

        beginTest("loading the test plugin gives four parameters with the expected names");
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
            pluginlab::hosting::HostedPlugin::load(formatManager, scanned.descriptions[0], kSampleRate, kBlockSize, error);
        expect(plugin != nullptr, "load failed: " + error);
        if (plugin == nullptr)
        {
            return;
        }
        const std::vector<pluginlab::hosting::ParameterInfo> parameters = plugin->getParameters();
        expectEquals(static_cast<int>(parameters.size()), kExpectedParameterCount);
        if (parameters.size() == static_cast<size_t>(kExpectedParameterCount))
        {
            expectEquals(parameters[0].name, juce::String("Gain"));
            expectEquals(parameters[1].name, juce::String("Frequency"));
            expectEquals(parameters[2].name, juce::String("Mode"));
            expectEquals(parameters[3].name, juce::String("Bypass"));
            expect(parameters[2].isDiscrete, "Mode: isDiscrete " + juce::String(static_cast<int>(parameters[2].isDiscrete)) + ", steps " + juce::String(parameters[2].numSteps));
            expect(parameters[3].isBoolean, "Bypass: isBoolean " + juce::String(static_cast<int>(parameters[3].isBoolean)) + ", discrete " + juce::String(static_cast<int>(parameters[3].isDiscrete)) + ", steps " + juce::String(parameters[3].numSteps));
        }

        beginTest("setting a parameter changes its value text");
        plugin->setParameterNormalised(kGainIndex, kGainNormalisedPlus12Db);
        expectEquals(plugin->getParameter(kGainIndex).valueText, juce::String("12.0 dB"));
        plugin->setParameterNormalised(kModeIndex, kModeNormalisedC);
        expectEquals(plugin->getParameter(kModeIndex).valueText, juce::String("C"));

        beginTest("an out-of-range parameter index is ignored");
        plugin->setParameterNormalised(kExpectedParameterCount, 0.5f);
        plugin->setParameterNormalised(-1, 0.5f);
        expectEquals(static_cast<int>(plugin->getParameters().size()), kExpectedParameterCount);

        beginTest("a plugin that does not exist is reported with an error text, not a crash");
        juce::PluginDescription missing = scanned.descriptions[0];
        missing.fileOrIdentifier = testpaths::getTestPluginFolder().getChildFile("DoesNotExist.vst3").getFullPathName();
        juce::String missingError;
        const std::unique_ptr<pluginlab::hosting::HostedPlugin> notLoaded =
            pluginlab::hosting::HostedPlugin::load(formatManager, missing, kSampleRate, kBlockSize, missingError);
        expect(notLoaded == nullptr);
        expect(missingError.isNotEmpty());
    }
};

static HostedPluginTests hostedPluginTests;
