#include "pluginlab/ui/ChartImage.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/ui/PlasmaColourMap.h"
#include "pluginlab/ui/ResponsePlotImage.h"

namespace pluginlab::ui
{
namespace
{
constexpr int kLeft = 64;
constexpr int kRight = 24;
constexpr int kTop = 34;
constexpr int kBottom = 44;
constexpr int kColourBar = 70;       // the heat map's colour bar and its labels
constexpr float kFontHeight = 14.0f;
constexpr int kTick = 5;
constexpr int kTicks = 6;            // about this many labels on a linear axis
const juce::Colour kBackground(0xff1e1e1e);
const juce::Colour kText(0xffe0e0e0);
const juce::Colour kGrid(0xff4a4a4a);

// a nice step for about `ticks` labels between low and high (1, 2, 5 times a power of ten)
double getNiceStep(double low, double high, int ticks)
{
    const double raw = std::max((high - low) / ticks, 1.0e-12);
    const double power = std::pow(10.0, std::floor(std::log10(raw)));
    for (const double multiple : {1.0, 2.0, 5.0, 10.0})
    {
        if (multiple * power >= raw)
        {
            return multiple * power;
        }
    }
    return 10.0 * power;
}

juce::String formatNumber(double value, double step)
{
    if (std::abs(value) >= 1000.0 && std::abs(std::fmod(value, 1000.0)) < 1.0e-9)
    {
        return juce::String(juce::roundToInt(value / 1000.0)) + "k";
    }
    int decimals = 0;
    while (decimals < 4 && std::abs(step * std::pow(10.0, decimals) - std::round(step * std::pow(10.0, decimals))) > 1.0e-6)
    {
        ++decimals;
    }
    if (decimals == 0)
    {
        return juce::String(juce::roundToInt(value));
    }
    return juce::String(value, decimals);
}

juce::Colour getPlasma(double value, double floor, double ceiling)
{
    const double position = std::clamp((value - floor) / std::max(ceiling - floor, 1.0e-12), 0.0, 1.0);
    const std::array<float, 3>& entry = kPlasma[static_cast<size_t>(std::lround(position * 255.0))];
    return juce::Colour::fromFloatRGBA(entry[0], entry[1], entry[2], 1.0f);
}

// x and y positions of values on the plot area (linear or log)
struct Axis
{
    double minimum = 0.0;
    double maximum = 1.0;
    bool log = false;
    float start = 0.0f;  // pixel of the minimum
    float length = 1.0f; // pixels to the maximum (negative for y: upwards)

    float map(double value) const
    {
        double position = 0.0;
        if (log)
        {
            position = std::log(std::max(value, minimum * 1.0e-6) / minimum) / std::log(maximum / minimum);
        }
        else
        {
            position = (value - minimum) / (maximum - minimum);
        }
        return start + static_cast<float>(position) * length;
    }
};

// the labels of an axis: 1-2-5 per decade (log) or nice steps (linear)
std::vector<std::pair<double, juce::String>> getTicks(double minimum, double maximum, bool log)
{
    std::vector<std::pair<double, juce::String>> ticks;
    if (log)
    {
        for (double decade = std::pow(10.0, std::floor(std::log10(minimum))); decade <= maximum; decade *= 10.0)
        {
            for (const double multiple : {1.0, 2.0, 5.0})
            {
                const double value = decade * multiple;
                if (value >= minimum * 0.999 && value <= maximum * 1.001)
                {
                    ticks.emplace_back(value, formatNumber(value, value));
                }
            }
        }
        return ticks;
    }
    const double step = getNiceStep(minimum, maximum, kTicks);
    for (double value = std::ceil(minimum / step) * step; value <= maximum + 1.0e-9 * step; value += step)
    {
        ticks.emplace_back(value, formatNumber(value, step));
    }
    return ticks;
}

void drawFrame(juce::Graphics& g, juce::Rectangle<int> plot, const Axis& x, const Axis& y, const juce::String& title, const juce::String& xLabel,
               const juce::String& yLabel, const std::vector<std::pair<double, juce::String>>& yTicks, int width)
{
    g.setFont(juce::FontOptions(kFontHeight));
    g.setColour(kText);
    g.drawText(title, plot.getX(), 0, width - plot.getX(), kTop, juce::Justification::centredLeft, true);
    for (const auto& [value, text] : getTicks(x.minimum, x.maximum, x.log))
    {
        const float position = x.map(value);
        g.setColour(kGrid);
        g.drawVerticalLine(juce::roundToInt(position), static_cast<float>(plot.getY()), static_cast<float>(plot.getBottom()));
        g.setColour(kText);
        g.drawText(text, juce::roundToInt(position) - 30, plot.getBottom() + kTick, 60, juce::roundToInt(kFontHeight), juce::Justification::centred, false);
    }
    for (const auto& [value, text] : yTicks)
    {
        const float position = y.map(value);
        g.setColour(kGrid);
        g.drawHorizontalLine(juce::roundToInt(position), static_cast<float>(plot.getX()), static_cast<float>(plot.getRight()));
        g.setColour(kText);
        g.drawText(text, 0, juce::roundToInt(position - kFontHeight / 2.0f), plot.getX() - kTick, juce::roundToInt(kFontHeight), juce::Justification::centredRight, false);
    }
    g.setColour(kText);
    g.drawText(xLabel, plot.getX(), plot.getBottom() + kTick + juce::roundToInt(kFontHeight), plot.getWidth(), juce::roundToInt(kFontHeight) + 4,
               juce::Justification::centred, false);
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, 10.0f, static_cast<float>(plot.getCentreY())));
    g.drawText(yLabel, 10 - plot.getHeight() / 2, plot.getCentreY() - 8, plot.getHeight(), 16, juce::Justification::centred, false);
    g.restoreState();
    g.setColour(kGrid.brighter(0.3f));
    g.drawRect(plot.expanded(1));
}
}

juce::Colour getChartColour(int index)
{
    // the response plot's five, then three more (a chart of the octaves has seven curves)
    static const juce::Colour more[] = {juce::Colour(0xff4dd0c8), juce::Colour(0xffff6b6b), juce::Colour(0xffa0a0ff)};
    if (index >= 0 && index % 8 >= 5)
    {
        return more[index % 8 - 5];
    }
    return getPlotColour(index % 8);
}

juce::Image renderChart(const std::vector<ChartSeries>& series, const ChartOptions& options)
{
    juce::Image image(juce::Image::RGB, options.width, options.height, true, juce::SoftwareImageType());
    juce::Graphics g(image);
    g.fillAll(kBackground);
    const juce::Rectangle<int> plot(kLeft, kTop, options.width - kLeft - kRight, options.height - kTop - kBottom);
    const Axis x{options.xMinimum, options.xMaximum, options.logX, static_cast<float>(plot.getX()), static_cast<float>(plot.getWidth())};
    const Axis y{options.yMinimum, options.yMaximum, false, static_cast<float>(plot.getBottom()), -static_cast<float>(plot.getHeight())};
    drawFrame(g, plot, x, y, options.title, options.xLabel, options.yLabel, getTicks(options.yMinimum, options.yMaximum, false), options.width);
    g.saveState();
    g.reduceClipRegion(plot);
    for (const ChartSeries& one : series)
    {
        g.setColour(one.colour);
        const size_t count = std::min(one.x.size(), one.y.size());
        if (one.stems)
        {
            for (size_t index = 0; index < count; ++index)
            {
                const float px = x.map(one.x[index]);
                g.drawLine(px, static_cast<float>(plot.getBottom()), px, y.map(one.y[index]), one.thickness);
            }
            continue;
        }
        juce::Path path;
        bool started = false;
        for (size_t index = 0; index < count; ++index)
        {
            if (! std::isfinite(one.y[index]) || (options.logX && one.x[index] <= 0.0))
            {
                started = false;
                continue;
            }
            const juce::Point<float> point(x.map(one.x[index]), y.map(one.y[index]));
            if (started)
            {
                path.lineTo(point);
            }
            else
            {
                path.startNewSubPath(point);
                started = true;
            }
        }
        g.strokePath(path, juce::PathStrokeType(one.thickness));
    }
    for (const auto& [value, text] : options.markers)
    {
        const float px = x.map(value);
        g.setColour(kText.withAlpha(0.7f));
        g.drawLine(px, static_cast<float>(plot.getY()), px, static_cast<float>(plot.getBottom()), 1.0f);
        g.drawText(text, juce::roundToInt(px) + 4, plot.getY() + 2, 200, juce::roundToInt(kFontHeight), juce::Justification::centredLeft, false);
    }
    g.restoreState();
    // the legend, top right
    int row = 0;
    for (const ChartSeries& one : series)
    {
        if (one.name.isEmpty())
        {
            continue;
        }
        const int top = plot.getY() + 4 + row * juce::roundToInt(kFontHeight + 4);
        g.setColour(one.colour);
        g.fillRect(plot.getRight() - 190, top + 5, 14, 4);
        g.setColour(kText);
        g.drawText(one.name, plot.getRight() - 170, top, 168, juce::roundToInt(kFontHeight), juce::Justification::centredLeft, true);
        ++row;
    }
    return image;
}

juce::Image renderHeatmap(const std::vector<std::vector<double>>& values, const HeatmapOptions& options)
{
    juce::Image image(juce::Image::RGB, options.width, options.height, true, juce::SoftwareImageType());
    juce::Graphics g(image);
    g.fillAll(kBackground);
    const juce::Rectangle<int> plot(kLeft, kTop, options.width - kLeft - kRight - kColourBar, options.height - kTop - kBottom);
    const size_t rows = std::min(values.size(), options.rowValues.size());
    if (rows < 2 || plot.getWidth() < 2 || plot.getHeight() < 2)
    {
        return image;
    }
    const double lowest = options.rowValues.front();
    const double highest = options.rowValues[rows - 1];
    const Axis x{options.xMinimum, options.xMaximum, false, static_cast<float>(plot.getX()), static_cast<float>(plot.getWidth())};
    const Axis y{lowest, highest, options.logY, static_cast<float>(plot.getBottom()), -static_cast<float>(plot.getHeight())};
    // per pixel: the row whose value is nearest on the axis, the column by time
    {
        juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
        for (int py = 0; py < plot.getHeight(); ++py)
        {
            const double position = 1.0 - (py + 0.5) / plot.getHeight();
            double value = lowest + position * (highest - lowest);
            if (options.logY)
            {
                value = lowest * std::pow(highest / lowest, position);
            }
            const auto upper = std::lower_bound(options.rowValues.begin(), options.rowValues.begin() + static_cast<std::ptrdiff_t>(rows), value);
            size_t row = static_cast<size_t>(std::distance(options.rowValues.begin(), upper));
            if (row >= rows)
            {
                row = rows - 1;
            }
            else if (row > 0 && std::abs(options.rowValues[row - 1] - value) < std::abs(options.rowValues[row] - value))
            {
                row -= 1;
            }
            const std::vector<double>& line = values[row];
            for (int px = 0; px < plot.getWidth(); ++px)
            {
                if (line.empty())
                {
                    continue;
                }
                const size_t column = std::min(line.size() - 1, static_cast<size_t>((px + 0.5) / plot.getWidth() * line.size()));
                pixels.setPixelColour(plot.getX() + px, plot.getY() + py, getPlasma(line[column], options.floor, options.ceiling));
            }
        }
    }
    std::vector<std::pair<double, juce::String>> yTicks = options.rowLabels;
    if (yTicks.empty())
    {
        yTicks = getTicks(lowest, highest, options.logY);
    }
    drawFrame(g, plot, x, y, options.title, options.xLabel, options.yLabel, yTicks, options.width - kColourBar);
    // the colour bar
    const juce::Rectangle<int> bar(plot.getRight() + 16, plot.getY(), 16, plot.getHeight());
    for (int py = 0; py < bar.getHeight(); ++py)
    {
        const double value = options.ceiling - (options.ceiling - options.floor) * (py + 0.5) / bar.getHeight();
        g.setColour(getPlasma(value, options.floor, options.ceiling));
        g.fillRect(bar.getX(), bar.getY() + py, bar.getWidth(), 1);
    }
    g.setColour(kText);
    for (const auto& [value, text] : getTicks(options.floor, options.ceiling, false))
    {
        const int py = bar.getBottom() - juce::roundToInt((value - options.floor) / (options.ceiling - options.floor) * bar.getHeight());
        g.drawText(text, bar.getRight() + 4, py - 7, kColourBar - 36, 14, juce::Justification::centredLeft, false);
    }
    g.drawText(options.unit, bar.getX() - 4, bar.getBottom() + 4, 50, 14, juce::Justification::centredLeft, false);
    return image;
}

bool writePng(const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    return stream.openedOk() && png.writeImageToStream(image, stream);
}
}
