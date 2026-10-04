#include "pluginlab/hosting/PluginValidator.h"

#include "pluginlab/hosting/PluginCatalog.h"
#include "pluginlab/hosting/PluginProbe.h"
#include "pluginlab/hosting/PluginScanner.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace pluginlab::hosting
{
namespace
{
const juce::String kEnvironmentVariable = "PLUGINLAB_PLUGINVAL";
const juce::String kPluginvalName = "pluginval";
const juce::String kSuccessLine = "SUCCESS";
const juce::String kTestStartPrefix = "Starting tests in:";
constexpr int kLogTailLength = 2000;
constexpr int kMsPerSecond = 1000;

// the part of the pluginval log that tells what went wrong (the end)
juce::String tailOf(const juce::String& text)
{
    if (text.length() <= kLogTailLength)
    {
        return text;
    }
    return text.substring(text.length() - kLogTailLength);
}

juce::String firstFailureLine(const juce::String& output)
{
    const juce::StringArray lines = juce::StringArray::fromLines(output);
    for (const juce::String& line : lines)
    {
        if (line.containsIgnoreCase("FAILED") || line.containsIgnoreCase("Assertion") || line.contains("***"))
        {
            return line.trim();
        }
    }
    return {};
}

// pluginval prints "Starting tests in: <name>..." before every test; the last one is where it was when it died
juce::String lastStartedTest(const juce::String& output)
{
    const juce::StringArray lines = juce::StringArray::fromLines(output);
    for (int index = lines.size() - 1; index >= 0; --index)
    {
        if (lines[index].startsWith(kTestStartPrefix))
        {
            return lines[index].fromFirstOccurrenceOf(kTestStartPrefix, false, false).trim();
        }
    }
    return {};
}
}

juce::String toString(ValidationStatus status)
{
    switch (status)
    {
        case ValidationStatus::Passed:
            return "Passed";
        case ValidationStatus::Failed:
            return "Failed";
        case ValidationStatus::TimedOut:
            return "TimedOut";
        case ValidationStatus::NotAvailable:
            return "NotAvailable";
    }
    return "Unknown";
}

PluginValidator::PluginValidator(const juce::File& pluginvalExecutable,
                                 const juce::File& catalogFile,
                                 int strictnessLevel,
                                 int timeoutMs,
                                 const juce::File& scannerExecutable)
    : m_pluginvalExecutable(pluginvalExecutable),
      m_scannerExecutable(scannerExecutable),
      m_pluginvalStamp(juce::String((pluginvalExecutable.getFullPathName() + ":" + juce::String(pluginvalExecutable.getSize())).hashCode64())),
      m_catalog(std::make_shared<PluginCatalog>(catalogFile)),
      m_strictnessLevel(strictnessLevel),
      m_timeoutMs(timeoutMs)
{
}

int PluginValidator::getNumberOfPluginvalRuns() const
{
    return m_numberOfRuns;
}

juce::File PluginValidator::findPluginval()
{
    const juce::String fromEnvironment = juce::SystemStats::getEnvironmentVariable(kEnvironmentVariable, {});
    if (fromEnvironment.isNotEmpty() && juce::File(fromEnvironment).existsAsFile())
    {
        return juce::File(fromEnvironment);
    }

    const juce::File executableFolder = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    // (not the extension of the running module: inside a plugin that is .so or .vst3)
#if JUCE_WINDOWS
    const juce::String fileName = kPluginvalName + ".exe";
#else
    const juce::String fileName = kPluginvalName;
#endif
    const juce::File nextToExecutable = executableFolder.getChildFile(fileName);
    if (nextToExecutable.existsAsFile())
    {
        return nextToExecutable;
    }

    const juce::File inPath = juce::File::createFileWithoutCheckingPath(kPluginvalName);
    juce::ChildProcess which;
    if (which.start(juce::StringArray{kPluginvalName, "--version"}, 0) && which.waitForProcessToFinish(kMsPerSecond * 10)
        && which.getExitCode() == 0)
    {
        return inPath; // started: it is in the PATH (the name alone is enough to start it)
    }

    // where tools/run_pluginval.sh and .ps1 download it
    juce::Array<juce::File> downloadLocations;
    const juce::File cacheFolder = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile(".cache").getChildFile(kPluginvalName);
    downloadLocations.add(cacheFolder.getChildFile(kPluginvalName));
    downloadLocations.add(cacheFolder.getChildFile("pluginval.app/Contents/MacOS/pluginval"));
    const juce::String localAppData = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {});
    if (localAppData.isNotEmpty())
    {
        downloadLocations.add(juce::File(localAppData).getChildFile(kPluginvalName).getChildFile("pluginval.exe"));
    }
    for (const juce::File& location : downloadLocations)
    {
        if (location.existsAsFile())
        {
            return location;
        }
    }
    return {};
}

juce::File PluginValidator::getDefaultCacheFile()
{
    return PluginCatalog::getDefaultFile();
}

ValidationResult PluginValidator::runPluginval(const juce::File& pluginFile) const
{
    ValidationResult result;

    juce::StringArray arguments;
#if JUCE_LINUX || JUCE_MAC
    // pluginval keeps settings in the home folder of the user: give it a temporary one, like tools/run_pluginval.sh
    const juce::File temporaryHome = juce::File::createTempFile("pluginlab_home");
    temporaryHome.createDirectory();
    arguments.add("env");
    arguments.add("HOME=" + temporaryHome.getFullPathName());
#endif
    arguments.add(m_pluginvalExecutable.getFullPathName());
    arguments.add("--strictness-level");
    arguments.add(juce::String(m_strictnessLevel));
    arguments.add("--timeout-ms");
    arguments.add(juce::String(m_timeoutMs));
    arguments.add("--validate");
    arguments.add(pluginFile.getFullPathName());

    // A plugin that crashes inside pluginval must not open a Windows error dialog that nobody can click: the child process inherits
    // the error mode of this process at the moment it is created. The old mode is restored right after the start.
#if JUCE_WINDOWS
    const UINT oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
    juce::ChildProcess pluginval;
    const bool started = pluginval.start(arguments);
#if JUCE_WINDOWS
    SetErrorMode(oldErrorMode);
#endif
    if (! started)
    {
        result.status = ValidationStatus::NotAvailable;
        result.message = "pluginval could not be started";
        return result;
    }

    // The output is read while pluginval runs: if it is only read afterwards, a long log fills the pipe, pluginval blocks on writing and
    // never finishes. The read ends when the process ends. Time limits: pluginval's own (--timeout-ms: no output for that long).
    juce::String output;
    constexpr int kReadBufferSize = 4096;
    char buffer[kReadBufferSize];
    int bytesRead = pluginval.readProcessOutput(buffer, kReadBufferSize);
    while (bytesRead > 0)
    {
        output += juce::String::fromUTF8(buffer, bytesRead);
        bytesRead = pluginval.readProcessOutput(buffer, kReadBufferSize);
    }
    if (! pluginval.waitForProcessToFinish(m_timeoutMs))
    {
        pluginval.kill();
        result.status = ValidationStatus::TimedOut;
        result.message = "pluginval did not finish in time";
        return result;
    }

    result.log = tailOf(output);
    // exit code 0 alone is not enough: a child killed by a signal reports 0 on POSIX; pluginval prints SUCCESS at the end
    const bool passed = pluginval.getExitCode() == 0 && output.contains(kSuccessLine);
    if (passed)
    {
        result.status = ValidationStatus::Passed;
        result.message = "Passed pluginval at strictness level " + juce::String(m_strictnessLevel);
        return result;
    }

    result.status = ValidationStatus::Failed;
    result.message = "Failed pluginval";
    const juce::String failureLine = firstFailureLine(output);
    if (failureLine.isNotEmpty())
    {
        result.message += ": " + failureLine;
        return result;
    }
    const juce::String lastTest = lastStartedTest(output);
    if (lastTest.isNotEmpty())
    {
        result.message += ": the plugin crashed or hung during '" + lastTest + "' (strictness level " + juce::String(m_strictnessLevel) + ")";
    }
    return result;
}

bool PluginValidator::getStoredResult(const juce::File& pluginFile, ValidationResult& result) const
{
    return findStored(pluginFile, false, juce::String(), result);
}

bool PluginValidator::getStoredQuickResult(const juce::File& pluginFile, const juce::PluginDescription& description, ValidationResult& result) const
{
    return findStored(pluginFile, true, description.createIdentifierString(), result);
}

bool PluginValidator::findStored(const juce::File& pluginFile, bool quick, const juce::String& pluginId, ValidationResult& result) const
{
    if (! pluginFile.exists())
    {
        return false;
    }
    for (const CatalogEntry& entry : m_catalog->load())
    {
        if (entry.file != pluginFile)
        {
            continue;
        }
        for (const CatalogValidation& stored : entry.validations)
        {
            int level = m_strictnessLevel;
            if (quick)
            {
                level = 0;
            }
            if (stored.level != level || stored.isQuickCheck != quick || stored.pluginId != pluginId)
            {
                continue;
            }
            result.status = stored.status;
            result.message = stored.message;
            result.validatedAt = stored.validatedAt;
            result.pluginModified = stored.pluginModified;
            result.fromCache = true;
            result.isQuickCheck = quick;
            juce::String expectedPluginvalStamp = m_pluginvalStamp;
            if (quick)
            {
                expectedPluginvalStamp.clear();
            }
            const bool sameVersion = stored.pluginStamp == describePluginFile(pluginFile) && stored.pluginvalStamp == expectedPluginvalStamp;
            result.outdated = ! sameVersion;
            return true;
        }
    }
    return false;
}

bool PluginValidator::getCachedResult(const juce::File& pluginFile, ValidationResult& result) const
{
    ValidationResult stored;
    if (! getStoredResult(pluginFile, stored) || stored.outdated)
    {
        return false;
    }
    result = stored;
    return true;
}

ValidationResult PluginValidator::validate(const juce::File& pluginFile)
{
    if (! m_pluginvalExecutable.existsAsFile() && m_pluginvalExecutable.getFullPathName() != kPluginvalName)
    {
        ValidationResult missing;
        missing.status = ValidationStatus::NotAvailable;
        missing.message = "pluginval was not found: the plugin was not validated";
        return missing;
    }
    if (! pluginFile.exists())
    {
        ValidationResult notThere;
        notThere.status = ValidationStatus::Failed;
        notThere.message = "The plugin file does not exist";
        return notThere;
    }

    ValidationResult cached;
    if (getCachedResult(pluginFile, cached))
    {
        return cached;
    }

    // the version of the plugin is noted before the run: if the file changes during the run, the result counts for the old version
    const juce::String stamp = describePluginFile(pluginFile);
    const juce::String modified = getModifiedText(pluginFile);
    ++m_numberOfRuns;
    ValidationResult result = runPluginval(pluginFile);
    result.validatedAt = getNowText();
    result.pluginModified = modified;
    if (result.status != ValidationStatus::NotAvailable)
    {
        CatalogValidation validation;
        validation.level = m_strictnessLevel;
        validation.status = result.status;
        validation.message = result.message;
        validation.validatedAt = result.validatedAt;
        validation.pluginStamp = stamp;
        validation.pluginModified = modified;
        validation.pluginvalStamp = m_pluginvalStamp;
        m_catalog->storeValidation(pluginFile, validation);
    }
    return result;
}

ValidationResult PluginValidator::validateQuick(const juce::File& pluginFile, const juce::PluginDescription& description)
{
    if (! pluginFile.exists())
    {
        ValidationResult notThere;
        notThere.status = ValidationStatus::Failed;
        notThere.message = "The plugin file does not exist";
        return notThere;
    }
    ValidationResult cached;
    if (getStoredQuickResult(pluginFile, description, cached) && ! cached.outdated)
    {
        return cached;
    }

    juce::File scanner = m_scannerExecutable;
    if (scanner == juce::File())
    {
        scanner = PluginScanner::getDefaultScannerExecutable();
    }
    const juce::String stamp = describePluginFile(pluginFile);
    const juce::String modified = getModifiedText(pluginFile);
    ValidationResult result = probePlugin(scanner, pluginFile, description.createIdentifierString(), m_timeoutMs);
    result.isQuickCheck = true;
    result.validatedAt = getNowText();
    result.pluginModified = modified;
    if (result.status != ValidationStatus::NotAvailable)
    {
        CatalogValidation validation;
        validation.isQuickCheck = true;
        validation.pluginId = description.createIdentifierString();
        validation.level = 0;
        validation.status = result.status;
        validation.message = result.message;
        validation.validatedAt = result.validatedAt;
        validation.pluginStamp = stamp;
        validation.pluginModified = modified;
        m_catalog->storeValidation(pluginFile, validation);
    }
    return result;
}
}
