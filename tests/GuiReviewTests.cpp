#include <cmath>

#include <juce_graphics/juce_graphics.h>

#include "pluginlab/ui/GuiReview.h"

namespace
{
namespace ui = pluginlab::ui;

juce::Image makeFilled(juce::Colour colour, int width = 8, int height = 8)
{
    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics g(image);
    g.fillAll(colour);
    return image;
}

// Euclidean distance of two colours in 8-bit sRGB
double getDistance(juce::Colour a, juce::Colour b)
{
    const double red = a.getRed() - b.getRed();
    const double green = a.getGreen() - b.getGreen();
    const double blue = a.getBlue() - b.getBlue();
    return std::sqrt(red * red + green * green + blue * blue);
}
}

// W5d: the image variants of the GUI review (docs/design/W5d-gui-review.md)
class GuiReviewTests : public juce::UnitTest
{
public:
    GuiReviewTests()
        : juce::UnitTest("GUI review images", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("WCAG luminance and contrast; white stays white in every variant; grayscale keeps the luminance");
        expectWithinAbsoluteError(ui::getRelativeLuminance(juce::Colours::white), 1.0, 1.0e-9);
        expectWithinAbsoluteError(ui::getRelativeLuminance(juce::Colours::black), 0.0, 1.0e-9);
        expectWithinAbsoluteError(ui::getContrastRatio(juce::Colours::black, juce::Colours::white), 21.0, 1.0e-9);
        expectWithinAbsoluteError(ui::getRelativeLuminance(juce::Colour(255, 0, 0)), 0.2126, 1.0e-9);
        for (const ui::VisionVariant variant : ui::getAllVariants())
        {
            const juce::Colour white = ui::makeVariant(makeFilled(juce::Colours::white), variant).getPixelAt(3, 3);
            expect(getDistance(white, juce::Colours::white) <= 2.0, ui::getVariantName(variant) + ": white " + white.toDisplayString(false));
        }
        const juce::Colour orange(230, 120, 30);
        const juce::Colour gray = ui::makeVariant(makeFilled(orange), ui::VisionVariant::Grayscale).getPixelAt(1, 1);
        expect(gray.getRed() == gray.getGreen() && gray.getGreen() == gray.getBlue(), "grayscale is gray");
        expectWithinAbsoluteError(ui::getRelativeLuminance(gray), ui::getRelativeLuminance(orange), 0.005, "grayscale keeps the luminance");

        beginTest("protanopia and deuteranopia bring red and green together, tritanopia does not (Machado et al. 2009)");
        const juce::Colour red(200, 40, 40);
        const juce::Colour green(40, 160, 40);
        const double normal = getDistance(red, green);
        const auto distanceIn = [&](ui::VisionVariant variant)
        {
            return getDistance(ui::makeVariant(makeFilled(red), variant).getPixelAt(1, 1), ui::makeVariant(makeFilled(green), variant).getPixelAt(1, 1));
        };
        const double protan = distanceIn(ui::VisionVariant::Protanopia);
        const double deutan = distanceIn(ui::VisionVariant::Deuteranopia);
        const double tritan = distanceIn(ui::VisionVariant::Tritanopia);
        logMessage("red-green distance (8-bit sRGB): normal " + juce::String(normal, 1) + ", protanopia " + juce::String(protan, 1) + ", deuteranopia "
                   + juce::String(deutan, 1) + ", tritanopia " + juce::String(tritan, 1));
        expect(protan < 0.5 * normal, "protanopia");
        expect(deutan < 0.5 * normal, "deuteranopia");
        expect(tritan > 0.7 * normal, "tritanopia");

        beginTest("the low-contrast map marks an edge that only colour carries, not one with luminance contrast; content share; contact sheet");
        // left half red, right half a green of almost the same luminance; then the same with a black right half
        const auto makeEdge = [](juce::Colour left, juce::Colour right)
        {
            juce::Image image(juce::Image::ARGB, 20, 10, true);
            juce::Graphics g(image);
            g.setColour(left);
            g.fillRect(0, 0, 10, 10);
            g.setColour(right);
            g.fillRect(10, 0, 10, 10);
            return image;
        };
        const juce::Colour equalGreen(0, 125, 0);   // luminance close to that of (230, 0, 0)
        const juce::Colour pureRed(230, 0, 0);
        double share = 0.0;
        const juce::Image colourOnly = ui::makeLowContrastMap(makeEdge(pureRed, equalGreen), 1.5, share);
        logMessage("red / green contrast ratio " + juce::String(ui::getContrastRatio(pureRed, equalGreen), 2) + ", low-contrast share " + juce::String(share, 2));
        expectWithinAbsoluteError(share, 1.0, 1.0e-9, "red against an equally bright green: every edge pixel is low-contrast");
        expect(colourOnly.getPixelAt(9, 5) == juce::Colours::red, "the edge is marked");
        ui::makeLowContrastMap(makeEdge(pureRed, juce::Colours::black), 1.5, share);
        expectWithinAbsoluteError(share, 0.0, 1.0e-9, "red against black: no low-contrast edge");
        expectWithinAbsoluteError(ui::getContentShare(makeFilled(juce::Colours::black)), 0.0, 1.0e-12, "uniform image");
        expect(ui::getContentShare(makeEdge(pureRed, juce::Colours::black)) > 0.4, "half the image differs");
        const juce::Image sheet = ui::makeContactSheet({{"a", makeFilled(juce::Colours::red, 30, 20)}, {"b", makeFilled(juce::Colours::blue, 30, 20)}}, 2);
        expect(sheet.getWidth() > 60 && sheet.getHeight() > 20, "contact sheet size");
    }
};

static GuiReviewTests guiReviewTests;
