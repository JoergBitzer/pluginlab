#include "pluginlab/hosting/PluginDisplayName.h"

namespace pluginlab::hosting
{
namespace
{
const juce::String kVst2FormatName = "VST";
}

juce::String getDisplayName(const juce::PluginDescription& description)
{
    const bool isVst2 = description.pluginFormatName == kVst2FormatName;
    if (isVst2 && description.descriptiveName.isNotEmpty())
    {
        return description.descriptiveName;
    }
    return description.name;
}
}
