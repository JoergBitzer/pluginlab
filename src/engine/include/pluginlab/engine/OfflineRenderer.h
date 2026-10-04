#pragma once

#include <vector>

#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::engine
{
struct OfflineRenderResult
{
    bool ok = false;
    juce::String message;
    std::vector<juce::File> files; // one per slot, in the order of the slots
    juce::int64 samplesPerFile = 0;
};

struct OfflineRenderOptions
{
    int bitsPerSample = 24;
    double tailSeconds = 1.0; // rendered after the end of the last file, so that the tail of a plugin is in the file
};

// Renders the file list of the engine through every slot at the same time and writes one WAV file per slot into the folder (named
// 00_<slot name>.wav, 01_...). The files are aligned to the input: sample n of every file belongs to sample n of the first file of the
// list (the latency of the slots is cut off at the start and rendered at the end). The engine is restarted first and the list is
// played once (afterwards the list wraps again, the default).
OfflineRenderResult renderOffline(MeasurementEngine& engine, const juce::File& folder, const OfflineRenderOptions& options = {});

// A name for a file made from a slot name (letters, digits, '-' and '_' only).
juce::String makeFileNamePart(const juce::String& text);
}
