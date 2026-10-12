#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Spectrogram.h"
#include "pluginlab/ui/SpectrogramImage.h"

namespace
{
namespace measure = pluginlab::measure;

constexpr double kSampleRate = 48000.0;
constexpr double kBinCentredHz = 1500.0;    // 2048 samples at 48 kHz: bin 64 exactly (no scalloping)
constexpr double kAmplitude = 0.5;          // -6.02 dB re full scale
constexpr double kLevelToleranceDb = 0.05;
}

// The spectrogram of the fingerprint report (docs/measurements/spectrogram.md): its level scale, the two resolutions, the sweep and the picture
class MeasureSpectrogramTests : public juce::UnitTest
{
public:
    MeasureSpectrogramTests()
        : juce::UnitTest("Spectrogram", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("a sine on a bin reads its level in dB re full scale, with one and with two resolutions, Hann and Blackman-Harris");
        std::vector<float> sine(static_cast<size_t>(kSampleRate));
        for (size_t n = 0; n < sine.size(); ++n)
        {
            sine[n] = static_cast<float>(kAmplitude * std::sin(2.0 * juce::MathConstants<double>::pi * kBinCentredHz * static_cast<double>(n) / kSampleRate));
        }
        const double expected = 20.0 * std::log10(kAmplitude);
        for (const measure::SpectrogramWindow window : {measure::SpectrogramWindow::Hann, measure::SpectrogramWindow::BlackmanHarris})
        {
            for (const int lowSize : {0, 8192})
            {
                measure::SpectrogramSettings settings;
                settings.window = window;
                settings.lowFrequencyFftSize = lowSize;
                const measure::Spectrogram spectrogram = measure::computeSpectrogram(sine, kSampleRate, settings);
                const std::vector<float>& middle = spectrogram.frames[spectrogram.frames.size() / 2];
                const size_t bin = static_cast<size_t>(std::lround(kBinCentredHz / spectrogram.getBinHz()));
                const juce::String what = juce::String(measure::getSpectrogramWindowName(window)) + (lowSize > 0 ? ", two resolutions" : "");
                expectWithinAbsoluteError(static_cast<double>(middle[bin]), expected, kLevelToleranceDb, what);
                expect(middle[bin / 2] < expected - 80.0, what + ": an octave below is empty (" + juce::String(middle[bin / 2]) + " dB)");
                expectEquals(spectrogram.hopSize, 512, what + ": hop of the short frame at 75 % overlap");
            }
        }

        beginTest("the synchronized sweep: its length and level");
        const std::vector<float> sweep = measure::makeSynchronizedSweep(kSampleRate, 20.0, 20000.0, 3.0, -6.0);
        expectWithinAbsoluteError(static_cast<double>(sweep.size()) / kSampleRate, 3.0, 0.15, "about the length asked for (L is rounded)");
        float peak = 0.0f;
        for (const float value : sweep)
        {
            peak = std::max(peak, std::abs(value));
        }
        expectWithinAbsoluteError(20.0 * std::log10(peak), -6.0, 0.01);

        beginTest("the picture: the size asked for, and the sweep in it");
        measure::SpectrogramSettings settings;
        settings.lowFrequencyFftSize = 8192;
        const measure::Spectrogram spectrogram = measure::computeSpectrogram(sweep, kSampleRate, settings);
        pluginlab::ui::SpectrogramImageOptions options;
        const juce::Image image = pluginlab::ui::renderSpectrogramImage(spectrogram, options);
        expectEquals(image.getWidth(), options.width);
        expectEquals(image.getHeight(), options.height);
        // the plot's centre column must hold bright (yellow) and dark pixels: the sweep and the empty rest
        bool bright = false;
        bool dark = false;
        for (int y = 40; y < options.height - 50; ++y)
        {
            const juce::Colour colour = image.getPixelAt(options.width / 2, y);
            bright = bright || (colour.getRed() > 200 && colour.getGreen() > 150);
            dark = dark || colour.getBrightness() < 0.6f;
        }
        expect(bright && dark, "the sweep shows as a bright line on a dark background");
    }
};

static MeasureSpectrogramTests measureSpectrogramTests;
