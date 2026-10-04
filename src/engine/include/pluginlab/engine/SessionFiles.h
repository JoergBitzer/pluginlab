#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::engine
{
// The two things a user wants to keep and switch between, each in a file of its own (XML):
//  - the audio list: the files with their passes and loop regions
//  - the plugin set: the plugins of the slots with their states (parameters), in the order of the slots, and which slot is audible
// The dry slot is not saved: the host always has it.

inline const juce::String kAudioListExtension = ".audiolist";
inline const juce::String kPluginSetExtension = ".pluginset";

bool saveAudioList(const MeasurementEngine& engine, const juce::File& file);

// Replaces the files of the engine by the list in the file. Files that cannot be read are skipped and named in the report. Returns false
// if the file is not an audio list.
bool loadAudioList(MeasurementEngine& engine, const juce::File& file, juce::String& report);

// The number of files in a saved audio list, -1 if the file is not one (for the question at the start of the program)
int countFilesInAudioList(const juce::File& file);

bool savePluginSet(MeasurementEngine& engine, const juce::File& file);

// Removes the plugin slots of the engine and loads the plugins of the file into new slots (the dry slot stays). Plugins that cannot be
// loaded are skipped and named in the report. Returns false if the file is not a plugin set.
bool loadPluginSet(MeasurementEngine& engine, juce::AudioPluginFormatManager& formatManager, const juce::File& file, juce::String& report);

// The number of plugins in a saved plugin set, -1 if the file is not one
int countPluginsInPluginSet(const juce::File& file);
}
