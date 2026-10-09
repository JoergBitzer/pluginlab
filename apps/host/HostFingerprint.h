#pragma once

#include <juce_core/juce_core.h>

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
}
