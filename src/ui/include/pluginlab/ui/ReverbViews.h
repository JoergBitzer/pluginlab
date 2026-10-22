#pragma once

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Reverb.h"

namespace pluginlab::ui
{
// The views of a reverb (W11.2, docs/design/W11-reverb-plan.md 6) as PNG files next to a report: the EDC per octave with the T30 lines, the decay
// time over frequency (T30 and EDT per third octave), the energy decay relief, the echogram of the first 100 ms, the echo density profile with
// the mixing time, Griesinger's echo count, the short-time histogram with the kurtosis, the normalized autocorrelation; for two channels the
// coherence over frequency, the IAD over time and frequency and the Spatial Distribution Meter. Files <stem>_<view>.png in `folder`; returns the
// Markdown section that shows them (empty without a reverb).
juce::String writeReverbViews(const measure::ReverbMeasurement& reverb, double sampleRate, const juce::File& folder, const juce::String& stem,
                              const juce::String& name);
}
