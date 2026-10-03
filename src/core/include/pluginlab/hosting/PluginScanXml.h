#pragma once

#include "pluginlab/hosting/PluginScanResult.h"

namespace pluginlab::hosting
{
// The scanner process writes its result as XML, the host reads it.
// Returns false if the file cannot be written / does not contain a scan result.
bool writeScanResult(const PluginScanResult& result, const juce::File& xmlFile);
bool readScanResult(const juce::File& xmlFile, PluginScanResult& result);
}
