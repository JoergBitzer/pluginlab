#include "pluginlab/hosting/LoaderState.h"

namespace pluginlab::hosting
{
namespace
{
const juce::String kRootTag = "PluginLabLoaderState";
const juce::String kHostedStateTag = "HostedPluginState";
}

juce::MemoryBlock createLoaderState(const juce::PluginDescription& description, const juce::MemoryBlock& hostedPluginState)
{
    juce::XmlElement root(kRootTag);
    root.addChildElement(description.createXml().release());
    juce::XmlElement* stateElement = root.createNewChildElement(kHostedStateTag);
    stateElement->addTextElement(hostedPluginState.toBase64Encoding());

    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(root, block);
    return block;
}

bool parseLoaderState(const void* data, int sizeInBytes, juce::PluginDescription& description, juce::MemoryBlock& hostedPluginState)
{
    const std::unique_ptr<juce::XmlElement> root = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes);
    if (root == nullptr || ! root->hasTagName(kRootTag))
    {
        return false;
    }

    const juce::XmlElement* descriptionElement = root->getChildByName("PLUGIN");
    if (descriptionElement == nullptr || ! description.loadFromXml(*descriptionElement))
    {
        return false;
    }

    hostedPluginState.reset();
    const juce::XmlElement* stateElement = root->getChildByName(kHostedStateTag);
    if (stateElement != nullptr)
    {
        hostedPluginState.fromBase64Encoding(stateElement->getAllSubText());
    }
    return true;
}
}
