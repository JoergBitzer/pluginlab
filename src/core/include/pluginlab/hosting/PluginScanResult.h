#pragma once

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::hosting
{
enum class ScanStatus
{
    Ok,             // the file contains at least one plugin
    NoPluginInFile, // the file could be examined, but contains no plugin of the format
    Crashed,        // the scanner process died or wrote no result: loading this file is not safe
    TimedOut,       // the scanner did not finish in time and was killed
    ScannerFailed   // the scanner process could not be started
};

struct PluginScanResult
{
    juce::File file;
    ScanStatus status = ScanStatus::ScannerFailed;
    juce::String message;
    juce::Array<juce::PluginDescription> descriptions;
};

// A short name of a status for lists and reports ("Ok", "Crashed", ...).
juce::String toString(ScanStatus status);
}
