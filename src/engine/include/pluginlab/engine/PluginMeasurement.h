#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/measure/Device.h"

namespace pluginlab::engine
{
// The measurement units of W7 run on one device with their AES17 defaults and summarised as a table (docs/measurements/plugins-and-host.md).
// One row per result; the document of each unit explains it.
struct MeasurementRow
{
    juce::String unit;                           // e.g. "THD+N"
    juce::String quantity;                       // e.g. "THD+N at -1 dBFS, 997 Hz"
    double value = 0.0;                          // in the unit of the text (NaN: not applicable)
    juce::String text;                           // the value with its unit, e.g. "-93.1 dB"
    juce::String clause;                         // e.g. "AES17 6.3.1"
};

struct MeasurementSummary
{
    juce::String deviceName;
    double sampleRate = 48000.0;
    int channels = 0;
    std::vector<MeasurementRow> rows;
    double seconds = 0.0;                        // wall-clock time of the measurement
};

struct MeasurementOptions
{
    double sampleRate = 48000.0;
    bool linearity = true;                       // gain non-linearity (the slowest unit: up to 28 levels)
};

// All units on a device with `channels` output channels (1 or 2 are used)
MeasurementSummary measureDevice(const measure::Device& device, int channels, const juce::String& deviceName, const MeasurementOptions& options = {});

// The same for a plugin: the plugin device (careful delivery of the setting; empty setting: the plugin's defaults)
MeasurementSummary measurePlugin(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                                 const MeasurementOptions& options = {});

// The summary as a Markdown section (for the fingerprint report and --measure)
juce::String createMeasurementReport(const MeasurementSummary& summary);

// Finds a row by its quantity (nullptr if there is none)
const MeasurementRow* findRow(const MeasurementSummary& summary, const juce::String& quantity);
}
