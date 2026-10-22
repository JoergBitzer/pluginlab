#include "pluginlab/ui/ResponsePlotImage.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::ui
{
namespace
{
constexpr int kLeft = 56;
constexpr int kRight = 20;
constexpr int kTop = 34;
constexpr int kBottom = 40;
constexpr int kGap = 24;
constexpr float kFontHeight = 14.0f;
constexpr float kDifferenceShare = 0.28f;
const juce::Colour kBackground(0xff1e1e1e);
const juce::Colour kText(0xffe0e0e0);
const juce::Colour kGrid(0xff4a4a4a);
const juce::Colour kColours[] = {juce::Colour(0xff4fa3e0), juce::Colour(0xffffa040), juce::Colour(0xff7ccf6b), juce::Colour(0xffd96bd1),
                                 juce::Colour(0xffe8d44d)};

// Frequency axis label: whole hertz, or whole kilohertz with a "k" from 1 kHz on.
juce::String formatFrequencyLabel(double frequency)
{
    if (frequency >= 1000.0)
    {
        return juce::String(juce::roundToInt(frequency / 1000.0)) + "k";
    }
    return juce::String(juce::roundToInt(frequency));
}
}

juce::Colour getPlotColour(int index)
{
    return kColours[static_cast<size_t>(index) % std::size(kColours)];
}

juce::Image renderResponsePlot(const std::vector<double>& frequencies, const std::vector<PlotCurve>& curves, const std::vector<PlotCurve>& differences,
                               const ResponsePlotOptions& options)
{
    juce::Image image(juce::Image::RGB, options.width, options.height, true, juce::SoftwareImageType()); // the same on every platform (see SpectrogramImage)
    juce::Graphics g(image);
    g.fillAll(kBackground);
    g.setFont(juce::FontOptions(kFontHeight));
    const juce::Rectangle<int> all(kLeft, kTop, options.width - kLeft - kRight, options.height - kTop - kBottom);
    int differenceHeight = 0;
    int upperTrim = 0;
    if (differences.empty())
    {
        differenceHeight = 0;
        upperTrim = 0;
    }
    else
    {
        differenceHeight = juce::roundToInt(static_cast<float>(all.getHeight()) * kDifferenceShare);
        upperTrim = differenceHeight + kGap;
    }
    const juce::Rectangle<int> upper = all.withTrimmedBottom(upperTrim);
    const juce::Rectangle<int> lower = all.withTrimmedTop(all.getHeight() - differenceHeight);
    g.setColour(kText);
    if (options.title.isNotEmpty())
    {
        g.drawText(options.title, upper.getX(), 0, upper.getWidth(), kTop, juce::Justification::centredLeft, true);
    }
    if (frequencies.size() < 2)
    {
        return image;
    }
    const double lowest = frequencies.front();
    const double highest = frequencies.back();
    const auto toX = [&](double frequency) {
        return static_cast<float>(upper.getX()) + static_cast<float>(std::log(frequency / lowest) / std::log(highest / lowest)) * static_cast<float>(upper.getWidth());
    };
    const auto drawBox = [&](const juce::Rectangle<int>& box, double range, std::initializer_list<double> levels) {
        g.setColour(kGrid);
        g.drawRect(box);
        int decimals = 0;
        if (range < 5.0)
        {
            decimals = 1;
        }
        else
        {
            decimals = 0;
        }
        for (const double level : levels)
        {
            const float y = static_cast<float>(box.getCentreY()) - static_cast<float>(level / range) * static_cast<float>(box.getHeight()) / 2.0f;
            g.setColour(kGrid);
            g.drawHorizontalLine(juce::roundToInt(y), static_cast<float>(box.getX()), static_cast<float>(box.getRight()));
            g.setColour(kText);
            g.drawText(juce::String(level, decimals), 0, juce::roundToInt(y - kFontHeight / 2.0f), kLeft - 6, juce::roundToInt(kFontHeight),
                       juce::Justification::centredRight, false);
        }
    };
    drawBox(upper, options.rangeDb, {-options.rangeDb, -options.rangeDb / 2.0, 0.0, options.rangeDb / 2.0, options.rangeDb});
    if (! differences.empty())
    {
        drawBox(lower, options.differenceRangeDb, {-options.differenceRangeDb, 0.0, options.differenceRangeDb});
    }
    // frequency grid: 1-2-5 per decade
    int bottom = 0;
    if (differences.empty())
    {
        bottom = upper.getBottom();
    }
    else
    {
        bottom = lower.getBottom();
    }
    for (double decade = 10.0; decade <= highest; decade *= 10.0)
    {
        for (const double multiple : {1.0, 2.0, 5.0})
        {
            const double frequency = decade * multiple;
            if (frequency < lowest * 0.999 || frequency > highest * 1.001)
            {
                continue;
            }
            const float x = toX(frequency);
            g.setColour(kGrid);
            g.drawVerticalLine(juce::roundToInt(x), static_cast<float>(upper.getY()), static_cast<float>(bottom));
            g.setColour(kText);
            const juce::String label = formatFrequencyLabel(frequency);
            g.drawText(label, juce::roundToInt(x) - 24, bottom + 4, 48, juce::roundToInt(kFontHeight), juce::Justification::centred, false);
        }
    }
    g.drawText("frequency in Hz", upper.getX(), options.height - juce::roundToInt(kFontHeight) - 4, upper.getWidth(), juce::roundToInt(kFontHeight),
               juce::Justification::centred, false);
    const auto drawCurve = [&](const PlotCurve& curve, const juce::Rectangle<int>& box, double range) {
        if (curve.valuesDb.size() != frequencies.size())
        {
            return;
        }
        juce::Path path;
        for (size_t index = 0; index < frequencies.size(); ++index)
        {
            const float x = toX(frequencies[index]);
            const float y = static_cast<float>(box.getCentreY())
                            - static_cast<float>(juce::jlimit(-range, range, curve.valuesDb[index]) / range) * static_cast<float>(box.getHeight()) / 2.0f;
            if (index == 0)
            {
                path.startNewSubPath(x, y);
            }
            else
            {
                path.lineTo(x, y);
            }
        }
        g.setColour(curve.colour);
        g.strokePath(path, juce::PathStrokeType(curve.thickness));
    };
    for (const PlotCurve& curve : curves)
    {
        drawCurve(curve, upper, options.rangeDb);
    }
    for (const PlotCurve& curve : differences)
    {
        drawCurve(curve, lower, options.differenceRangeDb);
    }
    // legend, top right of the curves
    int y = upper.getY() + 6;
    for (const PlotCurve& curve : curves)
    {
        g.setColour(curve.colour);
        g.drawText(curve.name, upper.getX(), y, upper.getWidth() - 8, juce::roundToInt(kFontHeight), juce::Justification::centredRight, true);
        y += juce::roundToInt(kFontHeight) + 2;
    }
    if (! differences.empty())
    {
        g.setColour(kText);
        g.drawText(options.differenceTitle, lower.getX(), lower.getY() + 4, lower.getWidth() - 8, juce::roundToInt(kFontHeight), juce::Justification::centredRight, true);
    }
    return image;
}

bool writeResponsePlotPng(const std::vector<double>& frequencies, const std::vector<PlotCurve>& curves, const std::vector<PlotCurve>& differences,
                          const ResponsePlotOptions& options, const juce::File& file)
{
    const juce::Image image = renderResponsePlot(frequencies, curves, differences, options);
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    return stream.openedOk() && png.writeImageToStream(image, stream);
}
}
