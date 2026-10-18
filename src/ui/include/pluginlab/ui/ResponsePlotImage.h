#pragma once

#include <vector>

#include <juce_graphics/juce_graphics.h>

namespace pluginlab::ui
{
// Magnitude responses as a picture for the reports (the match report of W9.7): the curves on a logarithmic frequency axis (1-2-5 labels per
// decade), the differences in a strip below, a legend; drawn like the spectrogram (dark background).
struct PlotCurve
{
    juce::String name;
    juce::Colour colour;
    std::vector<double> valuesDb;    // on the plot's frequencies
    float thickness = 2.0f;
};

struct ResponsePlotOptions
{
    int width = 1000;
    int height = 560;
    double rangeDb = 24.0;           // the curves: -range ... +range
    double differenceRangeDb = 2.0;  // the strip below
    juce::String title;
    juce::String differenceTitle = "target - master (dB)";
};

juce::Image renderResponsePlot(const std::vector<double>& frequencies, const std::vector<PlotCurve>& curves, const std::vector<PlotCurve>& differences,
                               const ResponsePlotOptions& options);

bool writeResponsePlotPng(const std::vector<double>& frequencies, const std::vector<PlotCurve>& curves, const std::vector<PlotCurve>& differences,
                          const ResponsePlotOptions& options, const juce::File& file);

// The colours of the curves of a plot: the master light gray, the targets in this order
juce::Colour getPlotColour(int index);
}
