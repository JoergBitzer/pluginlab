#pragma once

#include <atomic>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::engine
{
// A play head of our own (W11.1a, docs/design/W11-delay-plan.md): a tempo-synced plugin (a delay in note values, an LFO in beats) asks the host
// for the tempo and the position. The measurement rig and the application's engine have no DAW, so they give the plugins this one: a fixed
// tempo, 4/4, playing, the position counted from the samples processed. Inside a DAW the plugin product passes the DAW's play head instead.
//
// Threads: setTempo and reset from any thread; advance from the audio thread; getPosition from the audio thread (the plugin asks in its
// processBlock).
class FixedPlayHead : public juce::AudioPlayHead
{
public:
    static constexpr double kDefaultTempoBpm = 120.0;

    explicit FixedPlayHead(double tempoBpm = kDefaultTempoBpm, double sampleRate = 48000.0);

    void setTempo(double tempoBpm);
    double getTempo() const;
    void setSampleRate(double sampleRate);

    // back to the first sample (a new render, a restart of the files)
    void reset();
    // after a block of numSamples has been processed
    void advance(int numSamples);

    juce::Optional<PositionInfo> getPosition() const override;

private:
    std::atomic<double> m_tempoBpm;
    std::atomic<double> m_sampleRate;
    std::atomic<juce::int64> m_samples{0};
};
}
