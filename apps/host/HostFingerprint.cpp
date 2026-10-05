#include "HostFingerprint.h"

#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
bool writeFingerprintReport(const juce::File& pluginFile, const juce::File& reportFile)
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
        report += pluginlab::engine::createReport(pluginlab::engine::measureFingerprint(formatManager, description)) + "\n";
    }
    return reportFile.replaceWithText(report);
}
}
