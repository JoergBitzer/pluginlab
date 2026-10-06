#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace pluginlab::signals
{
// The test signals of W6.1 (docs/design/W6-plan.md). Every generator is deterministic (noise from a seed) and returns a buffer with the
// requested number of channels; levels are given in dBFS (peak or RMS, as named).

// How the channels of a multichannel signal relate to each other
enum class ChannelRelation
{
    Same,         // the same samples on every channel (L = R)
    Uncorrelated, // channel n uses its own seed (noise) or its own phase set (multitone); for deterministic signals like a sine: same as Same
    LeftOnly,     // the signal on channel 1, silence on the others
    RightOnly,    // the signal on channel 2, silence on the others
    Inverted      // channel 2 = -channel 1 (L = -R)
};

double dbToGain(double decibels);
double gainToDb(double gain);
double getPeak(const juce::AudioBuffer<float>& buffer, int channel);
double getRms(const juce::AudioBuffer<float>& buffer, int channel, int start = 0, int length = -1);

juce::AudioBuffer<float> makeSilence(int length, int channels);
juce::AudioBuffer<float> makeImpulse(int length, int position, double amplitude, int channels);
juce::AudioBuffer<float> makeStep(int length, int position, double amplitude, int channels);

// The frequency of the FFT bin nearest to the wanted one (a sine on a bin has no leakage)
double snapToBin(double frequencyHz, double sampleRate, int fftSize);

struct SineSettings
{
    double sampleRate = 48000.0;
    double frequencyHz = 1000.0;
    double levelDbfsPeak = -6.0;
    double phaseRadians = 0.0;
    int length = 48000;
    int fadeSamples = 0;   // raised-cosine fade in and out (0: none)
};
juce::AudioBuffer<float> makeSine(const SineSettings& settings, int channels, ChannelRelation relation = ChannelRelation::Same);

// Two tones: SMPTE (60 Hz and 7 kHz, 4:1), CCIF (19 and 20 kHz, 1:1), or free. The level is the peak of the sum.
struct TwoToneSettings
{
    double sampleRate = 48000.0;
    double firstHz = 60.0;
    double secondHz = 7000.0;
    double amplitudeRatio = 4.0;  // first / second
    double levelDbfsPeak = -6.0;
    int length = 48000;
};
TwoToneSettings makeSmpteSettings(double sampleRate, int length);
TwoToneSettings makeCcifSettings(double sampleRate, int length);
juce::AudioBuffer<float> makeTwoTone(const TwoToneSettings& settings, int channels, ChannelRelation relation = ChannelRelation::Same);

// Log-spaced tones with Schroeder phases (low crest factor; the form for any spacing: the tones peak one after the other over the period), each on an FFT bin of fftSize; the level is the RMS of the sum.
struct MultitoneSettings
{
    double sampleRate = 48000.0;
    double lowestHz = 30.0;
    double highestHz = 20000.0;
    int numberOfTones = 31;
    double levelDbfsRms = -20.0;
    int fftSize = 65536;          // the signal is periodic in fftSize samples
    int length = 65536;
    bool schroederPhases = true;  // false: all tones in phase at the start (cosines; the highest crest factor, for comparison)
};
juce::AudioBuffer<float> makeMultitone(const MultitoneSettings& settings, int channels, ChannelRelation relation = ChannelRelation::Same);
std::vector<double> getMultitoneFrequencies(const MultitoneSettings& settings);

enum class NoiseColour
{
    WhiteUniform,
    WhiteGaussian,
    Pink
};
struct NoiseSettings
{
    NoiseColour colour = NoiseColour::WhiteGaussian;
    double levelDbfsRms = -20.0;
    int length = 48000;
    int seed = 7;
};
juce::AudioBuffer<float> makeNoise(const NoiseSettings& settings, int channels, ChannelRelation relation = ChannelRelation::Same);

// Sine bursts with given levels, one after the other, each followed by a gap of silence (dynamics, attack/release, time-varying detection)
struct BurstSettings
{
    double sampleRate = 48000.0;
    double frequencyHz = 1000.0;
    std::vector<double> levelsDbfsPeak = {-40.0, -20.0, -6.0};
    double burstSeconds = 0.5;
    double gapSeconds = 0.5;
};
juce::AudioBuffer<float> makeBursts(const BurstSettings& settings, int channels);

// Writes the buffer as a 32-bit float WAV file. Returns false if the file cannot be written.
bool writeWav(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate);
}
