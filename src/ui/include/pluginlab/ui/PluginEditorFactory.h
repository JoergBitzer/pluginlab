#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace pluginlab::ui
{
// The editor of a loaded plugin; a generic editor (a list of the parameters) if the plugin has none or cannot create one.
// The caller owns the editor and must delete it before the plugin instance.
juce::AudioProcessorEditor* createEditorFor(juce::AudioPluginInstance& instance);
}
