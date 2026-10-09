#include "HostSettings.h"

namespace pluginlab::host
{
namespace
{
const juce::String kFolderName = "pluginlab";
const juce::String kSettingsFileName = "host_settings.xml";
const juce::String kRootTag = "HostSettings";
const juce::String kAudioDirectoryKey = "lastAudioDirectory";
const juce::String kListFolderKey = "listFolder";
const juce::String kListsFolderName = "lists";
const juce::String kLastAudioListName = "last_session.audiolist";
const juce::String kLastPluginSetName = "last_session.pluginset";
const juce::String kFingerprintFolderName = "fingerprints";
const juce::String kRestoreMarkerName = "restoring_plugins.marker";
}

HostSettings::HostSettings() = default;

juce::File HostSettings::getFolder() const
{
    // tests (and test runs of the host) give a folder of their own, so that they never touch the user's settings and reports
    const juce::String overridden = juce::SystemStats::getEnvironmentVariable("PLUGINLAB_SETTINGS_FOLDER", {});
    if (overridden.isNotEmpty())
    {
        return juce::File(overridden);
    }
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(kFolderName);
}

juce::String HostSettings::read(const juce::String& key) const
{
    const std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse(getFolder().getChildFile(kSettingsFileName));
    if (root == nullptr || ! root->hasTagName(kRootTag))
    {
        return {};
    }
    return root->getStringAttribute(key);
}

void HostSettings::write(const juce::String& key, const juce::String& value)
{
    const juce::File file = getFolder().getChildFile(kSettingsFileName);
    std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse(file);
    if (root == nullptr || ! root->hasTagName(kRootTag))
    {
        root = std::make_unique<juce::XmlElement>(kRootTag);
    }
    root->setAttribute(key, value);
    file.getParentDirectory().createDirectory();
    root->writeTo(file);
}

juce::File HostSettings::getLastAudioDirectory() const
{
    const juce::String path = read(kAudioDirectoryKey);
    if (path.isNotEmpty() && juce::File(path).isDirectory())
    {
        return juce::File(path);
    }
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory);
}

void HostSettings::setLastAudioDirectory(const juce::File& folder)
{
    write(kAudioDirectoryKey, folder.getFullPathName());
}

juce::File HostSettings::getListFolder() const
{
    const juce::String path = read(kListFolderKey);
    if (path.isNotEmpty() && juce::File(path).isDirectory())
    {
        return juce::File(path);
    }
    const juce::File folder = getFolder().getChildFile(kListsFolderName);
    folder.createDirectory();
    return folder;
}

void HostSettings::setListFolder(const juce::File& folder)
{
    write(kListFolderKey, folder.getFullPathName());
}

juce::File HostSettings::getLastSessionAudioList() const
{
    return getFolder().getChildFile(kLastAudioListName);
}

juce::File HostSettings::getLastSessionPluginSet() const
{
    return getFolder().getChildFile(kLastPluginSetName);
}

juce::File HostSettings::getFingerprintFolder() const
{
    return getFolder().getChildFile(kFingerprintFolderName);
}

juce::File HostSettings::getRestoreMarker() const
{
    return getFolder().getChildFile(kRestoreMarkerName);
}
}
