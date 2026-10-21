#pragma once

#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::engine
{
// The impulse response of a plugin with its whole tail (W11.1a, docs/design/W11-delay-plan.md 4.1): a delta impulse (the author: "since we are
// in the digital domain, this should work in any case": no deconvolution, no time aliasing of a too short sweep period) into a fresh instance
// with the setting delivered the careful way (prepare, every parameter first to another value, the setting, settling in silence), then
// silence until the output has decayed: below silenceDbfs for holdSeconds. A delay with feedback near 100 %, a frozen reverb or a
// self-oscillating loop is stopped at maxSeconds or at the safety limit and reported as such. The plugin sees a play head with the tempo.
struct ImpulseRenderOptions
{
    double sampleRate = 48000.0;
    int blockSize = 512;
    double tempoBpm = 120.0;             // the tempo of the play head (the author: 120 and an odd second one, 97)
    double levelDbfs = -6.0;             // the height of the impulse
    int preDelaySamples = 1024;          // silence before the impulse (what comes out before it is the plugin's own signal)
    double silenceDbfs = -100.0;         // decayed: every output sample below this ...
    double holdSeconds = 1.0;            // ... for this long
    double maxSeconds = 30.0;            // the longest render after the impulse
    double safetyLimitDbfs = 24.0;       // a sample above this stops the render: a loop that grows
    double settleSeconds = 0.25;         // silence after the setting
    int impulseChannel = -1;             // the input channel that gets the impulse (0, 1), or -1: both
};

struct ImpulseRender
{
    std::vector<std::vector<float>> output; // per output channel, from the impulse on (sample 0 = the impulse)
    std::vector<float> beforeImpulse;       // the largest output per channel before the impulse arrived
    int channels = 0;
    bool decayed = false;                   // the output stayed below silenceDbfs for holdSeconds
    bool overflowed = false;                // a sample above safetyLimitDbfs: the render was stopped
    double seconds = 0.0;                   // the length rendered after the impulse
    juce::String error;                     // the plugin could not be created
};

ImpulseRender renderImpulse(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& setting,
                            const ImpulseRenderOptions& options = {});
}
