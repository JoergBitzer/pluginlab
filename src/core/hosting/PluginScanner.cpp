#include "pluginlab/hosting/PluginScanner.h"

#include "pluginlab/hosting/PluginScanXml.h"

namespace pluginlab::hosting
{
namespace
{
const juce::String kScannerName = "PluginLabScanner";
constexpr bool kSearchRecursively = true;
constexpr bool kOnlySynchronousPlugins = false;
}

PluginScanner::PluginScanner(const juce::File& scannerExecutable, int timeoutMs)
    : m_scannerExecutable(scannerExecutable), m_timeoutMs(timeoutMs)
{
}

juce::StringArray PluginScanner::findPluginFiles(juce::AudioPluginFormat& format, const juce::FileSearchPath& folders)
{
    return format.searchPathsForPlugins(folders, kSearchRecursively, kOnlySynchronousPlugins);
}

juce::File PluginScanner::getDefaultScannerExecutable()
{
    const juce::File executableFolder = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    return executableFolder.getChildFile(kScannerName).withFileExtension(juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFileExtension());
}

PluginScanResult PluginScanner::scanFile(const juce::File& pluginFile) const
{
    PluginScanResult result;
    result.file = pluginFile;

    // starting a program that does not exist "succeeds" on POSIX (the forked child fails): check first
    if (! m_scannerExecutable.existsAsFile())
    {
        result.status = ScanStatus::ScannerFailed;
        result.message = "The scanner " + m_scannerExecutable.getFullPathName() + " does not exist";
        return result;
    }

    juce::TemporaryFile resultFile(".xml");
    juce::ChildProcess scanner;
    const juce::StringArray arguments{m_scannerExecutable.getFullPathName(), pluginFile.getFullPathName(),
                                      resultFile.getFile().getFullPathName()};
    if (! scanner.start(arguments, 0))
    {
        result.status = ScanStatus::ScannerFailed;
        result.message = "Cannot start " + m_scannerExecutable.getFullPathName();
        return result;
    }

    if (! scanner.waitForProcessToFinish(m_timeoutMs))
    {
        scanner.kill();
        result.status = ScanStatus::TimedOut;
        result.message = "The scanner did not finish in " + juce::String(m_timeoutMs / 1000) + " s";
        return result;
    }

    const bool resultRead = scanner.getExitCode() == 0 && readScanResult(resultFile.getFile(), result);
    if (! resultRead)
    {
        result.status = ScanStatus::Crashed;
        result.message = "The scanner process ended with exit code " + juce::String(static_cast<int>(scanner.getExitCode()))
                       + " without a result";
    }
    result.file = pluginFile;
    return result;
}

PluginScanResult PluginScanner::scanFileInProcess(juce::AudioPluginFormat& format, const juce::File& pluginFile)
{
    PluginScanResult result;
    result.file = pluginFile;

    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found, pluginFile.getFullPathName());
    for (const juce::PluginDescription* description : found)
    {
        result.descriptions.add(*description);
    }

    result.status = ScanStatus::Ok;
    if (result.descriptions.isEmpty())
    {
        result.status = ScanStatus::NoPluginInFile;
        result.message = "No plugin of the format " + format.getName() + " in this file";
    }
    return result;
}
}
