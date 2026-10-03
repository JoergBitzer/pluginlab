#include "pluginlab/hosting/PluginScanXml.h"

namespace pluginlab::hosting
{
namespace
{
const juce::String kRootTag = "PluginScanResult";
const juce::String kStatusAttribute = "status";
const juce::String kMessageAttribute = "message";
const juce::String kFileAttribute = "file";
}

juce::String toString(ScanStatus status)
{
    switch (status)
    {
        case ScanStatus::Ok:
            return "Ok";
        case ScanStatus::NoPluginInFile:
            return "NoPluginInFile";
        case ScanStatus::Crashed:
            return "Crashed";
        case ScanStatus::TimedOut:
            return "TimedOut";
        case ScanStatus::ScannerFailed:
            return "ScannerFailed";
    }
    return "Unknown";
}

bool writeScanResult(const PluginScanResult& result, const juce::File& xmlFile)
{
    juce::XmlElement root(kRootTag);
    root.setAttribute(kStatusAttribute, toString(result.status));
    root.setAttribute(kMessageAttribute, result.message);
    root.setAttribute(kFileAttribute, result.file.getFullPathName());
    for (const juce::PluginDescription& description : result.descriptions)
    {
        root.addChildElement(description.createXml().release());
    }
    return root.writeTo(xmlFile);
}

bool readScanResult(const juce::File& xmlFile, PluginScanResult& result)
{
    const std::unique_ptr<juce::XmlElement> root = juce::XmlDocument::parse(xmlFile);
    if (root == nullptr || ! root->hasTagName(kRootTag))
    {
        return false;
    }

    const juce::String statusText = root->getStringAttribute(kStatusAttribute);
    result.status = ScanStatus::Crashed;
    if (statusText == toString(ScanStatus::Ok))
    {
        result.status = ScanStatus::Ok;
    }
    if (statusText == toString(ScanStatus::NoPluginInFile))
    {
        result.status = ScanStatus::NoPluginInFile;
    }

    result.message = root->getStringAttribute(kMessageAttribute);
    result.descriptions.clear();
    for (const juce::XmlElement* child : root->getChildIterator())
    {
        juce::PluginDescription description;
        if (description.loadFromXml(*child))
        {
            result.descriptions.add(description);
        }
    }
    return true;
}
}
