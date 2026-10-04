#include "pluginlab/engine/SessionFiles.h"

#include "pluginlab/hosting/PluginDisplayName.h"

namespace pluginlab::engine
{
namespace
{
const juce::String kAudioListTag = "PluginLabAudioList";
const juce::String kFileTag = "File";
const juce::String kPathAttribute = "path";
const juce::String kPassesAttribute = "passes";
const juce::String kRegionStartAttribute = "regionStartSeconds";
const juce::String kRegionEndAttribute = "regionEndSeconds";
const juce::String kPluginSetTag = "PluginLabPluginSet";
const juce::String kSlotTag = "Slot";
const juce::String kStateTag = "State";
const juce::String kNameAttribute = "name";
const juce::String kActiveAttribute = "activeSlot";
constexpr int kDefaultPasses = 1;
constexpr int kBlockSize = 512;

std::unique_ptr<juce::XmlElement> readRoot(const juce::File& file, const juce::String& tag)
{
    std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse(file);
    if (root == nullptr || ! root->hasTagName(tag))
    {
        return nullptr;
    }
    return root;
}

bool writeRoot(const juce::XmlElement& root, const juce::File& file)
{
    file.getParentDirectory().createDirectory();
    const juce::TemporaryFile temporary(file);
    if (! root.writeTo(temporary.getFile()))
    {
        return false;
    }
    return temporary.overwriteTargetFileWithTemporary();
}
}

bool saveAudioList(const MeasurementEngine& engine, const juce::File& file)
{
    juce::XmlElement root(kAudioListTag);
    for (int index = 0; index < engine.getNumFiles(); ++index)
    {
        const FileInfo info = engine.getFileInfo(index);
        juce::XmlElement* element = root.createNewChildElement(kFileTag);
        element->setAttribute(kPathAttribute, info.file.getFullPathName());
        element->setAttribute(kPassesAttribute, info.passes);
        element->setAttribute(kRegionStartAttribute, info.regionStartSeconds);
        element->setAttribute(kRegionEndAttribute, info.regionEndSeconds);
    }
    return writeRoot(root, file);
}

bool loadAudioList(MeasurementEngine& engine, const juce::File& file, juce::String& report)
{
    const std::unique_ptr<juce::XmlElement> root = readRoot(file, kAudioListTag);
    if (root == nullptr)
    {
        report = file.getFullPathName() + " is not an audio list";
        return false;
    }
    engine.clearFiles();
    for (const juce::XmlElement* element : root->getChildWithTagNameIterator(kFileTag))
    {
        const juce::File audioFile(element->getStringAttribute(kPathAttribute));
        juce::String error;
        if (! engine.addFile(audioFile, element->getIntAttribute(kPassesAttribute, kDefaultPasses), error))
        {
            report += error + "\n";
            continue;
        }
        const int index = engine.getNumFiles() - 1;
        engine.setFileRegionSeconds(index, element->getDoubleAttribute(kRegionStartAttribute), element->getDoubleAttribute(kRegionEndAttribute));
    }
    return true;
}

int countFilesInAudioList(const juce::File& file)
{
    const std::unique_ptr<juce::XmlElement> root = readRoot(file, kAudioListTag);
    if (root == nullptr)
    {
        return -1;
    }
    return root->getNumChildElements();
}

bool savePluginSet(MeasurementEngine& engine, const juce::File& file)
{
    juce::XmlElement root(kPluginSetTag);
    root.setAttribute(kActiveAttribute, engine.getActiveSlot());
    for (int index = 0; index < engine.getNumSlots(); ++index)
    {
        hosting::HostedPlugin* plugin = engine.getPlugin(index);
        if (plugin == nullptr)
        {
            continue; // the dry slot
        }
        juce::XmlElement* slot = root.createNewChildElement(kSlotTag);
        slot->setAttribute(kNameAttribute, engine.getSlotInfo(index).name);
        slot->addChildElement(plugin->getDescription().createXml().release());
        juce::MemoryBlock state;
        plugin->getInstance().getStateInformation(state);
        slot->createNewChildElement(kStateTag)->addTextElement(state.toBase64Encoding());
    }
    return writeRoot(root, file);
}

bool loadPluginSet(MeasurementEngine& engine, juce::AudioPluginFormatManager& formatManager, const juce::File& file, juce::String& report)
{
    const std::unique_ptr<juce::XmlElement> root = readRoot(file, kPluginSetTag);
    if (root == nullptr)
    {
        report = file.getFullPathName() + " is not a plugin set";
        return false;
    }

    for (int index = engine.getNumSlots() - 1; index >= 0; --index)
    {
        if (engine.getPlugin(index) != nullptr)
        {
            engine.removeSlot(index);
        }
    }

    for (const juce::XmlElement* slot : root->getChildWithTagNameIterator(kSlotTag))
    {
        juce::PluginDescription description;
        juce::MemoryBlock state;
        const juce::XmlElement* stateElement = slot->getChildByName(kStateTag);
        bool hasDescription = false;
        for (const juce::XmlElement* child : slot->getChildIterator())
        {
            if (description.loadFromXml(*child))
            {
                hasDescription = true;
            }
        }
        if (! hasDescription)
        {
            report += "A slot without a plugin description was skipped\n";
            continue;
        }
        if (stateElement != nullptr)
        {
            state.fromBase64Encoding(stateElement->getAllSubText());
        }

        juce::String error;
        std::unique_ptr<hosting::HostedPlugin> plugin =
            hosting::HostedPlugin::load(formatManager, description, engine.getSampleRate(), kBlockSize, error);
        if (plugin == nullptr)
        {
            report += hosting::getDisplayName(description) + ": " + error + "\n";
            continue;
        }
        if (state.getSize() > 0)
        {
            plugin->getInstance().setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        }
        const juce::String name = slot->getStringAttribute(kNameAttribute, hosting::getDisplayName(description));
        if (engine.addSlot(std::move(plugin), name, error) < 0)
        {
            report += name + ": " + error + "\n";
        }
    }

    const int active = root->getIntAttribute(kActiveAttribute);
    if (active > 0 && active < engine.getNumSlots())
    {
        engine.setActiveSlot(active);
    }
    return true;
}

int countPluginsInPluginSet(const juce::File& file)
{
    const std::unique_ptr<juce::XmlElement> root = readRoot(file, kPluginSetTag);
    if (root == nullptr)
    {
        return -1;
    }
    return root->getNumChildElements();
}
}
