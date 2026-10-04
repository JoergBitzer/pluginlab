#include <juce_core/juce_core.h>

#include "TestPluginPaths.h"
#include "pluginlab/hosting/PluginCatalog.h"

namespace
{
constexpr int kLevel = 5;
const juce::String kStamp = "stamp-of-the-validated-version";
}

// The plugin catalog: scan results and validations in one file that survives the program.
class CatalogTests : public juce::UnitTest
{
public:
    CatalogTests()
        : juce::UnitTest("PluginCatalog", "pluginlab")
    {
    }

    void runTest() override
    {
        const juce::TemporaryFile catalogFile(".xml");
        const juce::TemporaryFile pluginFile(".vst3");
        expect(pluginFile.getFile().replaceWithText("first version"));

        beginTest("a scan result is kept in the file and found by another catalog object");
        pluginlab::hosting::PluginScanResult scan;
        scan.file = pluginFile.getFile();
        scan.status = pluginlab::hosting::ScanStatus::Crashed;
        scan.message = "it crashed";
        {
            pluginlab::hosting::PluginCatalog catalog(catalogFile.getFile());
            catalog.storeScanResults({scan}, false);
        }
        const std::vector<pluginlab::hosting::CatalogEntry> entries = pluginlab::hosting::PluginCatalog(catalogFile.getFile()).load();
        expectEquals(static_cast<int>(entries.size()), 1);
        if (entries.size() == 1)
        {
            expect(entries[0].file == pluginFile.getFile());
            expect(entries[0].scanStatus == pluginlab::hosting::ScanStatus::Crashed);
            expectEquals(entries[0].message, juce::String("it crashed"));
            expect(entries[0].scannedAt.isNotEmpty());
            expectEquals(entries[0].stamp, pluginlab::hosting::describePluginFile(pluginFile.getFile()));
        }

        beginTest("a validation is kept per level and survives a new scan of the file");
        pluginlab::hosting::CatalogValidation validation;
        validation.level = kLevel;
        validation.status = pluginlab::hosting::ValidationStatus::Passed;
        validation.message = "fine";
        validation.validatedAt = pluginlab::hosting::getNowText();
        validation.pluginStamp = kStamp;
        validation.pluginModified = pluginlab::hosting::getModifiedText(pluginFile.getFile());
        pluginlab::hosting::PluginCatalog catalog(catalogFile.getFile());
        catalog.storeValidation(pluginFile.getFile(), validation);
        validation.level = 1;
        validation.status = pluginlab::hosting::ValidationStatus::Failed;
        catalog.storeValidation(pluginFile.getFile(), validation);
        catalog.storeScanResults({scan}, false);
        const std::vector<pluginlab::hosting::CatalogEntry> afterScan = catalog.load();
        expectEquals(static_cast<int>(afterScan.size()), 1);
        if (afterScan.size() == 1)
        {
            expectEquals(static_cast<int>(afterScan[0].validations.size()), 2);
        }

        beginTest("a changed plugin file has another stamp and a new modification date");
        const juce::String stampBefore = pluginlab::hosting::describePluginFile(pluginFile.getFile());
        expect(pluginFile.getFile().appendText(" and more"));
        expect(pluginlab::hosting::describePluginFile(pluginFile.getFile()) != stampBefore);

        beginTest("entries of files that are gone are removed after a scan of the standard folders");
        {
            const juce::TemporaryFile vanishing(".vst3");
            expect(vanishing.getFile().replaceWithText("x"));
            pluginlab::hosting::PluginScanResult other;
            other.file = vanishing.getFile();
            other.status = pluginlab::hosting::ScanStatus::NoPluginInFile;
            catalog.storeScanResults({other}, false);
            expectEquals(static_cast<int>(catalog.load().size()), 2);
        }
        catalog.storeScanResults({}, true);
        expectEquals(static_cast<int>(catalog.load().size()), 1);

        beginTest("a catalog without a file lives in memory");
        pluginlab::hosting::PluginCatalog memory{juce::File()};
        memory.storeScanResults({scan}, false);
        expectEquals(static_cast<int>(memory.load().size()), 1);
    }
};

static CatalogTests catalogTests;
