#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "TestPluginPaths.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginProbe.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/hosting/PluginValidator.h"

namespace
{
constexpr int kTimeoutMs = 60000;
constexpr int kLevel = 5;
}

// The quick check of one plugin of a file: for files with many plugins, where pluginval would test all of them.
class ProbeTests : public juce::UnitTest
{
public:
    ProbeTests()
        : juce::UnitTest("PluginProbe", "pluginlab")
    {
    }

    void runTest() override
    {
        juce::AudioPluginFormatManager formatManager;
        pluginlab::hosting::addHeadlessFormats(formatManager);
        const juce::File scanner = testpaths::getScannerExecutable();

        const pluginlab::hosting::PluginScanResult gain =
            pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getGainPlugin());
        const pluginlab::hosting::PluginScanResult crashing =
            pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, testpaths::getCrashInProcessPlugin());
        expect(! gain.descriptions.isEmpty() && ! crashing.descriptions.isEmpty(), "the test plugins must be scannable");
        if (gain.descriptions.isEmpty() || crashing.descriptions.isEmpty())
        {
            return;
        }

        beginTest("a good plugin passes the quick check");
        const pluginlab::hosting::ValidationResult good =
            pluginlab::hosting::probePlugin(scanner, testpaths::getGainPlugin(), gain.descriptions[0].createIdentifierString(), kTimeoutMs);
        expect(good.status == pluginlab::hosting::ValidationStatus::Passed, good.message);

        beginTest("a plugin that crashes while processing fails the quick check and the step is named");
        const pluginlab::hosting::ValidationResult bad = pluginlab::hosting::probePlugin(
            scanner, testpaths::getCrashInProcessPlugin(), crashing.descriptions[0].createIdentifierString(), kTimeoutMs);
        expect(bad.status == pluginlab::hosting::ValidationStatus::Failed, bad.message);
        expect(bad.message.contains("processing audio"), bad.message);

        beginTest("a plugin that is not in the file fails the quick check with a message");
        const pluginlab::hosting::ValidationResult missing =
            pluginlab::hosting::probePlugin(scanner, testpaths::getGainPlugin(), "no such plugin", kTimeoutMs);
        expect(missing.status == pluginlab::hosting::ValidationStatus::Failed, missing.message);
        expect(missing.message.contains("not in the file"), missing.message);

        beginTest("a missing scanner is reported as NotAvailable");
        const pluginlab::hosting::ValidationResult noScanner = pluginlab::hosting::probePlugin(
            testpaths::getTestPluginFolder().getChildFile("no_scanner_here"), testpaths::getGainPlugin(), "x", kTimeoutMs);
        expect(noScanner.status == pluginlab::hosting::ValidationStatus::NotAvailable);

        beginTest("the validator keeps the quick check, per plugin, and does not run it again");
        const juce::TemporaryFile catalogFile(".xml");
        pluginlab::hosting::PluginValidator validator(juce::File(), catalogFile.getFile(), kLevel, kTimeoutMs, scanner);
        const pluginlab::hosting::ValidationResult first = validator.validateQuick(testpaths::getGainPlugin(), gain.descriptions[0]);
        expect(first.status == pluginlab::hosting::ValidationStatus::Passed, first.message);
        expect(first.isQuickCheck);
        expect(first.validatedAt.isNotEmpty());
        expect(! first.fromCache);
        const pluginlab::hosting::ValidationResult second = validator.validateQuick(testpaths::getGainPlugin(), gain.descriptions[0]);
        expect(second.fromCache);
        pluginlab::hosting::ValidationResult stored;
        expect(validator.getStoredQuickResult(testpaths::getGainPlugin(), gain.descriptions[0], stored));
        expect(! validator.getStoredQuickResult(testpaths::getGainPlugin(), crashing.descriptions[0], stored),
               "the result belongs to the plugin that was checked");
    }
};

static ProbeTests probeTests;
