#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace pluginlab::reference
{
// A reference processor: processes a buffer in place (all channels at once, so that channel matrices fit in) and knows its answer
// (each derived class documents and offers its own: H(e^jw), harmonic levels, a gain curve, ...). The reference plugins of W6.4 wrap these.
class Processor
{
public:
    virtual ~Processor() = default;

    // Back to the state after construction (filters empty, noise and modulators from the start)
    virtual void reset() = 0;
    virtual void process(juce::AudioBuffer<float>& buffer) = 0;

    // The delay a host should compensate
    virtual int getLatencySamples() const;
};
}
