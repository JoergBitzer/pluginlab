#include "pluginlab/ui/Logo.h"

#include <cmath>

namespace pluginlab::ui
{
namespace
{
constexpr int kLogoHeight = 48;
// the red of the Jade Hochschule (as in the author's TGMTools/JadeLookAndFeel.h); the letters and the impulse a light gray (white was too
// bright), "Benchmark" a lighter gray than JadeGray (too dark)
const juce::Colour kLogoBackground(0xffe30613);
const juce::Colour kLogoGray(0xff8a8e86);
const juce::Colour kLogoLetters(0xffd8d8d8);
constexpr int kWaveSteps = 96;
constexpr float kSincHalfWidth = 3.0f;        // si(x) = sin(x) / x for x = -3 pi ... 3 pi: the main peak and two side lobes on each side
}

juce::Image makeLogo(int height)
{
    // a "JB" monogram (Jade Benchmark) on a JadeRed rounded square, the name beside it ("Jade" red, "Benchmark" gray)
    const int width = height * 4;
    juce::Image image(juce::Image::ARGB, width, height, true);
    juce::Graphics g(image);
    const auto size = static_cast<float>(height);
    g.setColour(kLogoBackground);
    g.fillRoundedRectangle(0.0f, 0.0f, size, size, size * 0.18f);
    g.setColour(kLogoLetters);
    g.setFont(juce::FontOptions(size * 0.56f, juce::Font::bold));
    g.drawText("JB", 0, 0, height, height * 4 / 5, juce::Justification::centred);
    // an si impulse under the letters (sin(x) / x, two side lobes on each side): the impulse response, measured
    juce::Path wave;
    const float left = size * 0.12f;
    const float right = size * 0.88f;
    const float baseline = size * 0.88f;
    const float amplitude = size * 0.19f;
    for (int step = 0; step <= kWaveSteps; ++step)
    {
        const float fraction = static_cast<float>(step) / kWaveSteps;
        const float x = juce::MathConstants<float>::pi * kSincHalfWidth * (2.0f * fraction - 1.0f);
        const float si = std::abs(x) < 1.0e-6f ? 1.0f : std::sin(x) / x;
        const float px = left + (right - left) * fraction;
        const float py = baseline - amplitude * si;
        if (step == 0)
        {
            wave.startNewSubPath(px, py);
        }
        else
        {
            wave.lineTo(px, py);
        }
    }
    g.setColour(kLogoLetters);
    g.strokePath(wave, juce::PathStrokeType(size * 0.04f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(kLogoBackground);
    g.setFont(juce::FontOptions(size * 0.34f, juce::Font::bold));
    g.drawText("Jade", height + height / 6, height / 12, width - height, height / 2, juce::Justification::centredLeft);
    g.setColour(kLogoGray);
    g.setFont(juce::FontOptions(size * 0.3f));
    g.drawText("Benchmark", height + height / 6, height / 2 - height / 24, width - height, height / 2, juce::Justification::centredLeft);
    return image;
}

bool writeLogo(const juce::File& folder, const juce::String& fileName)
{
    // always written: a folder keeps reports for years, and the logo of an older version would stay
    const juce::File file = folder.getChildFile(fileName);
    folder.createDirectory();
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    return stream.openedOk() && png.writeImageToStream(makeLogo(kLogoHeight), stream);
}
}
