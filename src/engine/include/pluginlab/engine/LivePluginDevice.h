#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "pluginlab/measure/Device.h"
#include "pluginlab/measure/SystemClass.h"

namespace pluginlab::engine
{
// The fast render path of the matcher (W9.3, docs/matching/fast-render-path.md). The careful delivery of makePluginDevice (a fresh instance per
// render, W7.11) takes too long for the author's budget of 10 ... 15 s per target. Here one instance stays alive for the whole match; a
// render sets every parameter to another value first ("poke": a plugin that reads a parameter only when it changes cannot miss the setting),
// then the setting, lets the plugin settle in silence, then runs the input. Valid only for an LTI, deterministic plugin: checkLiveTarget says
// whether it is.

struct LiveDeviceOptions
{
    int blockSize = 512;
    double settleSeconds = 0.25;    // silence after the setting: the ringing of the last render decays, smoothing ends (see the fingerprint's "settles within")
    bool poke = true;
};

class LivePluginTarget
{
public:
    LivePluginTarget(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate,
                     const LiveDeviceOptions& options = {});
    ~LivePluginTarget();

    bool isValid() const;
    juce::String getError() const;
    double getSampleRate() const;

    // A device for a setting; every call renders through the one live instance (not thread-safe: one match at a time)
    measure::Device makeDevice(const std::vector<float>& setting) const;
    int getRenders() const;

private:
    struct State;
    std::shared_ptr<State> m_state;
};

// Whether the fast path may be used for a target at a setting: the system class (W8.1, careful delivery) must be LTI (non-LTI targets are
// refused: the author, "Refusal, that is for later"); the same setting rendered twice through the live instance must give the same features
// (determinism, no state carried from one render to the next).
struct LiveTargetCheck
{
    bool usable = false;
    measure::SystemClass systemClass = measure::SystemClass::Lti;
    double repeatDifference = 0.0;   // largest difference of the features of two renders of the same setting
    juce::String reason;             // why it is refused (empty if usable)
};

LiveTargetCheck checkLiveTarget(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const LivePluginTarget& target,
                                const std::vector<float>& setting, const std::function<std::vector<double>(const measure::Device&)>& features,
                                double repeatTolerance = 0.001);
}
