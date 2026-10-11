#pragma once

#include <optional>
#include <vector>

#include <juce_core/juce_core.h>

namespace pluginlab::engine
{
// The law of a knob (W8.5, docs/eqmodel/mapping.md): the normalised value v (0 ... 1) -> a value x (Hz, dB, Q, samples). Fitted to the texts the
// plugin shows and to measured values.
enum class LawKind
{
    Linear,       // x = min + (max - min) v
    Logarithmic,  // x = min (max / min)^v (min, max > 0)
    Power,        // x = min + (max - min) v^(1 / skew): JUCE's NormalisableRange with a skew factor (skew 1 is linear)
    Table         // a discrete knob: one value per step
};

const char* getLawKindName(LawKind kind);

struct KnobLaw
{
    LawKind kind = LawKind::Linear;
    double minimum = 0.0;          // x at v = 0
    double maximum = 1.0;          // x at v = 1
    double skew = 1.0;             // Power only
    std::vector<double> table;     // Table: x for the steps v = k / (n - 1)
    double rmsError = 0.0;         // of the fit, relative to the range |max - min| (Table: 0)
    int points = 0;                // the points the law was fitted to

    double toValue(double normalised) const;
    double toNormalised(double value) const;       // the inverse (Table: the nearest step); clamped to 0 ... 1
    juce::String describe(const juce::String& unit) const; // "log, 20 ... 20000 Hz"

    juce::var toVar() const;
    static std::optional<KnobLaw> fromVar(const juce::var& value);
};

struct LawPoint
{
    double normalised = 0.0;
    double value = 0.0;
};

// The best law for the points: linear, logarithmic or power (the simplest one within a tenth of the best error wins); nothing for fewer than three
// points or a constant value
std::optional<KnobLaw> fitKnobLaw(const std::vector<LawPoint>& points);

// A table for a discrete knob of numSteps steps from its points (one per step)
KnobLaw makeTableLaw(const std::vector<LawPoint>& points);

// The first number in a text the plugin shows, with "k" (1000) after it ("1.2 kHz", "-3.0dB", "Q 0.70", "1k"); nothing if there is none
std::optional<double> parseDisplayedNumber(const juce::String& text);
}
