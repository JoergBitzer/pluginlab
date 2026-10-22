#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "pluginlab/measure/Dynamics.h"
#include "pluginlab/measure/Loudness.h"

namespace pluginlab::ui
{
// The views of a compressor (W11.3f, docs/design/W11-dynamics-plan.md 6) as PNG files next to a report: the static curve (input against output
// level, the 1 : 1 line, the fitted model, the threshold marked) and the gain reduction over the input level; the gain curves of the attack and
// the release phase with the fitted time model over the measurement (the author: attack and release must be visualised; hold and look-ahead
// marked); the dynamic transfer function of the two-tone test. Files <stem>_<view>.png in `folder`; returns the Markdown section that shows them
// (empty if the device is not level dependent).
juce::String writeDynamicsViews(const measure::DynamicsMeasurement& dynamics, double sampleRate, const juce::File& folder, const juce::String& stem,
                                const juce::String& name);

// Several devices' gain curves on one music file (4.8): one time axis, every device's g(t) (getGainCurve) and the input's level under them
struct MusicGainTrace
{
    juce::String name;
    measure::GainCurve curve;
};

juce::Image renderMusicGainCurves(const std::vector<MusicGainTrace>& traces, const juce::String& title);
}
