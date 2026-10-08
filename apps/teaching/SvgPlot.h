#pragma once

#include <string>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>

// A small line chart written as SVG, for the teaching pages (docs/teaching): light surface, hairline grid, 2 px lines in the categorical
// order of the chart palette (slot 1 blue, 2 orange, 3 aqua, 4 yellow; at most four series per chart), text in neutral ink, a legend
// above the plot for two or more series, one y axis.
class SvgPlot
{
public:
    struct Series
    {
        juce::String name;
        std::vector<double> x;
        std::vector<double> y;
        bool dashed = false;   // a reference curve (e.g. the analog prototype) as a dashed line
    };

    SvgPlot(const juce::String& title, const juce::String& xLabel, const juce::String& yLabel);

    void setLogX(bool logX);
    void setXRange(double minimum, double maximum);
    void setYRange(double minimum, double maximum);
    void addSeries(const Series& series);

    juce::String render() const;
    bool write(const juce::File& file) const;

private:
    double mapX(double x) const;
    double mapY(double y) const;
    std::vector<std::pair<double, int>> layoutLegend() const;
    double getPlotTop() const;
    std::vector<double> getXTicks() const;
    std::vector<double> getYTicks() const;

    juce::String m_title;
    juce::String m_xLabel;
    juce::String m_yLabel;
    bool m_logX = false;
    double m_xMin = 0.0;
    double m_xMax = 1.0;
    double m_yMin = 0.0;
    double m_yMax = 1.0;
    std::vector<Series> m_series;
};

// "1k", "20", "0.5"
juce::String formatTick(double value);
