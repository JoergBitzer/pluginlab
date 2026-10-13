#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::engine
{
// The Markdown of a fingerprint report for a window with a monospaced font: every table is turned into aligned columns (cells padded to the
// width of the widest cell, the separator line replaced by a line of dashes), the other lines are kept. Headings are not touched (the
// window sets their font).
juce::String alignMarkdownTables(const juce::String& markdown);

// The line length of the reports' running text (tables are not wrapped)
constexpr int kReportLineLength = 78;

// Text wrapped at word boundaries into lines of at most `width` characters (a word longer than that stays whole)
juce::String wrapText(const juce::String& text, int width = kReportLineLength);

// A frequency for a report, without false precision: one decimal below 100 Hz, whole hertz above ("20.9", "1000", "16860").
// (juce::String(x, 0) does not round: it gives every digit.)
juce::String formatFrequency(double hertz);

// A plugin's text without false precision: a number with more than two decimals rounded ("7678.3115234" -> "7678", "7.5249996" -> "7.52")
juce::String tidyNumberText(const juce::String& text);

// Who made a review: "Jade Benchmark, pluginlab 0.42.0 (PluginLabHost; JUCE v9.0.3; Linux), 2026-10-11 10:12", for the head of every report
juce::String getReviewSignature(const juce::String& program);

// The head of a report: the logo (an image file next to the report, written by the caller), the title and the signature
juce::String makeReportHead(const juce::String& title, const juce::String& program);

// Every line of running text wrapped at `width` characters; tables, headings, images, code blocks and empty lines unchanged; a list item
// ("- ...") keeps its marker and its continuation lines are indented
juce::String wrapMarkdownProse(const juce::String& markdown, int width = kReportLineLength);

// A finished report: the logo above the first heading, "Reviewed by <signature>" below it, the running text wrapped
juce::String signReport(const juce::String& markdown, const juce::String& program);

// The file name of the logo that the report heads refer to
constexpr const char* kLogoFileName = "jade_benchmark_logo.png";
}
