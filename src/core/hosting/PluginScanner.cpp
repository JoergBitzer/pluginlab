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

namespace
{
// Inside a VST3 bundle (and an AU component) are shared libraries that look like VST2 plugins to the VST2 format; they belong to
// the bundle and are not plugins of their own.
bool isInsideOtherPluginBundle(const juce::String& path)
{
    return path.containsIgnoreCase(".vst3" + juce::File::getSeparatorString())
        || path.containsIgnoreCase(".component" + juce::File::getSeparatorString());
}

void addFilesOfFormat(juce::StringArray& files, juce::AudioPluginFormat& format, const juce::FileSearchPath& folders)
{
    const juce::StringArray found = format.searchPathsForPlugins(folders, kSearchRecursively, kOnlySynchronousPlugins);
    for (const juce::String& path : found)
    {
        const bool isVst3 = format.getName() == "VST3";
        if (isVst3 || ! isInsideOtherPluginBundle(path))
        {
            files.addIfNotAlreadyThere(path);
        }
    }
}
}

juce::StringArray PluginScanner::findPluginFiles(juce::AudioPluginFormatManager& formatManager, const juce::FileSearchPath& folders)
{
    juce::StringArray files;
    for (int formatIndex = 0; formatIndex < formatManager.getNumFormats(); ++formatIndex)
    {
        addFilesOfFormat(files, *formatManager.getFormat(formatIndex), folders);
    }
    return files;
}

juce::StringArray PluginScanner::findPluginFilesInStandardFolders(juce::AudioPluginFormatManager& formatManager)
{
    juce::StringArray files;
    for (int formatIndex = 0; formatIndex < formatManager.getNumFormats(); ++formatIndex)
    {
        juce::AudioPluginFormat& format = *formatManager.getFormat(formatIndex);
        addFilesOfFormat(files, format, format.getDefaultLocationsToSearch());
    }
    return files;
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
        // (on POSIX a child that was killed by a signal reports exit code 0)
        result.message = "The scanner process ended without a result (it crashed or was killed; exit code "
                       + juce::String(static_cast<int>(scanner.getExitCode())) + ")";
    }
    result.file = pluginFile;
    return result;
}

PluginScanResult PluginScanner::scanFileInProcess(juce::AudioPluginFormatManager& formatManager, const juce::File& pluginFile)
{
    PluginScanResult result;
    result.file = pluginFile;

    juce::OwnedArray<juce::PluginDescription> found;
    for (int formatIndex = 0; formatIndex < formatManager.getNumFormats(); ++formatIndex)
    {
        juce::AudioPluginFormat& format = *formatManager.getFormat(formatIndex);
        if (format.fileMightContainThisPluginType(pluginFile.getFullPathName()))
        {
            format.findAllTypesForFile(found, pluginFile.getFullPathName());
        }
    }
    for (const juce::PluginDescription* description : found)
    {
        result.descriptions.add(*description);
    }

    result.status = ScanStatus::Ok;
    if (result.descriptions.isEmpty())
    {
        result.status = ScanStatus::NoPluginInFile;
        result.message = "No plugin of a known format in this file";
    }
    return result;
}
}
