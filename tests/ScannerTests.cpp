#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "TestPluginPaths.h"
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
        juce::VST3PluginFormatHeadless format;
        const juce::StringArray files = pluginlab::hosting::PluginScanner::findPluginFiles(format, juce::FileSearchPath(testpaths::getTestPluginFolder().getFullPathName()));
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
        expectEquals(numberOfGoodPlugins, 1);
        expectEquals(numberOfCrashes, 1);
    }
};

static ScannerTests scannerTests;
