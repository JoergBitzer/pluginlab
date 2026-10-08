#include "SvgPlot.h"

#include <cmath>

namespace
{
constexpr double kWidth = 720.0;
constexpr double kHeight = 420.0;
constexpr double kLeft = 70.0;
constexpr double kRight = 24.0;
constexpr double kTitleBottom = 48.0;        // baseline of the first legend row
constexpr double kLegendRowHeight = 18.0;
constexpr double kGapBelowLegend = 22.0;
constexpr double kBottom = 54.0;
constexpr int kMaximumSeries = 4;
constexpr int kMaximumYTicks = 8;
constexpr double kLegendKey = 22.0;
constexpr double kLegendGap = 18.0;
constexpr double kAverageCharacterWidth = 6.6;

// the chart palette (light mode): surface, ink, grid, and the first four categorical slots in their fixed order
const juce::String kSurface = "#fcfcfb";
const juce::String kPrimaryInk = "#0b0b0b";
const juce::String kSecondaryInk = "#52514e";
const juce::String kMutedInk = "#898781";
const juce::String kGridline = "#e1e0d9";
const juce::String kBaseline = "#c3c2b7";
const juce::String kSeriesColours[kMaximumSeries] = {"#2a78d6", "#eb6834", "#1baf7a", "#eda100"};
const juce::String kFont = "font-family=\"Helvetica, Arial, sans-serif\"";

juce::String number(double value)
{
    return juce::String(value, 2);
}

juce::String escape(const juce::String& text)
{
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
}

juce::String text(double x, double y, const juce::String& content, const juce::String& colour, int size, const juce::String& anchor,
                  const juce::String& extra = {})
{
    return "<text x=\"" + number(x) + "\" y=\"" + number(y) + "\" " + kFont + " font-size=\"" + juce::String(size) + "\" fill=\"" + colour
           + "\" text-anchor=\"" + anchor + "\"" + extra + ">" + escape(content) + "</text>\n";
}

juce::String line(double x1, double y1, double x2, double y2, const juce::String& colour, double width, const juce::String& extra = {})
{
    return "<line x1=\"" + number(x1) + "\" y1=\"" + number(y1) + "\" x2=\"" + number(x2) + "\" y2=\"" + number(y2) + "\" stroke=\"" + colour
           + "\" stroke-width=\"" + number(width) + "\"" + extra + "/>\n";
}
}

juce::String formatTick(double value)
{
    if (std::abs(value) >= 1000.0)
    {
        const double thousands = value / 1000.0;
        if (std::abs(thousands - std::round(thousands)) < 1.0e-9)
        {
            return juce::String(static_cast<int>(std::round(thousands))) + "k";
        }
        return juce::String(thousands, 1) + "k";
    }
    if (std::abs(value - std::round(value)) < 1.0e-9)
    {
        return juce::String(static_cast<int>(std::round(value)));
    }
    // as few decimals as the value needs (0.05, not 0.050)
    juce::String text = juce::String(value, 3);
    while (text.endsWithChar('0'))
    {
        text = text.dropLastCharacters(1);
    }
    return text;
}

SvgPlot::SvgPlot(const juce::String& title, const juce::String& xLabel, const juce::String& yLabel)
    : m_title(title), m_xLabel(xLabel), m_yLabel(yLabel)
{
}

void SvgPlot::setLogX(bool logX)
{
    m_logX = logX;
}

void SvgPlot::setXRange(double minimum, double maximum)
{
    m_xMin = minimum;
    m_xMax = maximum;
}

void SvgPlot::setYRange(double minimum, double maximum)
{
    m_yMin = minimum;
    m_yMax = maximum;
}

void SvgPlot::addSeries(const Series& series)
{
    jassert(static_cast<int>(m_series.size()) < kMaximumSeries); // more series: split the chart
    m_series.push_back(series);
}

double SvgPlot::mapX(double x) const
{
    double position = (x - m_xMin) / (m_xMax - m_xMin);
    if (m_logX)
    {
        position = std::log(x / m_xMin) / std::log(m_xMax / m_xMin);
    }
    return kLeft + position * (kWidth - kLeft - kRight);
}

// The legend in rows above the plot: the x of every entry and its row; a row is full when the next entry would pass the right edge
std::vector<std::pair<double, int>> SvgPlot::layoutLegend() const
{
    std::vector<std::pair<double, int>> places;
    if (m_series.size() < 2)
    {
        return places;
    }
    double x = kLeft;
    int row = 0;
    for (const Series& series : m_series)
    {
        const double width = kLegendKey + 6.0 + series.name.length() * kAverageCharacterWidth;
        if (x > kLeft && x + width > kWidth - kRight)
        {
            x = kLeft;
            ++row;
        }
        places.emplace_back(x, row);
        x += width + kLegendGap;
    }
    return places;
}

double SvgPlot::getPlotTop() const
{
    const std::vector<std::pair<double, int>> places = layoutLegend();
    int rows = 0;
    if (!places.empty())
    {
        rows = places.back().second + 1;
    }
    return kTitleBottom + (rows - 1) * kLegendRowHeight + kGapBelowLegend;
}

double SvgPlot::mapY(double y) const
{
    const double position = (y - m_yMin) / (m_yMax - m_yMin);
    return kHeight - kBottom - position * (kHeight - getPlotTop() - kBottom);
}

std::vector<double> SvgPlot::getXTicks() const
{
    std::vector<double> ticks;
    if (m_logX)
    {
        for (double decade = std::pow(10.0, std::floor(std::log10(m_xMin))); decade <= m_xMax; decade *= 10.0)
        {
            for (const double factor : {1.0, 2.0, 5.0})
            {
                const double tick = decade * factor;
                if (tick >= m_xMin * 0.999 && tick <= m_xMax * 1.001)
                {
                    ticks.push_back(tick);
                }
            }
        }
        return ticks;
    }
    const double span = m_xMax - m_xMin;
    double step = std::pow(10.0, std::floor(std::log10(span)));
    while (span / step < 4.0)
    {
        step /= 2.0;
    }
    for (double tick = std::ceil(m_xMin / step) * step; tick <= m_xMax + step * 1.0e-9; tick += step)
    {
        ticks.push_back(tick);
    }
    return ticks;
}

std::vector<double> SvgPlot::getYTicks() const
{
    const double span = m_yMax - m_yMin;
    double step = std::pow(10.0, std::ceil(std::log10(span / kMaximumYTicks)));
    for (const double candidate : {step / 5.0, step / 2.0, step})
    {
        if (span / candidate <= kMaximumYTicks)
        {
            step = candidate;
            break;
        }
    }
    std::vector<double> ticks;
    for (double tick = std::ceil(m_yMin / step) * step; tick <= m_yMax + step * 1.0e-9; tick += step)
    {
        double value = tick;
        if (std::abs(value) < step * 1.0e-9)
        {
            value = 0.0; // no "-0"
        }
        ticks.push_back(value);
    }
    return ticks;
}

juce::String SvgPlot::render() const
{
    juce::String svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << kWidth << "\" height=\"" << kHeight << "\" viewBox=\"0 0 " << kWidth << " " << kHeight
        << "\" role=\"img\" aria-label=\"" << escape(m_title) << "\">\n";
    svg << "<rect width=\"100%\" height=\"100%\" fill=\"" << kSurface << "\"/>\n";
    svg << text(kLeft, 26.0, m_title, kPrimaryInk, 15, "start", " font-weight=\"bold\"");

    // grid, ticks, axis labels
    const double plotLeft = kLeft;
    const double plotRight = kWidth - kRight;
    const double plotTop = getPlotTop();
    const double plotBottom = kHeight - kBottom;
    for (const double tick : getXTicks())
    {
        const double x = mapX(tick);
        svg << line(x, plotTop, x, plotBottom, kGridline, 1.0);
        svg << text(x, plotBottom + 16.0, formatTick(tick), kMutedInk, 11, "middle");
    }
    for (const double tick : getYTicks())
    {
        const double y = mapY(tick);
        svg << line(plotLeft, y, plotRight, y, kGridline, 1.0);
        svg << text(plotLeft - 6.0, y + 4.0, formatTick(tick), kMutedInk, 11, "end");
    }
    svg << line(plotLeft, plotBottom, plotRight, plotBottom, kBaseline, 1.0);
    svg << line(plotLeft, plotTop, plotLeft, plotBottom, kBaseline, 1.0);
    svg << text((plotLeft + plotRight) / 2.0, kHeight - 12.0, m_xLabel, kSecondaryInk, 12, "middle");
    svg << text(18.0, (plotTop + plotBottom) / 2.0, m_yLabel, kSecondaryInk, 12, "middle",
                " transform=\"rotate(-90 18 " + number((plotTop + plotBottom) / 2.0) + ")\"");

    // the series, clipped to the plot area
    svg << "<clipPath id=\"plot\"><rect x=\"" << number(plotLeft) << "\" y=\"" << number(plotTop) << "\" width=\"" << number(plotRight - plotLeft)
        << "\" height=\"" << number(plotBottom - plotTop) << "\"/></clipPath>\n";
    for (size_t index = 0; index < m_series.size(); ++index)
    {
        const Series& series = m_series[index];
        juce::String points;
        for (size_t point = 0; point < series.x.size() && point < series.y.size(); ++point)
        {
            if (!std::isfinite(series.y[point]))
            {
                continue;
            }
            points << number(mapX(series.x[point])) << "," << number(mapY(series.y[point])) << " ";
        }
        juce::String dash;
        if (series.dashed)
        {
            dash = " stroke-dasharray=\"6 4\"";
        }
        svg << "<polyline clip-path=\"url(#plot)\" fill=\"none\" stroke=\"" << kSeriesColours[index] << "\" stroke-width=\"2\" stroke-linejoin=\"round\" "
            << "stroke-linecap=\"round\"" << dash << " points=\"" << points.trim() << "\"/>\n";
    }

    // legend in rows above the plot (only for two or more series; the title names a single one)
    const std::vector<std::pair<double, int>> places = layoutLegend();
    for (size_t index = 0; index < places.size(); ++index)
    {
        const double x = places[index].first;
        const double y = kTitleBottom + places[index].second * kLegendRowHeight;
        juce::String dash;
        if (m_series[index].dashed)
        {
            dash = " stroke-dasharray=\"6 4\"";
        }
        svg << line(x, y - 4.0, x + kLegendKey, y - 4.0, kSeriesColours[index], 2.0, dash);
        svg << text(x + kLegendKey + 6.0, y, m_series[index].name, kSecondaryInk, 12, "start");
    }
    svg << "</svg>\n";
    return svg;
}

bool SvgPlot::write(const juce::File& file) const
{
    return file.replaceWithText(render());
}
