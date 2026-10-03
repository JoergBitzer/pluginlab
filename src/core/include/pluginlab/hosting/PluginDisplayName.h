#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::hosting
{
// The name to show to the user. VST3: the name of the plugin. VST2: JUCE derives `name` from the file name; the name that the
// plugin reports itself (effGetEffectName) is in `descriptiveName`.
juce::String getDisplayName(const juce::PluginDescription& description);
}
