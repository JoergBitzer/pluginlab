#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "TestPluginPaths.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
const juce::String kGainPluginName = "PluginLab Test Gain";
constexpr int kShortTimeoutMs = 60000;
}

class ScannerTests : public juce::UnitTest
{
public:
    ScannerTests()
        : juce::UnitTest("PluginScanner", "pluginlab")
    {
    }

    void runTest() override
    {
        const pluginlab::hosting::PluginScanner scanner(testpaths::getScannerExecutable(), kShortTimeoutMs);

        beginTest("a good plugin is found with its name");
        const pluginlab::hosting::PluginScanResult good = scanner.scanFile(testpaths::getGainPlugin());
        expect(good.status == pluginlab::hosting::ScanStatus::Ok, "status " + pluginlab::hosting::toString(good.status) + ": " + good.message);
        expectEquals(good.descriptions.size(), 1);
        if (good.descriptions.size() == 1)
        {
            expectEquals(good.descriptions[0].name, kGainPluginName);
        }

        if (pluginlab::hosting::isVst2Supported())
        {
            beginTest("the VST2 version of the plugin is found with its name");
            const juce::File vst2File = testpaths::getGainPluginVst2();
            expect(vst2File != juce::File(), "the VST2 test plugin was not built");
            const pluginlab::hosting::PluginScanResult vst2 = scanner.scanFile(vst2File);
            expect(vst2.status == pluginlab::hosting::ScanStatus::Ok, "status " + pluginlab::hosting::toString(vst2.status) + ": " + vst2.message);
            expectEquals(vst2.descriptions.size(), 1);
            if (vst2.descriptions.size() == 1)
            {
                expectEquals(pluginlab::hosting::getDisplayName(vst2.descriptions[0]), kGainPluginName);
                expectEquals(vst2.descriptions[0].pluginFormatName, juce::String("VST"));
            }
        }

        beginTest("a plugin that crashes is reported as Crashed");
        const pluginlab::hosting::PluginScanResult crashed = scanner.scanFile(testpaths::getCrashPlugin());
        expect(crashed.status == pluginlab::hosting::ScanStatus::Crashed, "status " + pluginlab::hosting::toString(crashed.status));
        expect(crashed.descriptions.isEmpty());

        beginTest("a file that is not a plugin is reported as NoPluginInFile");
        const pluginlab::hosting::PluginScanResult notAPlugin = scanner.scanFile(testpaths::getNotAPluginFile());
        expect(notAPlugin.status == pluginlab::hosting::ScanStatus::NoPluginInFile, "status " + pluginlab::hosting::toString(notAPlugin.status));

        beginTest("a missing scanner executable is reported as ScannerFailed");
        const pluginlab::hosting::PluginScanner brokenScanner(testpaths::getTestPluginFolder().getChildFile("no_scanner_here"), kShortTimeoutMs);
        const pluginlab::hosting::PluginScanResult notScanned = brokenScanner.scanFile(testpaths::getGainPlugin());
        expect(notScanned.status == pluginlab::hosting::ScanStatus::ScannerFailed, "status " + pluginlab::hosting::toString(notScanned.status));

        beginTest("the good plugin is still found after the crash (whole folder)");
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const juce::StringArray files = pluginlab::hosting::PluginScanner::findPluginFiles(formatManager, juce::FileSearchPath(testpaths::getTestPluginFolder().getFullPathName()));
        expect(files.size() >= 2, "files found: " + juce::String(files.size()));
        int numberOfGoodPlugins = 0;
        int numberOfCrashes = 0;
        for (const juce::String& path : files)
        {
            const pluginlab::hosting::PluginScanResult result = scanner.scanFile(juce::File(path));
            if (result.status == pluginlab::hosting::ScanStatus::Ok)
            {
                ++numberOfGoodPlugins;
            }
            if (result.status == pluginlab::hosting::ScanStatus::Crashed)
            {
                ++numberOfCrashes;
            }
        }
        int expectedGoodPlugins = 2; // the VST3 gain plugin and the plugin that only crashes while processing (it scans fine)
        if (pluginlab::hosting::isVst2Supported())
        {
            expectedGoodPlugins = 3; // plus the VST2 version of the gain plugin
        }
        expectEquals(numberOfGoodPlugins, expectedGoodPlugins);
        expectEquals(numberOfCrashes, 1); // the VST2 formats must not report parts of the VST3 bundles as plugins
    }
};

static ScannerTests scannerTests;
