#include "HostFingerprint.h"

#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/engine/PluginMeasurement.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
namespace
{
double getMeasurementRate()
{
    return pluginlab::engine::MeasurementOptions().sampleRate;
}
}

bool writeMeasurementReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    juce::String report;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measurePlugin(
            formatManager, description, pluginlab::engine::getDefaultSetting(formatManager, description, getMeasurementRate()), {});
        report += "# " + description.name + " " + description.version + "\n\n" + pluginlab::engine::createMeasurementReport(summary) + "\n";
    }
    if (report.isEmpty())
    {
        return false;
    }
    return reportFile.replaceWithText(report);
}

bool writeFingerprintReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    if (scan.descriptions.isEmpty())
    {
        return false;
    }
    juce::String warning;
    const pluginlab::engine::FingerprintSettings settings =
        pluginlab::engine::FingerprintSettings::loadOrCreate(pluginlab::engine::FingerprintSettings::getDefaultFile(), warning);
    juce::String report;
    int measured = 0;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        pluginlab::engine::PluginFingerprint fingerprint = pluginlab::engine::measureFingerprint(formatManager, description, settings);
        fingerprint.settingsWarning = warning;
        report += pluginlab::engine::createReport(fingerprint) + "\n";
        if (settings.measurements)
        {
            // the measurement units (W7.11) on the default setting of the plugin
            const pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measurePlugin(
                formatManager, description, pluginlab::engine::getDefaultSetting(formatManager, description, getMeasurementRate()), {});
            report += pluginlab::engine::createMeasurementReport(summary) + "\n";
        }
        // the single results for programs (the Developer page): <report>.json, or <report>.<n>.json for further plugins of the file
        juce::String jsonPath = reportFile.getFullPathName() + ".json";
        if (measured > 0)
        {
            jsonPath = reportFile.getFullPathName() + "." + juce::String(measured) + ".json";
        }
        juce::File(jsonPath).replaceWithText(pluginlab::engine::createSummaryJson(fingerprint));
        ++measured;
    }
    if (report.isEmpty())
    {
        return false; // the identifier is not in the file
    }
    return reportFile.replaceWithText(report);
}
}
