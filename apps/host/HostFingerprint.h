#pragma once

#include <juce_core/juce_core.h>

#include "PageServices.h"

namespace pluginlab::host
{
// Command line mode --fingerprint <plugin file> <report file> [<plugin identifier>]: measures the fingerprint of every plugin in the file (see
// docs/design/W5-fingerprint.md) and writes the report (Markdown) and, for each plugin, its summary as JSON (<report file>.json). A plugin that crashes ends the program: no report, which tells.
// pluginIdentifier: only the plugin with this identifier string (PluginDescription::createIdentifierString) of a file that holds many; empty:
// all plugins of the file. Returns false if the file contains no such plugin or the report cannot be written.
// The report ends with the measurement units of W7 on the plugin's default setting (docs/measurements/plugins-and-host.md), unless the
// fingerprint settings switch them off ("measurements": false).
bool writeFingerprintReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier = {});

// Command line mode --measure <plugin file> <report file> [<plugin identifier>]: only the measurement units (W7.11) on the default setting of every
// plugin of the file (or the one with the identifier); Markdown. Returns false if there is no such plugin or the report cannot be written.
bool writeMeasurementReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier = {});

// Command line mode --map <plugin file> <report file> [<plugin identifier>]: the parameter survey (W8.4) and the mapping proposal (W8.5) of every
// plugin of the file (or the one with the identifier). The report (Markdown) has both; the proposal goes as JSON into the user's mapping folder
// (HostSettings::getMappingFolder). A confirmed mapping there is never overwritten: the proposal is then written next to it
// (<name>.proposal.json) and the report lists how it differs. Returns false if there is no such plugin or a file cannot be written.
bool writeMappingProposal(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier = {});

// Command line mode --calibrate <plugin file> <report file> [<plugin identifier>]: the calibration (W8.6, "myth and reality") of every plugin of
// the file (or the one with the identifier): the band parameters of its mapping, shown against measured, at 44.1, 48 and 96 kHz; only the
// deviations; then whether the bands add in dB (W8.7). The mapping comes from the user's mapping folder; without one the survey proposes it first
// (and writes it there, as --map does).
bool writeCalibrationReport(const juce::File& pluginFile, const juce::File& reportFile, const juce::String& pluginIdentifier = {});

// Command line mode --match <job file> <report file>: the matcher (W9.4, docs/matching/plugin-matching.md). The job (JSON) names the master
// ("plugin" with a "setting" of normalised values by parameter name, or a "curve" of bands), the targets ("plugin", "release": the parameter
// names to optimize, "start": values by name, "modelStart": from the master's bands through the target's mapping in the user's mapping
// folder), "tolerance" (exact, strict, medium, loose), "seconds" per target, "lowestHz" and "highestHz". Instead of names: "values" (the whole
// setting by position) and "releaseIndices". Writes the report (Markdown) and <report>.json (per target: the found setting, released, matched).
bool writeMatchReport(const juce::File& jobFile, const juce::File& reportFile);

// (the progress file of the jobs: PageServices.h of the pages library)
}
