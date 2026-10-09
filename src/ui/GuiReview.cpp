#include "pluginlab/ui/GuiReview.h"

#include <array>
#include <cmath>

namespace pluginlab::ui
{
namespace
{
using Matrix = std::array<std::array<double, 3>, 3>;

// Machado, Oliveira, Fernandes, "A physiologically-based model for simulation of color vision deficiency", IEEE TVCG 15(6), 2009: the matrices for
// severity 1.0, applied to linear RGB
constexpr Matrix kProtanopia = {{{0.152286, 1.052583, -0.204868}, {0.114503, 0.786281, 0.099216}, {-0.003882, -0.048116, 1.051998}}};
constexpr Matrix kDeuteranopia = {{{0.367322, 0.860646, -0.227968}, {0.280085, 0.672501, 0.047413}, {-0.011820, 0.042940, 0.968881}}};
constexpr Matrix kTritanopia = {{{1.255528, -0.076749, -0.178779}, {-0.078411, 0.930809, 0.147602}, {0.004733, 0.691367, 0.303900}}};

// BT.709 / sRGB luminance weights (WCAG 2.x)
constexpr double kRedWeight = 0.2126;
constexpr double kGreenWeight = 0.7152;
constexpr double kBlueWeight = 0.0722;
constexpr double kColourEdge = 0.12;          // a clear colour change: Euclidean distance in linear RGB
constexpr int kLabelHeight = 22;
constexpr int kGap = 12;

double toLinear(double channel)
{
    if (channel <= 0.04045)
    {
        return channel / 12.92;
    }
    return std::pow((channel + 0.055) / 1.055, 2.4);
}

double toSrgb(double channel)
{
    const double clamped = juce::jlimit(0.0, 1.0, channel);
    if (clamped <= 0.0031308)
    {
        return 12.92 * clamped;
    }
    return 1.055 * std::pow(clamped, 1.0 / 2.4) - 0.055;
}

std::array<double, 3> getLinear(juce::Colour colour)
{
    return {toLinear(colour.getFloatRed()), toLinear(colour.getFloatGreen()), toLinear(colour.getFloatBlue())};
}

juce::Colour fromLinear(const std::array<double, 3>& rgb, juce::uint8 alpha)
{
    const auto toByte = [](double value) { return static_cast<juce::uint8>(juce::roundToInt(255.0 * toSrgb(value))); };
    return juce::Colour(toByte(rgb[0]), toByte(rgb[1]), toByte(rgb[2]), alpha);
}

juce::Colour transform(juce::Colour colour, const Matrix& matrix)
{
    const std::array<double, 3> rgb = getLinear(colour);
    std::array<double, 3> result{};
    for (size_t row = 0; row < 3; ++row)
    {
        result[row] = matrix[row][0] * rgb[0] + matrix[row][1] * rgb[1] + matrix[row][2] * rgb[2];
    }
    return fromLinear(result, colour.getAlpha());
}

double getLinearDistance(juce::Colour first, juce::Colour second)
{
    const std::array<double, 3> a = getLinear(first);
    const std::array<double, 3> b = getLinear(second);
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

template <typename Function>
juce::Image mapPixels(const juce::Image& source, Function function)
{
    juce::Image result(juce::Image::ARGB, source.getWidth(), source.getHeight(), true);
    for (int y = 0; y < source.getHeight(); ++y)
    {
        for (int x = 0; x < source.getWidth(); ++x)
        {
            result.setPixelAt(x, y, function(source.getPixelAt(x, y)));
        }
    }
    return result;
}
}

juce::String getVariantName(VisionVariant variant)
{
    switch (variant)
    {
        case VisionVariant::Original:
            return "original";
        case VisionVariant::Grayscale:
            return "grayscale";
        case VisionVariant::Protanopia:
            return "protanopia";
        case VisionVariant::Deuteranopia:
            return "deuteranopia";
        case VisionVariant::Tritanopia:
            return "tritanopia";
    }
    return {};
}

std::vector<VisionVariant> getAllVariants()
{
    return {VisionVariant::Original, VisionVariant::Grayscale, VisionVariant::Protanopia, VisionVariant::Deuteranopia, VisionVariant::Tritanopia};
}

double getRelativeLuminance(juce::Colour colour)
{
    const std::array<double, 3> rgb = getLinear(colour);
    return kRedWeight * rgb[0] + kGreenWeight * rgb[1] + kBlueWeight * rgb[2];
}

double getContrastRatio(juce::Colour first, juce::Colour second)
{
    const double a = getRelativeLuminance(first);
    const double b = getRelativeLuminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

juce::Image makeVariant(const juce::Image& source, VisionVariant variant)
{
    switch (variant)
    {
        case VisionVariant::Original:
            return source.createCopy();
        case VisionVariant::Grayscale:
            return mapPixels(source, [](juce::Colour colour)
                             {
                                 const double luminance = getRelativeLuminance(colour);
                                 return fromLinear({luminance, luminance, luminance}, colour.getAlpha());
                             });
        case VisionVariant::Protanopia:
            return mapPixels(source, [](juce::Colour colour) { return transform(colour, kProtanopia); });
        case VisionVariant::Deuteranopia:
            return mapPixels(source, [](juce::Colour colour) { return transform(colour, kDeuteranopia); });
        case VisionVariant::Tritanopia:
            return mapPixels(source, [](juce::Colour colour) { return transform(colour, kTritanopia); });
    }
    return source.createCopy();
}

juce::Image makeLowContrastMap(const juce::Image& source, double minimumRatio, double& lowContrastShare)
{
    juce::Image result = mapPixels(source, [](juce::Colour colour) { return colour.withMultipliedBrightness(0.35f); });
    int edges = 0;
    int lowContrast = 0;
    for (int y = 0; y + 1 < source.getHeight(); ++y)
    {
        for (int x = 0; x + 1 < source.getWidth(); ++x)
        {
            const juce::Colour here = source.getPixelAt(x, y);
            for (const juce::Colour neighbour : {source.getPixelAt(x + 1, y), source.getPixelAt(x, y + 1)})
            {
                if (getLinearDistance(here, neighbour) < kColourEdge)
                {
                    continue;
                }
                ++edges;
                if (getContrastRatio(here, neighbour) < minimumRatio)
                {
                    ++lowContrast;
                    result.setPixelAt(x, y, juce::Colours::red);
                }
            }
        }
    }
    lowContrastShare = 0.0;
    if (edges > 0)
    {
        lowContrastShare = static_cast<double>(lowContrast) / edges;
    }
    return result;
}

juce::Image makeContactSheet(const std::vector<std::pair<juce::String, juce::Image>>& tiles, int columns)
{
    if (tiles.empty() || columns < 1)
    {
        return {};
    }
    int tileWidth = 0;
    int tileHeight = 0;
    for (const auto& [label, image] : tiles)
    {
        tileWidth = std::max(tileWidth, image.getWidth());
        tileHeight = std::max(tileHeight, image.getHeight());
    }
    const int count = static_cast<int>(tiles.size());
    const int rows = (count + columns - 1) / columns;
    const int cellWidth = tileWidth + kGap;
    const int cellHeight = tileHeight + kLabelHeight + kGap;
    juce::Image sheet(juce::Image::RGB, columns * cellWidth + kGap, rows * cellHeight + kGap, true);
    juce::Graphics g(sheet);
    g.fillAll(juce::Colour(0xff808080));
    g.setFont(juce::FontOptions(15.0f));
    for (int index = 0; index < count; ++index)
    {
        const int x = kGap + (index % columns) * cellWidth;
        const int y = kGap + (index / columns) * cellHeight;
        const auto& [label, image] = tiles[static_cast<size_t>(index)];
        g.drawImageAt(image, x, y);
        g.setColour(juce::Colours::white);
        g.drawText(label, x, y + tileHeight + 2, tileWidth, kLabelHeight - 4, juce::Justification::centredLeft);
    }
    return sheet;
}

double getContentShare(const juce::Image& image)
{
    if (! image.isValid() || image.getWidth() == 0 || image.getHeight() == 0)
    {
        return 0.0;
    }
    const juce::Colour first = image.getPixelAt(0, 0);
    juce::int64 different = 0;
    for (int y = 0; y < image.getHeight(); ++y)
    {
        for (int x = 0; x < image.getWidth(); ++x)
        {
            if (image.getPixelAt(x, y) != first)
            {
                ++different;
            }
        }
    }
    return static_cast<double>(different) / (static_cast<double>(image.getWidth()) * image.getHeight());
}
}
