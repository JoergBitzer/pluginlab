#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::hosting
{
// Adds the plugin formats that pluginlab hosts, without GUI support: VST3 and (if built with the FST headers) VST2.
// The GUI libraries add the same formats with editor support (pluginlab::ui::addGuiFormats).
void addHeadlessFormats(juce::AudioPluginFormatManager& formatManager);

// True if this build can host VST2 plugins.
bool isVst2Supported();
}
