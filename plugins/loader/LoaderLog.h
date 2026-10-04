#pragma once

#include <juce_events/juce_events.h>

namespace loaderlog
{
// Writes one line (with time and thread) into the log file of the loader: <user application data>/PluginLab/PluginLabLoader.log.
// A plugin has no console, and a crash inside a DAW leaves nothing else behind. Thread safe.
void write(const juce::String& message);

juce::File getLogFile();
}
