#pragma once

#include <utility>
#include <vector>

#include <juce_graphics/juce_graphics.h>

namespace pluginlab::ui
{
// Image variants that make GUI problems visible (W5d, docs/design/W5d-gui-review.md). All work on sRGB images; the colour transforms are done in
// linear RGB.
enum class VisionVariant
{
    Original,
    Grayscale,       // relative luminance (WCAG 2.x / ITU-R BT.709 weights): what is left without colour
    Protanopia,      // the simulations of Machado, Oliveira and Fernandes (2009), severity 1.0
    Deuteranopia,
    Tritanopia
};

juce::String getVariantName(VisionVariant variant);
std::vector<VisionVariant> getAllVariants();

juce::Image makeVariant(const juce::Image& source, VisionVariant variant);

// WCAG 2.x: relative luminance of an sRGB colour (0 ... 1) and the contrast ratio (L1 + 0.05) / (L2 + 0.05) of two colours (1 ... 21)
double getRelativeLuminance(juce::Colour colour);
double getContrastRatio(juce::Colour first, juce::Colour second);

// A hint, not a verdict: pixels where the colour changes clearly to a neighbour (right or below) but the luminance contrast of the two is below
// minimumRatio are marked red on a dimmed copy of the image (edges that only colour carries); also returns their share of all edge pixels
juce::Image makeLowContrastMap(const juce::Image& source, double minimumRatio, double& lowContrastShare);

// Tiles side by side (columns per row), each with its label below, on a neutral background
juce::Image makeContactSheet(const std::vector<std::pair<juce::String, juce::Image>>& tiles, int columns);

// The share of pixels that differ from the first pixel (0: a uniform image, e.g. a capture that got nothing)
double getContentShare(const juce::Image& image);
}
