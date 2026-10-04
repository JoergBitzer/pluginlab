#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "pluginlab/hosting/PluginValidator.h"

namespace pluginlab::hosting
{
// A quick check of ONE plugin of a file: load, prepare, process noise, change parameters, save and restore the state, delete.
// For files that contain many plugins (a bundle like LSP's has about 190) where pluginval, which always tests every plugin of the
// file, would take far too long. The check runs in a scanner process (PluginLabScanner --probe), so a crash of the plugin ends
// that process and is reported as a failed check, with the step in which it happened.

// The work of the scanner process.
// The file with the steps is written while the check runs; the line RESULT tells how it ended.
// Returns true if the plugin passed.
bool probePluginInProcess(juce::AudioPluginFormatManager& formatManager,
                          const juce::File& pluginFile,
                          const juce::String& pluginIdentifier,
                          const juce::File& resultFile);

// Starts the scanner process for the check and reads the outcome. Never throws, never crashes.
ValidationResult probePlugin(const juce::File& scannerExecutable,
                             const juce::File& pluginFile,
                             const juce::String& pluginIdentifier,
                             int timeoutMs);

// The argument of the scanner executable that selects the check
const juce::String& getProbeArgument();
}
