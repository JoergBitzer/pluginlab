#pragma once

#include <juce_graphics/juce_graphics.h>

#include "pluginlab/measure/Spectrogram.h"

namespace pluginlab::ui
{
// A spectrogram as a picture for the reports, drawn as JadeSpectrogram2 draws it (without its overlays): logarithmic frequency axis with 1-2-5
// labels per decade, a pixel row that covers several bins shows their maximum (narrow lines stay visible) and one that covers less than a bin
// interpolates; time axis in seconds; colour map plasma with its colour bar in dB.
struct SpectrogramImageOptions
{
    int width = 1000;
    int height = 460;
    double lowestHz = 20.0;
    double highestHz = 20000.0;     // at most half the sample rate
    double floorDb = -120.0;        // the colour range
    double ceilingDb = 0.0;
    juce::String title;
};

juce::Image renderSpectrogramImage(const measure::Spectrogram& spectrogram, const SpectrogramImageOptions& options);

// The picture as a PNG file; false if it cannot be written
bool writeSpectrogramPng(const measure::Spectrogram& spectrogram, const SpectrogramImageOptions& options, const juce::File& file);
}
