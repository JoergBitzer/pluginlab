#pragma once

#include <functional>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>

#include "pluginlab/reference/Processor.h"

namespace pluginlab::measure
{
// The equipment under test (AES17: EUT): renders an input buffer at a sample rate, from a fresh state, and returns the output (same number of
// channels and samples). Every measurement unit works on a Device, whatever is behind it (a reference processor, a hosted plugin).
using Device = std::function<juce::AudioBuffer<float>(const juce::AudioBuffer<float>& input, double sampleRate)>;

// A reference processor as a device: a new processor for every render (fresh state), made by the factory for the sample rate
Device makeProcessorDevice(std::function<std::unique_ptr<reference::Processor>(double sampleRate)> factory);
}
