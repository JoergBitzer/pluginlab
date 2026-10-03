#pragma once

#include "pluginlab/hosting/PluginScanResult.h"

namespace pluginlab::hosting
{
// Scans plugin files. Every file is examined by a separate scanner process (PluginLabScanner), so that a plugin that
// crashes when it is loaded cannot take the host down.
class PluginScanner
{
public:
    static constexpr int kDefaultTimeoutMs = 30000;

    PluginScanner(const juce::File& scannerExecutable, int timeoutMs = kDefaultTimeoutMs);

    // The plugin files (bundles/DLLs) of the format found in the folders, searched recursively.
    static juce::StringArray findPluginFiles(juce::AudioPluginFormat& format, const juce::FileSearchPath& folders);

    // Scans one file in a scanner process. Never throws, never crashes: problems are reported in the status.
    PluginScanResult scanFile(const juce::File& pluginFile) const;

    // The work of the scanner process: examines one file in this process. A crashing plugin crashes this process.
    static PluginScanResult scanFileInProcess(juce::AudioPluginFormat& format, const juce::File& pluginFile);

    // The scanner executable next to the executable of the running program (PluginLabScanner, .exe on Windows).
    static juce::File getDefaultScannerExecutable();

private:
    juce::File m_scannerExecutable;
    int m_timeoutMs;
};
}
