#include "pluginlab/hosting/PluginValidator.h"

#include <map>

namespace pluginlab::hosting
{
namespace
{
const juce::String kEnvironmentVariable = "PLUGINLAB_PLUGINVAL";
const juce::String kPluginvalName = "pluginval";
const juce::String kSuccessLine = "SUCCESS";
const juce::String kCacheRootTag = "ValidationCache";
const juce::String kCacheEntryTag = "Entry";
const juce::String kKeyAttribute = "key";
const juce::String kStatusAttribute = "status";
const juce::String kMessageAttribute = "message";
constexpr int kLogTailLength = 2000;
constexpr int kMsPerSecond = 1000;

juce::String statusToText(ValidationStatus status)
{
    return toString(status);
}

ValidationStatus statusFromText(const juce::String& text)
{
    if (text == toString(ValidationStatus::Passed))
    {
        return ValidationStatus::Passed;
    }
    if (text == toString(ValidationStatus::TimedOut))
    {
        return ValidationStatus::TimedOut;
    }
    return ValidationStatus::Failed;
}

// path, size and modification time of the file, or of every file inside a bundle folder
juce::String describeFiles(const juce::File& pluginFile)
{
    juce::String description = pluginFile.getFullPathName();
    juce::Array<juce::File> files;
    if (pluginFile.isDirectory())
    {
        files = pluginFile.findChildFiles(juce::File::findFiles, true);
    }
    else
    {
        files.add(pluginFile);
    }
    for (const juce::File& file : files)
    {
        description += "|" + file.getRelativePathFrom(pluginFile) + ":" + juce::String(file.getSize()) + ":"
                     + juce::String(file.getLastModificationTime().toMilliseconds());
    }
    return description;
}

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

PluginValidator::PluginValidator(const juce::File& pluginvalExecutable, const juce::File& cacheFile, int strictnessLevel, int timeoutMs)
    : m_pluginvalExecutable(pluginvalExecutable), m_cacheFile(cacheFile), m_strictnessLevel(strictnessLevel), m_timeoutMs(timeoutMs)
{
    loadCache();
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
    const juce::String fileName = kPluginvalName + juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFileExtension();
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
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("pluginlab")
        .getChildFile("validation_cache.xml");
}

juce::String PluginValidator::makeCacheKey(const juce::File& pluginFile) const
{
    const juce::String description = describeFiles(pluginFile) + "#level" + juce::String(m_strictnessLevel) + "#"
                                   + m_pluginvalExecutable.getFullPathName() + ":" + juce::String(m_pluginvalExecutable.getSize());
    return juce::String(description.hashCode64());
}

void PluginValidator::loadCache()
{
    m_cache.clear();
    if (m_cacheFile == juce::File() || ! m_cacheFile.existsAsFile())
    {
        return;
    }
    const std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse(m_cacheFile);
    if (root == nullptr || ! root->hasTagName(kCacheRootTag))
    {
        return;
    }
    for (const juce::XmlElement* entry : root->getChildWithTagNameIterator(kCacheEntryTag))
    {
        CacheEntry cached;
        cached.status = statusFromText(entry->getStringAttribute(kStatusAttribute));
        cached.message = entry->getStringAttribute(kMessageAttribute);
        m_cache[entry->getStringAttribute(kKeyAttribute)] = cached;
    }
}

void PluginValidator::saveCache() const
{
    if (m_cacheFile == juce::File())
    {
        return;
    }
    juce::XmlElement root(kCacheRootTag);
    for (const auto& [key, cached] : m_cache)
    {
        juce::XmlElement* entry = root.createNewChildElement(kCacheEntryTag);
        entry->setAttribute(kKeyAttribute, key);
        entry->setAttribute(kStatusAttribute, statusToText(cached.status));
        entry->setAttribute(kMessageAttribute, cached.message);
    }
    m_cacheFile.getParentDirectory().createDirectory();
    root.writeTo(m_cacheFile);
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

    juce::ChildProcess pluginval;
    if (! pluginval.start(arguments))
    {
        result.status = ValidationStatus::NotAvailable;
        result.message = "pluginval could not be started";
        return result;
    }

    // pluginval has its own timeout between test outputs; this is the limit for the whole run
    if (! pluginval.waitForProcessToFinish(m_timeoutMs * 2))
    {
        pluginval.kill();
        result.status = ValidationStatus::TimedOut;
        result.message = "pluginval did not finish in time";
        return result;
    }

    const juce::String output = pluginval.readAllProcessOutput();
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
    }
    return result;
}

bool PluginValidator::getCachedResult(const juce::File& pluginFile, ValidationResult& result) const
{
    if (! pluginFile.exists())
    {
        return false;
    }
    const auto cached = m_cache.find(makeCacheKey(pluginFile));
    if (cached == m_cache.end())
    {
        return false;
    }
    result.status = cached->second.status;
    result.message = cached->second.message;
    result.fromCache = true;
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

    const juce::String key = makeCacheKey(pluginFile);
    const auto cached = m_cache.find(key);
    if (cached != m_cache.end())
    {
        ValidationResult fromCache;
        fromCache.status = cached->second.status;
        fromCache.message = cached->second.message;
        fromCache.fromCache = true;
        return fromCache;
    }

    ++m_numberOfRuns;
    ValidationResult result = runPluginval(pluginFile);
    if (result.status != ValidationStatus::NotAvailable)
    {
        CacheEntry entry;
        entry.status = result.status;
        entry.message = result.message;
        m_cache[key] = entry;
        saveCache();
    }
    return result;
}
}
