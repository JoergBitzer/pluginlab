#pragma once

#include <utility>
#include <vector>

#include <juce_graphics/juce_graphics.h>

namespace pluginlab::ui
{
// Charts and heat maps as pictures for the reports (W11.2, the views of docs/design/W11-reverb-plan.md 6), drawn like the response plot and the
// spectrogram (dark background, software images: the same on every platform).

// A series of a chart: points (x, y) as a line, or as stems from the chart's lower edge
struct ChartSeries
{
    juce::String name;
    juce::Colour colour;
    std::vector<double> x;
    std::vector<double> y;
    bool stems = false;
    float thickness = 2.0f;
};

struct ChartOptions
{
    int width = 1000;
    int height = 420;
    juce::String title;
    juce::String xLabel;
    juce::String yLabel;
    bool logX = false;                 // a logarithmic x axis (frequencies: 1-2-5 labels per decade)
    double xMinimum = 0.0;
    double xMaximum = 1.0;
    double yMinimum = 0.0;
    double yMaximum = 1.0;
    std::vector<std::pair<double, juce::String>> markers; // vertical lines at x with a label
};

juce::Image renderChart(const std::vector<ChartSeries>& series, const ChartOptions& options);

// A heat map: values[row][column], the rows from bottom to top at rowValues (increasing; drawn on a log axis if logY), the columns evenly from
// xMinimum to xMaximum; the plasma colour map from floor to ceiling with its colour bar; rowLabels: custom labels at row values (else numbers)
struct HeatmapOptions
{
    int width = 1000;
    int height = 420;
    juce::String title;
    juce::String xLabel;
    juce::String yLabel;
    juce::String unit = "dB";
    double xMinimum = 0.0;
    double xMaximum = 1.0;
    std::vector<double> rowValues;
    bool logY = false;
    double floor = -60.0;
    double ceiling = 0.0;
    std::vector<std::pair<double, juce::String>> rowLabels;
};

juce::Image renderHeatmap(const std::vector<std::vector<double>>& values, const HeatmapOptions& options);

// The colour of the n-th series (the response plot's colours)
juce::Colour getChartColour(int index);

bool writePng(const juce::Image& image, const juce::File& file);
}
