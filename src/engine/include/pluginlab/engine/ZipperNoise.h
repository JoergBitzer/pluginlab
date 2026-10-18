#pragma once

#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include "pluginlab/engine/FingerprintSettings.h"

namespace pluginlab::engine
{
// Zipper noise per parameter (docs/reference/zipper-noise.md): while a parameter moves, a plugin that applies each new value at once
// changes its output in steps; every step is a click, a quick succession of them a "zipper". A host delivers automation and knob moves in
// steps (here once per block of 512 samples at 48 kHz, as a DAW without sample-accurate automation does); a plugin that smooths its
// parameters turns the steps into a continuous change. The measurement: steady tones through the plugin while the parameter ramps from
// the low to the high test position, one new value per block; the energy that appears away from the tones (and their harmonics) during
// the ramp, relative to the tones, against the same at the two ends held still (the plugin's own noise and distortion).

// The level of a signal away from its tones: the spectrum (Blackman-Harris window, no leakage from the tones beyond a few bins) summed
// outside guardHz around each tone and its harmonics up to maximumHarmonic, between lowestHz and highestHz, relative to the tones' power
struct ToneResidual
{
    double tonesDbfs = -200.0;    // the power within the guards of the fundamentals
    double residualDbfs = -200.0; // the power outside all guards
    double relativeDb = -200.0;   // residual re tones
};

ToneResidual measureToneResidual(const std::vector<float>& signal, double sampleRate, const std::vector<double>& toneHz, double guardHz,
                                 int maximumHarmonic = 5, double lowestHz = 20.0, double highestHz = 20000.0);

// The tones of the test: on bins of the analysis length, a few per decade over the audio band
std::vector<double> getZipperTones(double sampleRate, int analysisLength);

struct ZipperParameterResult
{
    int index = 0;
    juce::String name;
    bool judged = false;
    juce::String note;            // why it is not judged
    double changeDb = -200.0;     // low against high, both held still: how much the parameter changes the tones (relative, dB)
    double rampDb = -200.0;       // the residual during the ramp, re the tones
    double stillDb = -200.0;      // the larger residual of the two ends held still
    double excessDb = 0.0;        // rampDb - stillDb
    bool zipper = false;          // excess above zipperExcessDb and the ramp's residual above zipperAboveDb
};

struct ZipperFingerprint
{
    bool measured = false;
    int blockSize = 0;            // one new value per block
    double rampSeconds = 0.0;
    std::vector<double> tonesHz;
    std::vector<ZipperParameterResult> parameters;
    int zipperCount = 0;
};

// The measurement on its own (the fingerprint runs it for every continuous parameter that changes the audio, with the others at the setting
// B): each of `indices` ramps from settings.lowSetting to settings.highSetting with the others at `base`
ZipperFingerprint measureZipperNoise(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const std::vector<float>& base,
                                     const std::vector<int>& indices, const FingerprintSettings& settings);
}
