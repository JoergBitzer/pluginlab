#include "pluginlab/ui/SpectrogramImage.h"

#include <algorithm>
#include <cmath>

#include "pluginlab/ui/PlasmaColourMap.h"

namespace pluginlab::ui
{
namespace
{
constexpr int kLeft = 62;            // frequency labels
constexpr int kRight = 92;           // colour bar and its labels
constexpr int kTop = 34;             // title
constexpr int kBottom = 46;          // time labels and axis name
constexpr int kColourBarWidth = 18;
constexpr int kColourBarGap = 14;
constexpr int kTick = 5;
constexpr float kFontHeight = 14.0f;
constexpr double kColourStepDb = 20.0;
const juce::Colour kBackground(0xff1e1e1e);
const juce::Colour kText(0xffe0e0e0);
const juce::Colour kAxis(0xff909090);

juce::Colour getPlasmaColour(double value, double floorDb, double ceilingDb)
{
    const double t = juce::jlimit(0.0, 1.0, (value - floorDb) / (ceilingDb - floorDb));
    const auto& rgb = kPlasma[static_cast<size_t>(juce::roundToInt(t * (kPlasma.size() - 1)))];
    return juce::Colour::fromFloatRGBA(rgb[0], rgb[1], rgb[2], 1.0f);
}

juce::String getFrequencyLabel(double hertz)
{
    if (hertz >= 1000.0)
    {
        return juce::String(juce::roundToInt(hertz / 1000.0)) + "k";
    }
    return juce::String(juce::roundToInt(hertz));
}

// the level of a pixel row (lower edge low, upper edge high, in Hz) in one frame: the maximum of the bins in it, else the interpolation between
// the two bins around its centre
float getRowLevel(const std::vector<float>& frame, double binHz, double low, double high)
{
    const double lowBin = low / binHz;
    const double highBin = high / binHz;
    const size_t last = frame.size() - 1;
    if (highBin - lowBin > 1.0)
    {
        const size_t first = std::min(static_cast<size_t>(std::ceil(lowBin)), last);
        const size_t end = std::max(first, std::min(static_cast<size_t>(std::floor(highBin)), last));
        return *std::max_element(frame.begin() + static_cast<std::ptrdiff_t>(first), frame.begin() + static_cast<std::ptrdiff_t>(end) + 1);
    }
    const double centre = std::sqrt(low * high) / binHz;
    const size_t bin = std::min(static_cast<size_t>(centre), last - 1);
    const float fraction = static_cast<float>(juce::jlimit(0.0, 1.0, centre - static_cast<double>(bin)));
    return frame[bin] + fraction * (frame[bin + 1] - frame[bin]);
}
}

juce::Image renderSpectrogramImage(const measure::Spectrogram& spectrogram, const SpectrogramImageOptions& options)
{
    juce::Image image(juce::Image::RGB, options.width, options.height, true);
    juce::Graphics g(image);
    g.fillAll(kBackground);
    g.setFont(juce::FontOptions(kFontHeight));
    const juce::Rectangle<int> plot(kLeft, kTop, options.width - kLeft - kRight, options.height - kTop - kBottom);
    if (options.title.isNotEmpty())
    {
        g.setColour(kText);
        g.drawText(options.title, plot.getX(), 0, plot.getWidth(), kTop, juce::Justification::centredLeft, true);
    }
    if (spectrogram.frames.empty() || plot.getWidth() < 2 || plot.getHeight() < 2)
    {
        return image;
    }
    const double lowest = options.lowestHz; // below the first bin the rows interpolate towards DC
    const double highest = std::min(options.highestHz, 0.5 * spectrogram.sampleRate);
    const double ratio = highest / lowest;
    const double binHz = spectrogram.getBinHz();
    const int columns = plot.getWidth();
    const int rows = plot.getHeight();
    const size_t frameCount = spectrogram.frames.size();

    // the picture: per pixel column the frames in it (their maximum: short events stay visible), per pixel row the bins in it
    {
        juce::Image::BitmapData pixels(image, juce::Image::BitmapData::writeOnly);
        std::vector<float> column(static_cast<size_t>(rows));
        for (int x = 0; x < columns; ++x)
        {
            const size_t firstFrame = std::min(frameCount - 1, static_cast<size_t>(static_cast<double>(x) * frameCount / columns));
            const size_t endFrame = std::max(firstFrame + 1, std::min(frameCount, static_cast<size_t>(static_cast<double>(x + 1) * frameCount / columns)));
            std::fill(column.begin(), column.end(), -1000.0f);
            for (size_t frame = firstFrame; frame < endFrame; ++frame)
            {
                for (int row = 0; row < rows; ++row)
                {
                    const double low = lowest * std::pow(ratio, static_cast<double>(row) / rows);
                    const double high = lowest * std::pow(ratio, static_cast<double>(row + 1) / rows);
                    column[static_cast<size_t>(row)] = std::max(column[static_cast<size_t>(row)], getRowLevel(spectrogram.frames[frame], binHz, low, high));
                }
            }
            for (int row = 0; row < rows; ++row)
            {
                pixels.setPixelColour(plot.getX() + x, plot.getBottom() - 1 - row, getPlasmaColour(column[static_cast<size_t>(row)], options.floorDb, options.ceilingDb));
            }
        }
    }

    // frequency axis: 1-2-5 per decade
    g.setColour(kAxis);
    g.drawRect(plot.expanded(1));
    for (double decade = 10.0; decade <= highest; decade *= 10.0)
    {
        for (const double multiple : {1.0, 2.0, 5.0})
        {
            const double hertz = decade * multiple;
            if (hertz < lowest * 0.999 || hertz > highest * 1.001)
            {
                continue;
            }
            const float y = static_cast<float>(plot.getBottom()) - static_cast<float>(rows * std::log(hertz / lowest) / std::log(ratio));
            g.setColour(kAxis);
            g.drawLine(static_cast<float>(plot.getX() - kTick), y, static_cast<float>(plot.getX()), y);
            g.setColour(kText);
            g.drawText(getFrequencyLabel(hertz), 0, juce::roundToInt(y - kFontHeight / 2.0f), plot.getX() - kTick - 4, juce::roundToInt(kFontHeight),
                       juce::Justification::centredRight, false);
        }
    }
    g.saveState();
    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi, 12.0f, static_cast<float>(plot.getCentreY())));
    g.drawText("frequency in Hz", -88, plot.getCentreY() - 8, 200, 16, juce::Justification::centred, false);
    g.restoreState();

    // time axis: steps of 0.1, 0.2, 0.5, 1, 2, 5 s, at most about ten labels
    const double seconds = spectrogram.getSeconds();
    double step = 5.0;
    for (const double candidate : {0.1, 0.2, 0.5, 1.0, 2.0, 5.0})
    {
        if (seconds / candidate <= 10.0)
        {
            step = candidate;
            break;
        }
    }
    int decimals = 0;
    if (step < 1.0)
    {
        decimals = 1;
    }
    else
    {
        decimals = 0;
    }
    for (double t = 0.0; t <= seconds + 1.0e-9; t += step)
    {
        const float x = static_cast<float>(plot.getX()) + static_cast<float>(columns * t / seconds);
        g.setColour(kAxis);
        g.drawLine(x, static_cast<float>(plot.getBottom()), x, static_cast<float>(plot.getBottom() + kTick));
        g.setColour(kText);
        g.drawText(juce::String(t, decimals), juce::roundToInt(x) - 30, plot.getBottom() + kTick, 60, juce::roundToInt(kFontHeight),
                   juce::Justification::centred, false);
    }
    g.drawText("time in s", plot.getX(), options.height - juce::roundToInt(kFontHeight) - 4, plot.getWidth(), juce::roundToInt(kFontHeight),
               juce::Justification::centred, false);

    // colour bar with its levels
    const juce::Rectangle<int> bar(plot.getRight() + kColourBarGap, plot.getY(), kColourBarWidth, rows);
    for (int row = 0; row < rows; ++row)
    {
        const double level = options.floorDb + (options.ceilingDb - options.floorDb) * (row + 0.5) / rows;
        g.setColour(getPlasmaColour(level, options.floorDb, options.ceilingDb));
        g.fillRect(bar.getX(), bar.getBottom() - 1 - row, bar.getWidth(), 1);
    }
    g.setColour(kAxis);
    g.drawRect(bar.expanded(1));
    for (double level = options.floorDb; level <= options.ceilingDb + 1.0e-9; level += kColourStepDb)
    {
        const float y = static_cast<float>(bar.getBottom()) - static_cast<float>(rows * (level - options.floorDb) / (options.ceilingDb - options.floorDb));
        g.setColour(kText);
        g.drawText(juce::String(juce::roundToInt(level)), bar.getRight() + 4, juce::roundToInt(y - kFontHeight / 2.0f), kRight - kColourBarGap - kColourBarWidth - 4,
                   juce::roundToInt(kFontHeight), juce::Justification::centredLeft, false);
    }
    g.drawText("dB", bar.getX() - 4, plot.getY() - juce::roundToInt(kFontHeight) - 4, kColourBarWidth + 8, juce::roundToInt(kFontHeight), juce::Justification::centred, false);
    return image;
}

bool writeSpectrogramPng(const measure::Spectrogram& spectrogram, const SpectrogramImageOptions& options, const juce::File& file)
{
    const juce::Image image = renderSpectrogramImage(spectrogram, options);
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    return stream.openedOk() && png.writeImageToStream(image, stream);
}
}
