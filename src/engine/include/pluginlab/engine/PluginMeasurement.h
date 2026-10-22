#pragma once

#include <functional>
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

// The units by key (the Expert page chooses them, W10.5); the system class always runs first
const juce::StringArray& getMeasurementUnitKeys();       // gain, response, delay, phase, distortion, imd, noise, crosstalk, maximum, linearity, echoes, reverb
juce::String getMeasurementUnitTitle(const juce::String& key);

struct MeasurementOptions
{
    double sampleRate = 48000.0;
    bool linearity = true;                       // gain non-linearity (the slowest unit: up to 28 levels)
    juce::StringArray units;                     // the keys of getMeasurementUnitKeys() to run; empty: all
    double latencySamples = -1.0;                // the device's latency (the dry path of the echo response); < 0: unknown (measurePlugin: the reported one)
    std::function<void(double fraction, const juce::String& unit)> progress; // before each unit
};

// All units on a device with `channels` output channels (1 or 2 are used)
MeasurementSummary measureDevice(const measure::Device& device, int channels, const juce::String& deviceName, const MeasurementOptions& options = {});

// The same for a plugin: the plugin device (careful delivery of the setting; empty setting: the plugin's defaults)
MeasurementSummary measurePlugin(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                                 const MeasurementOptions& options = {});

// The summary as a Markdown section (for the fingerprint report and --measure)
juce::String createMeasurementReport(const MeasurementSummary& summary);

// The summary as JSON (device, rate, channels, seconds, rows) and back: the Expert page reads what the measurement process wrote
juce::var createMeasurementJson(const MeasurementSummary& summary);
MeasurementSummary parseMeasurementJson(const juce::var& json);

// The echo response in one line for the Developer page (W11.1f): "none", or the delay time, the stereo kind, saturation, modulation; empty if
// the unit did not run
juce::String summarizeEchoes(const MeasurementSummary& summary);
// The same for the reverb (W11.2e): "none", or T30, pre-delay, the late IACC
juce::String summarizeReverb(const MeasurementSummary& summary);

// Two delays compared echo by echo (W11.1g, docs/design/W11-delay-plan.md 5; no matching): the echo response of each (its unit "echoes") and
// a Markdown report: the quantities side by side, the echoes paired by their order, the loop filters at octave frequencies
juce::String compareEchoResponses(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& descriptionA, const std::vector<float>& settingA,
                                  const juce::PluginDescription& descriptionB, const std::vector<float>& settingB, double sampleRate = 48000.0);

// Two reverbs compared (W11.2f, docs/design/W11-reverb-plan.md 5): the reverb measurement of each at its setting and a Markdown report: T30 per
// octave and third octave with the verdict on the JND scale (the same <= 5 %, audibly close <= 10 %, different), the other quantities side by side
juce::String compareReverbResponses(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& descriptionA, const std::vector<float>& settingA,
                                    const juce::PluginDescription& descriptionB, const std::vector<float>& settingB, double sampleRate = 48000.0);

// Finds a row by its quantity (nullptr if there is none)
const MeasurementRow* findRow(const MeasurementSummary& summary, const juce::String& quantity);
}
