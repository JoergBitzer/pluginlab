#pragma once

#include <memory>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::hosting
{
class PluginCatalog;

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
    juce::String validatedAt;     // date and time of the pluginval run
    juce::String pluginModified;  // modification date of the plugin version that was validated
    bool outdated = false;        // only from getStoredResult: the plugin file is not the one that was validated
    bool isQuickCheck = false;    // the result of the quick check of one plugin, not of pluginval
};

juce::String toString(ValidationStatus status);

// (scannerExecutable: for the quick check; an empty File means the default scanner next to the program)
// Tests a plugin file with pluginval (https://github.com/Tracktion/pluginval) in a child process, so that a plugin that crashes
// during the tests cannot take the host down. Results are remembered in the plugin catalog per plugin file (path, size and modification times of
// all its files), strictness level and pluginval: an unchanged plugin is not validated again, a changed one counts as not validated.
class PluginValidator
{
public:
    static constexpr int kDefaultStrictnessLevel = 5; // pluginval recommends at least 5
    static constexpr int kDefaultTimeoutMs = 120000;

    // pluginvalExecutable: see findPluginval(); catalogFile: the plugin catalog, where the results are kept together with the scan
    // results (PluginCatalog; an empty File: kept in memory only)
    PluginValidator(const juce::File& pluginvalExecutable,
                    const juce::File& catalogFile,
                    int strictnessLevel = kDefaultStrictnessLevel,
                    int timeoutMs = kDefaultTimeoutMs,
                    const juce::File& scannerExecutable = juce::File());

    ValidationResult validate(const juce::File& pluginFile);

    // The quick check of one plugin of a file (see PluginProbe): for files with many plugins, where pluginval (which tests every plugin
    // of the file) takes too long. Result kept like that of pluginval. Needs the scanner executable.
    ValidationResult validateQuick(const juce::File& pluginFile, const juce::PluginDescription& description);
    bool getStoredQuickResult(const juce::File& pluginFile, const juce::PluginDescription& description, ValidationResult& result) const;

    // The remembered result for this plugin file, without running pluginval. Returns false if there is none (not validated yet,
    // or the plugin file changed since).
    bool getCachedResult(const juce::File& pluginFile, ValidationResult& result) const;

    // The remembered result of this level even if the plugin file changed since (outdated is then true: the date of the validated
    // version is in pluginModified). Returns false if this level was never run for the file.
    bool getStoredResult(const juce::File& pluginFile, ValidationResult& result) const;

    // How many times pluginval was started by this object (a cache hit does not start it).
    int getNumberOfPluginvalRuns() const;

    // Looks for pluginval: environment variable PLUGINLAB_PLUGINVAL, next to the running executable, in the PATH, in the
    // download folder of tools/run_pluginval.sh (~/.cache/pluginval). Returns an invalid File if there is none.
    static juce::File findPluginval();

    // The default place of the catalog file (the application data folder of the user).
    static juce::File getDefaultCacheFile();

private:
    ValidationResult runPluginval(const juce::File& pluginFile) const;

    bool findStored(const juce::File& pluginFile, bool quick, const juce::String& pluginId, ValidationResult& result) const;

    juce::File m_pluginvalExecutable;
    juce::File m_scannerExecutable;
    juce::String m_pluginvalStamp;
    std::shared_ptr<PluginCatalog> m_catalog;
    int m_strictnessLevel;
    int m_timeoutMs;
    int m_numberOfRuns = 0;
};
}
