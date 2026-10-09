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

// How alike two images look independent of their size: both scaled to the size of the first (high-quality resampling), then the correlation
// coefficient of their luminance (1: the same picture; near 0: unrelated; a drawing that did not scale with its window gives a low value)
double getImageSimilarity(const juce::Image& reference, const juce::Image& other);

// W5d.3: how an editor reacts to the host's scale factor
enum class ScaleBehaviour
{
    Follows,        // size and content scale by the factor
    SizeOnly,       // the size changes by the factor, the content does not scale with it uniformly (kept at its size or re-laid out)
    Ignores,        // the size stays
    Partly,         // the size changes, but not by the factor
    NotJudged       // the capture is larger than the (virtual) screen
};

constexpr double kSizeTolerance = 0.03;         // relative: the size counts as scaled by the factor within 3 %
// content counts as scaled if the similarity is at least this: calibrated with the test editors (W5d.3): a drawing scaled by JUCE 0.99, the
// Reference EQ (fine text and 1-pixel lines) 0.89, a drawing that keeps its size in a grown window 0.0
constexpr double kSameContentSimilarity = 0.7;

ScaleBehaviour judgeScaling(double factor, double sizeRatio, double similarity);
juce::String describe(ScaleBehaviour behaviour);
}
