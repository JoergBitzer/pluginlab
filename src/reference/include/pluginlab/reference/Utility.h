#pragma once

#include <memory>
#include <vector>

#include "pluginlab/reference/Processor.h"
#include "pluginlab/signals/Signals.h"

namespace pluginlab::reference
{
// Gain in dB, optionally with inverted polarity: y = +-g x
class Gain : public Processor
{
public:
    Gain(double gainDb, bool invertPolarity);

    double getFactor() const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    double m_factor;
};

// A 2 x 2 channel matrix: L' = ll L + lr R, R' = rl L + rr R (a buffer with one channel is left unchanged)
class ChannelMatrix : public Processor
{
public:
    ChannelMatrix(double ll, double lr, double rl, double rr);

    // Crosstalk of the given level (dB) from each channel into the other: ll = rr = 1, lr = rl = 10^(dB/20)
    static ChannelMatrix makeCrosstalk(double crosstalkDb);
    // Stereo width w (M/S): M = (L + R)/2, S = w (L - R)/2, L' = M + S, R' = M - S; w = 0 mono, 1 unchanged, > 1 wider
    static ChannelMatrix makeWidth(double width);

    double getLeftFromLeft() const;
    double getLeftFromRight() const;
    double getRightFromLeft() const;
    double getRightFromRight() const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    double m_ll;
    double m_lr;
    double m_rl;
    double m_rr;
};

// Adds a constant: y = x + offset
class DcOffset : public Processor
{
public:
    explicit DcOffset(double offset);

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    double m_offset;
};

// Adds hum: a sine of the mains frequency (50 or 60 Hz) at a peak level, plus harmonics at levels relative to it, all starting with sine phase 0;
// the same on every channel
class HumAdder : public Processor
{
public:
    HumAdder(double sampleRate, double frequencyHz, double levelDbfsPeak, std::vector<double> harmonicLevelsDb = {});

    // The hum at a sample (counted from the last reset)
    double getHum(juce::int64 sample) const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    double m_sampleRate;
    double m_frequencyHz;
    std::vector<double> m_amplitudes; // fundamental, 2nd, 3rd, ...
    juce::int64 m_position = 0;
};

// Adds noise of a colour at an RMS level (theoretical: the raw generator scaled by its known RMS), every channel with its own seed
// (seed + 4 n, as makeNoise for uncorrelated channels)
class NoiseAdder : public Processor
{
public:
    NoiseAdder(signals::NoiseColour colour, double levelDbfsRms, int seed);

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    signals::NoiseColour m_colour;
    double m_scale;
    int m_seed;
    std::vector<std::unique_ptr<signals::NoiseGenerator>> m_generators;
};

// Tremolo: y = g(n) x with g(n) = 1 - depth (1 - cos(2 pi rate n / fs)) / 2, between 1 - depth and 1, starting at 1; the same on every channel
class Tremolo : public Processor
{
public:
    Tremolo(double sampleRate, double rateHz, double depth);

    // The gain at a sample (counted from the last reset)
    double getGain(juce::int64 sample) const;

    void reset() override;
    void process(juce::AudioBuffer<float>& buffer) override;

private:
    double m_sampleRate;
    double m_rateHz;
    double m_depth;
    juce::int64 m_position = 0;
};
}
