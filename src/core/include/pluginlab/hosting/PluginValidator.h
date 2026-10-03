#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::hosting
{
enum class ValidationStatus
{
    Passed,       // pluginval ran and reported success
    Failed,       // pluginval reported a failure, or died (the plugin crashed during the tests)
    TimedOut,     // pluginval did not finish in time and was killed
    NotAvailable  // pluginval could not be found or started: the plugin was not validated
};

struct ValidationResult
{
    ValidationStatus status = ValidationStatus::NotAvailable;
    juce::String message;     // one line for the user
    juce::String log;         // the end of the output of pluginval
    bool fromCache = false;   // true: an earlier result for the same, unchanged plugin file
};

juce::String toString(ValidationStatus status);

// Tests a plugin file with pluginval (https://github.com/Tracktion/pluginval) in a child process, so that a plugin that crashes
// during the tests cannot take the host down. Results are remembered per plugin file (path, size and modification times of all
// its files), strictness level and pluginval version: an unchanged plugin is not validated again.
class PluginValidator
{
public:
    static constexpr int kDefaultStrictnessLevel = 5; // pluginval recommends at least 5
    static constexpr int kDefaultTimeoutMs = 120000;

    // pluginvalExecutable: see findPluginval(); cacheFile: where the results are kept (an empty File: no cache on disk)
    PluginValidator(const juce::File& pluginvalExecutable,
                    const juce::File& cacheFile,
                    int strictnessLevel = kDefaultStrictnessLevel,
                    int timeoutMs = kDefaultTimeoutMs);

    ValidationResult validate(const juce::File& pluginFile);

    // How many times pluginval was started by this object (a cache hit does not start it).
    int getNumberOfPluginvalRuns() const;

    // Looks for pluginval: environment variable PLUGINLAB_PLUGINVAL, next to the running executable, in the PATH, in the
    // download folder of tools/run_pluginval.sh (~/.cache/pluginval). Returns an invalid File if there is none.
    static juce::File findPluginval();

    // The default place of the cache file (the application data folder of the user).
    static juce::File getDefaultCacheFile();

private:
    juce::String makeCacheKey(const juce::File& pluginFile) const;
    ValidationResult runPluginval(const juce::File& pluginFile) const;
    void loadCache();
    void saveCache() const;

    juce::File m_pluginvalExecutable;
    juce::File m_cacheFile;
    int m_strictnessLevel;
    int m_timeoutMs;
    int m_numberOfRuns = 0;

    struct CacheEntry
    {
        ValidationStatus status = ValidationStatus::NotAvailable;
        juce::String message;
    };
    std::map<juce::String, CacheEntry> m_cache;
};
}
