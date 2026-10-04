#include "pluginlab/hosting/PluginDisplayName.h"

namespace pluginlab::hosting
{
namespace
{
const juce::String kVst2FormatName = "VST";
const juce::String kEffectText = "effect";
const juce::String kInstrumentText = "instrument";
const juce::String kSeparator = "|";
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

juce::String getTypeText(const juce::PluginDescription& description)
{
    juce::String text = kEffectText;
    if (description.isInstrument)
    {
        text = kInstrumentText;
    }

    // the first part of the category only repeats the kind (VST3: Fx, Instrument; VST2: Effect, Synth)
    juce::StringArray parts = juce::StringArray::fromTokens(description.category, kSeparator, "");
    parts.removeEmptyStrings();
    if (parts.size() > 0)
    {
        const juce::String first = parts[0];
        const bool repeatsKind = first.equalsIgnoreCase("Fx") || first.equalsIgnoreCase("Instrument") || first.equalsIgnoreCase("Effect")
                              || first.equalsIgnoreCase("Synth");
        if (repeatsKind)
        {
            parts.remove(0);
        }
    }
    for (const juce::String& part : parts)
    {
        text += kSeparator + part;
    }
    return text;
}
}
