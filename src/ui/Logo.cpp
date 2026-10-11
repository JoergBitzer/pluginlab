#include "pluginlab/ui/Logo.h"

#include <cmath>

namespace pluginlab::ui
{
namespace
{
constexpr int kLogoHeight = 48;
// the colours of the Jade Hochschule (as in the author's TGMTools/JadeLookAndFeel.h): JadeRed, JadeGray, JadeWhite
const juce::Colour kLogoBackground(0xffe30613);
const juce::Colour kLogoGray(0xff5b5f57);
const juce::Colour kLogoWave(0xfff2f2f2);
constexpr int kWaveSteps = 64;
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
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(size * 0.56f, juce::Font::bold));
    g.drawText("JB", 0, 0, height, height * 4 / 5, juce::Justification::centred);
    // a short sine under the letters: a signal, measured
    juce::Path wave;
    const float left = size * 0.16f;
    const float right = size * 0.84f;
    const float middle = size * 0.8f;
    const float amplitude = size * 0.06f;
    for (int step = 0; step <= kWaveSteps; ++step)
    {
        const float fraction = static_cast<float>(step) / kWaveSteps;
        const float x = left + (right - left) * fraction;
        const float y = middle - amplitude * std::sin(juce::MathConstants<float>::twoPi * fraction);
        if (step == 0)
        {
            wave.startNewSubPath(x, y);
        }
        else
        {
            wave.lineTo(x, y);
        }
    }
    g.setColour(kLogoWave);
    g.strokePath(wave, juce::PathStrokeType(size * 0.05f));
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
    const juce::File file = folder.getChildFile(fileName);
    if (file.existsAsFile())
    {
        return true;
    }
    folder.createDirectory();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    return stream.openedOk() && png.writeImageToStream(makeLogo(kLogoHeight), stream);
}
}
