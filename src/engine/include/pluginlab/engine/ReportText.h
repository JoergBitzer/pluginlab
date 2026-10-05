#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::engine
{
// The Markdown of a fingerprint report for a window with a monospaced font: every table is turned into aligned columns (cells padded to the
// width of the widest cell, the separator line replaced by a line of dashes), the other lines are kept. Headings are not touched (the
// window sets their font).
juce::String alignMarkdownTables(const juce::String& markdown);
}
