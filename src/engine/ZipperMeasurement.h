#pragma once

#include <vector>

#include "PluginRig.h"
#include "pluginlab/engine/ZipperNoise.h"

// Internal header of pluginlab_engine: the zipper measurement of the fingerprint
namespace pluginlab::engine::detail
{
struct ZipperCandidate
{
    int index = 0;
    juce::String name;
};

// Every candidate ramps from the low to the high test position with the others at `base`
ZipperFingerprint measureZipperNoise(const Bench& bench, const std::vector<float>& base, const std::vector<ZipperCandidate>& candidates,
                                     const FingerprintSettings& settings);
}
