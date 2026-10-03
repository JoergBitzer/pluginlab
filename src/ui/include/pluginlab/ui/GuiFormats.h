#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace pluginlab::ui
{
// Adds the plugin formats that pluginlab hosts, with editor support (VST3 and, if built with the FST headers, VST2).
// The headless version for scanners and tests is pluginlab::hosting::addHeadlessFormats.
void addGuiFormats(juce::AudioPluginFormatManager& formatManager);
}
