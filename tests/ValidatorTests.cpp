#include <juce_core/juce_core.h>

#include "TestPluginPaths.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginValidator.h"

namespace
{
// pluginval is an external program: without it the tests are skipped, unless PLUGINLAB_REQUIRE_PLUGINVAL is set (CI)
const juce::String kRequirePluginvalVariable = "PLUGINLAB_REQUIRE_PLUGINVAL";
constexpr int kStrictnessLevel = 5;
constexpr int kTimeoutMs = 60000;
}

class ValidatorTests : public juce::UnitTest
{
public:
    ValidatorTests()
        : juce::UnitTest("PluginValidator", "pluginlab")
    {
    }

    void runTest() override
    {
        const juce::File pluginval = pluginlab::hosting::PluginValidator::findPluginval();
        if (pluginval == juce::File())
        {
            beginTest("pluginval is available");
            const bool required = juce::SystemStats::getEnvironmentVariable(kRequirePluginvalVariable, {}).isNotEmpty();
            if (required)
            {
                expect(false, "pluginval was not found, but " + kRequirePluginvalVariable + " is set");
            }
            else
            {
                logMessage("pluginval not found: validator tests skipped (set PLUGINLAB_PLUGINVAL to run them)");
            }
            return;
        }

        const juce::TemporaryFile cacheFile(".xml");
        pluginlab::hosting::PluginValidator validator(pluginval, cacheFile.getFile(), kStrictnessLevel, kTimeoutMs);

        beginTest("a good VST3 plugin passes");
        const pluginlab::hosting::ValidationResult good = validator.validate(testpaths::getGainPlugin());
        expect(good.status == pluginlab::hosting::ValidationStatus::Passed, pluginlab::hosting::toString(good.status) + ": " + good.message + "\n" + good.log);
        expect(! good.fromCache);
        expectEquals(validator.getNumberOfPluginvalRuns(), 1);

        beginTest("the result is remembered: no second run of pluginval, also not in a new validator");
        const pluginlab::hosting::ValidationResult again = validator.validate(testpaths::getGainPlugin());
        expect(again.status == pluginlab::hosting::ValidationStatus::Passed);
        expect(again.fromCache);
        expectEquals(validator.getNumberOfPluginvalRuns(), 1);
        pluginlab::hosting::PluginValidator secondValidator(pluginval, cacheFile.getFile(), kStrictnessLevel, kTimeoutMs);
        const pluginlab::hosting::ValidationResult fromDisk = secondValidator.validate(testpaths::getGainPlugin());
        expect(fromDisk.fromCache);
        expectEquals(secondValidator.getNumberOfPluginvalRuns(), 0);

        if (pluginlab::hosting::isVst2Supported())
        {
            beginTest("a good VST2 plugin passes");
            const pluginlab::hosting::ValidationResult vst2 = validator.validate(testpaths::getGainPluginVst2());
            expect(vst2.status == pluginlab::hosting::ValidationStatus::Passed, pluginlab::hosting::toString(vst2.status) + ": " + vst2.message + "\n" + vst2.log);
        }

        beginTest("a plugin that crashes while processing audio fails the validation (the host survives)");
        const pluginlab::hosting::ValidationResult crashing = validator.validate(testpaths::getCrashInProcessPlugin());
        expect(crashing.status == pluginlab::hosting::ValidationStatus::Failed, pluginlab::hosting::toString(crashing.status) + ": " + crashing.message);

        beginTest("a plugin that crashes when it is created fails the validation");
        const pluginlab::hosting::ValidationResult crashAtCreation = validator.validate(testpaths::getCrashPlugin());
        expect(crashAtCreation.status == pluginlab::hosting::ValidationStatus::Failed, pluginlab::hosting::toString(crashAtCreation.status) + ": " + crashAtCreation.message);

        beginTest("a file that is not a plugin fails the validation");
        const pluginlab::hosting::ValidationResult notAPlugin = validator.validate(testpaths::getNotAPluginFile());
        expect(notAPlugin.status == pluginlab::hosting::ValidationStatus::Failed, pluginlab::hosting::toString(notAPlugin.status) + ": " + notAPlugin.message);

        beginTest("a plugin that changed after its validation counts as not validated, with the date of the validation");
        const juce::TemporaryFile copyFolder(".dir");
        expect(copyFolder.getFile().createDirectory());
        const juce::File copy = copyFolder.getFile().getChildFile("PluginLabTestGain.vst3");
        expect(testpaths::getGainPlugin().copyDirectoryTo(copy));
        const pluginlab::hosting::ValidationResult copyResult = validator.validate(copy);
        expect(copyResult.status == pluginlab::hosting::ValidationStatus::Passed, copyResult.message);
        expect(copyResult.validatedAt.isNotEmpty());
        pluginlab::hosting::ValidationResult stored;
        expect(validator.getStoredResult(copy, stored));
        expect(! stored.outdated);
        const juce::Array<juce::File> binaries = copy.findChildFiles(juce::File::findFiles, true, "*.so;*.dll;*.vst3;PluginLabTestGain");
        expect(! binaries.isEmpty());
        if (! binaries.isEmpty())
        {
            expect(binaries[0].appendText("changed"));
            expect(validator.getStoredResult(copy, stored));
            expect(stored.outdated, "the changed plugin must be marked as outdated");
            expect(stored.validatedAt.isNotEmpty());
            pluginlab::hosting::ValidationResult notCurrent;
            expect(! validator.getCachedResult(copy, notCurrent));
        }

        beginTest("a missing pluginval is reported as NotAvailable");
        pluginlab::hosting::PluginValidator withoutPluginval(testpaths::getTestPluginFolder().getChildFile("no_pluginval_here"), juce::File(), kStrictnessLevel, kTimeoutMs);
        const pluginlab::hosting::ValidationResult notValidated = withoutPluginval.validate(testpaths::getGainPlugin());
        expect(notValidated.status == pluginlab::hosting::ValidationStatus::NotAvailable);
    }
};

static ValidatorTests validatorTests;
