#include "HostReport.h"

#include <memory>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
const juce::String kNewLine = "\n";

void appendLine(juce::String& report, const juce::String& line)
{
    report += line + kNewLine;
}

void appendLoadedPlugin(juce::String& report, juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description)
{
    juce::String error;
    const std::unique_ptr<hosting::HostedPlugin> plugin =
        hosting::HostedPlugin::load(formatManager, description, kSampleRate, kBlockSize, error);
    if (plugin == nullptr)
    {
        appendLine(report, "LOADERROR " + hosting::getDisplayName(description) + " | " + error);
        return;
    }

    const std::vector<hosting::ParameterInfo> parameters = plugin->getParameters();
    appendLine(report, "PLUGIN " + hosting::getDisplayName(description) + " (" + description.pluginFormatName + ") | parameters " + juce::String(static_cast<int>(parameters.size())));
    for (const hosting::ParameterInfo& parameter : parameters)
    {
        appendLine(report, "PARAMETER " + juce::String(parameter.index) + " " + parameter.name + " = " + parameter.valueText);
    }
}
}

bool writeReport(const juce::File& pluginFolder, const juce::File& reportFile)
{
    juce::String report;
    appendLine(report, "FOLDER " + pluginFolder.getFullPathName());

    juce::AudioPluginFormatManager formatManager;
    hosting::addHeadlessFormats(formatManager);

    const hosting::PluginScanner scanner(hosting::PluginScanner::getDefaultScannerExecutable());
    const juce::StringArray files =
        hosting::PluginScanner::findPluginFiles(formatManager, juce::FileSearchPath(pluginFolder.getFullPathName()));
    for (const juce::String& path : files)
    {
        const hosting::PluginScanResult result = scanner.scanFile(juce::File(path));
        appendLine(report, "FILE " + result.file.getFileName() + " | " + hosting::toString(result.status));
        if (result.message.isNotEmpty())
        {
            appendLine(report, "  " + result.message);
        }
        for (const juce::PluginDescription& description : result.descriptions)
        {
            appendLoadedPlugin(report, formatManager, description);
        }
    }
    return reportFile.replaceWithText(report);
}
}
