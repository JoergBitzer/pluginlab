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

    // The plugin files (bundles/DLLs) of all formats of the manager found in the folders, searched recursively.
    static juce::StringArray findPluginFiles(juce::AudioPluginFormatManager& formatManager, const juce::FileSearchPath& folders);

    // The plugin files of every format in the standard folders of that format.
    static juce::StringArray findPluginFilesInStandardFolders(juce::AudioPluginFormatManager& formatManager);

    // Scans one file in a scanner process. Never throws, never crashes: problems are reported in the status.
    PluginScanResult scanFile(const juce::File& pluginFile) const;

    // The work of the scanner process: examines one file in this process. A crashing plugin crashes this process.
    // The format that can read the file is taken from the manager.
    static PluginScanResult scanFileInProcess(juce::AudioPluginFormatManager& formatManager, const juce::File& pluginFile);

    // The scanner executable: the file named by the environment variable PLUGINLAB_SCANNER if it is set, else PluginLabScanner next to
    // the module that contains this code (the host program, or inside the plugin bundle of the loader).
    static juce::File getDefaultScannerExecutable();

    // "PluginLabScanner", with ".exe" on Windows
    static juce::String getScannerFileName();

private:
    juce::File m_scannerExecutable;
    int m_timeoutMs;
};
}
