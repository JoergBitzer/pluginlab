#include <memory>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "TestPluginPaths.h"
#include "pluginlab/hosting/FormatManager.h"
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
        pluginlab::hosting::addHeadlessFormats(formatManager);

        const pluginlab::hosting::PluginScanResult vst3 =
            pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getGainPlugin());
        expect(vst3.status == pluginlab::hosting::ScanStatus::Ok, "the VST3 test plugin must be scannable: " + vst3.message);
        if (vst3.descriptions.isEmpty())
        {
            return;
        }
        testFormat("VST3", formatManager, vst3.descriptions[0]);

        if (pluginlab::hosting::isVst2Supported())
        {
            const pluginlab::hosting::PluginScanResult vst2 =
                pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getGainPluginVst2());
            expect(vst2.status == pluginlab::hosting::ScanStatus::Ok, "the VST2 test plugin must be scannable: " + vst2.message);
            if (! vst2.descriptions.isEmpty())
            {
                testFormat("VST2", formatManager, vst2.descriptions[0]);
            }
        }

        beginTest("a plugin that does not exist is reported with an error text, not a crash");
        juce::PluginDescription missing = vst3.descriptions[0];
        missing.fileOrIdentifier = testpaths::getTestPluginFolder().getChildFile("DoesNotExist.vst3").getFullPathName();
        juce::String missingError;
        const std::unique_ptr<pluginlab::hosting::HostedPlugin> notLoaded =
            pluginlab::hosting::HostedPlugin::load(formatManager, missing, kSampleRate, kBlockSize, missingError);
        expect(notLoaded == nullptr);
        expect(missingError.isNotEmpty());
    }

private:
    // the same checks for every format
    void testFormat(const juce::String& formatName, juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description)
    {
        beginTest(formatName + ": loading the test plugin gives four parameters with the expected names");
        juce::String error;
        std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin =
            pluginlab::hosting::HostedPlugin::load(formatManager, description, kSampleRate, kBlockSize, error);
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
            if (formatName == "VST3")
            {
                expect(parameters[2].isDiscrete, "Mode: isDiscrete, steps " + juce::String(parameters[2].numSteps));
                expect(parameters[3].isBoolean, "Bypass: isBoolean, steps " + juce::String(parameters[3].numSteps));
            }
            else
            {
                // VST2 has no step information: every parameter is a continuous value between 0 and 1
                expect(! parameters[2].isDiscrete, "Mode of a VST2 plugin must look continuous");
                expect(! parameters[3].isBoolean, "Bypass of a VST2 plugin must look continuous");
            }
        }

        beginTest(formatName + ": setting a parameter changes its value text");
        plugin->setParameterNormalised(kGainIndex, kGainNormalisedPlus12Db);
        const juce::String gainText = plugin->getParameter(kGainIndex).valueText;
        expect(gainText.startsWith("12.0"), "gain text '" + gainText + "'");
        plugin->setParameterNormalised(kModeIndex, kModeNormalisedC);
        const juce::String modeText = plugin->getParameter(kModeIndex).valueText;
        expect(modeText.startsWith("C"), "mode text '" + modeText + "'");

        beginTest(formatName + ": an out-of-range parameter index is ignored");
        plugin->setParameterNormalised(kExpectedParameterCount, 0.5f);
        plugin->setParameterNormalised(-1, 0.5f);
        expectEquals(static_cast<int>(plugin->getParameters().size()), kExpectedParameterCount);
    }
};

static HostedPluginTests hostedPluginTests;
