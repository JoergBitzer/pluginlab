#include "HostFingerprint.h"

#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
bool writeFingerprintReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);
    if (scan.descriptions.isEmpty())
    {
        return false;
    }
    juce::String report;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        report += pluginlab::engine::createReport(pluginlab::engine::measureFingerprint(formatManager, description)) + "\n";
    }
    if (report.isEmpty())
    {
        return false; // the identifier is not in the file
    }
    return reportFile.replaceWithText(report);
}
}
