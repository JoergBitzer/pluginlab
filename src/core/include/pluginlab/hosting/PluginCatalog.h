#pragma once

#include <vector>

#include "pluginlab/hosting/PluginScanResult.h"
#include "pluginlab/hosting/PluginValidator.h"

namespace pluginlab::hosting
{
// The result of one pluginval run, as kept in the catalog.
struct CatalogValidation
{
    int level = 0;                                   // pluginval strictness level
    ValidationStatus status = ValidationStatus::NotAvailable;
    juce::String message;
    juce::String validatedAt;                        // date and time of the run
    juce::String pluginStamp;                        // identifies the version of the plugin that was validated (see describePluginFile)
    juce::String pluginModified;                     // modification date of that version, for the user
    juce::String pluginvalStamp;                     // identifies the pluginval that did the run
};

// Everything that is known about one plugin file: the result of the last scan and the validations (one per strictness level).
struct CatalogEntry
{
    juce::File file;
    juce::String stamp;                              // the version of the plugin file at the time of the scan
    juce::String modified;                           // modification date of that version (the newest file inside a bundle)
    juce::String scannedAt;
    ScanStatus scanStatus = ScanStatus::ScannerFailed;
    juce::String message;
    juce::Array<juce::PluginDescription> descriptions;
    std::vector<CatalogValidation> validations;
};

// Identifies the version of a plugin file: path, size and modification time of the file or of every file in a bundle. Another
// stamp than the one of a validation means: the plugin changed after it was validated.
juce::String describePluginFile(const juce::File& pluginFile);

// The date of the newest file of the plugin ("2026-10-04 14:13"), empty if the plugin does not exist.
juce::String getModifiedText(const juce::File& pluginFile);

// The date and time now, in the format of the catalog.
juce::String getNowText();

// The list of the known plugins in one XML file, so that the next start of a host or of the loader shows it at once: scan results and
// validations together. Every function reads the file, changes it and writes it again under a lock, so several programs (the
// loader in a DAW, the host, the validation thread) can use the file; the last writer never throws away what another one wrote.
// An empty File: the catalog lives in memory only.
class PluginCatalog
{
public:
    explicit PluginCatalog(const juce::File& catalogFile);

    static juce::File getDefaultFile();

    std::vector<CatalogEntry> load() const;

    // Adds or replaces the entries of these files (the validations of a file are kept). removeMissing: entries whose file does not
    // exist any more are removed (after a scan of the standard folders).
    void storeScanResults(const std::vector<PluginScanResult>& results, bool removeMissing);

    // Adds or replaces the validation of this level for the plugin file (the entry is created if the file was never scanned).
    void storeValidation(const juce::File& pluginFile, const CatalogValidation& validation);

private:
    std::vector<CatalogEntry> read() const;
    void write(const std::vector<CatalogEntry>& entries) const;

    juce::File m_file;
    mutable std::vector<CatalogEntry> m_memory;
};
}
