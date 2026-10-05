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

    // A file that exists while the plugins of the last session are being loaded: if it is still there at the next start, loading them
    // did not finish (a plugin crashed the program).
    juce::File getRestoreMarker() const;

private:
    juce::File getFolder() const;
    juce::String read(const juce::String& key) const;
    void write(const juce::String& key, const juce::String& value);
};
}
