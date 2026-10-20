#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::host
{
// What the host remembers between two starts (a small settings file in the application data folder) and the files of the last session.
class HostSettings
{
public:
    HostSettings();

    // The folder the file dialog for audio files opens in: the last one used
    juce::File getLastAudioDirectory() const;
    void setLastAudioDirectory(const juce::File& folder);

    // The folder of the saved audio lists and plugin sets (the file dialogs open there; the last used one is remembered)
    juce::File getListFolder() const;
    void setListFolder(const juce::File& folder);

    // The state of the last session, written whenever the lists change: offered at the next start
    juce::File getLastSessionAudioList() const;
    juce::File getLastSessionPluginSet() const;

    // Where the fingerprint reports of the Developer page are kept
    juce::File getFingerprintFolder() const;

    // Where the mappings of plugins that are not our own are kept (W8.5; our own plugins' mappings are in the repository, mappings/)
    juce::File getMappingFolder() const;

    // Where the Matcher page keeps its jobs and reports (W9.6), and the presets it saves (one folder per plugin)
    juce::File getMatcherFolder() const;
    juce::File getPresetFolder() const;

    // The measurement presets (W10.6, JSON, shared by the application and the plugin) and the jobs and reports of the page Expert
    juce::File getMeasurementPresetFolder() const;
    juce::File getExpertFolder() const;

    // The name of the person who last confirmed a mapping on the Developer page (offered at the next confirmation)
    juce::String getMappingConfirmer() const;
    void setMappingConfirmer(const juce::String& name);

    // A file that exists while the plugins of the last session are being loaded: if it is still there at the next start, loading them
    // did not finish (a plugin crashed the program).
    juce::File getRestoreMarker() const;

private:
    juce::File getFolder() const;
    juce::String read(const juce::String& key) const;
    void write(const juce::String& key, const juce::String& value);
};
}
