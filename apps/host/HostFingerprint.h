#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::host
{
// Command line mode --fingerprint <plugin file> <report file>: measures the fingerprint of every plugin in the file (see
// docs/design/W5-fingerprint.md) and writes the report (Markdown). A plugin that crashes ends the program: no report, which tells.
// Returns false if the file contains no plugin or the report cannot be written.
bool writeFingerprintReport(const juce::File& pluginFile, const juce::File& reportFile);
}
