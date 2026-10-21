#include "pluginlab/engine/FixedPlayHead.h"

#include <cmath>

namespace pluginlab::engine
{
namespace
{
constexpr int kBeatsPerBar = 4;
constexpr int kBeatUnit = 4;
constexpr double kSecondsPerMinute = 60.0;
}

FixedPlayHead::FixedPlayHead(double tempoBpm, double sampleRate)
    : m_tempoBpm(tempoBpm), m_sampleRate(sampleRate)
{
}

void FixedPlayHead::setTempo(double tempoBpm)
{
    m_tempoBpm.store(tempoBpm);
}

double FixedPlayHead::getTempo() const
{
    return m_tempoBpm.load();
}

void FixedPlayHead::setSampleRate(double sampleRate)
{
    m_sampleRate.store(sampleRate);
}

void FixedPlayHead::reset()
{
    m_samples.store(0);
}

void FixedPlayHead::advance(int numSamples)
{
    m_samples.fetch_add(numSamples);
}

juce::Optional<juce::AudioPlayHead::PositionInfo> FixedPlayHead::getPosition() const
{
    const double tempo = m_tempoBpm.load();
    const double sampleRate = m_sampleRate.load();
    const juce::int64 samples = m_samples.load();
    const double seconds = static_cast<double>(samples) / sampleRate;
    const double beats = seconds * tempo / kSecondsPerMinute; // quarter notes in 4/4
    const double barStart = std::floor(beats / kBeatsPerBar) * kBeatsPerBar;

    PositionInfo position;
    position.setBpm(tempo);
    position.setTimeSignature(TimeSignature{kBeatsPerBar, kBeatUnit});
    position.setTimeInSamples(samples);
    position.setTimeInSeconds(seconds);
    position.setPpqPosition(beats);
    position.setPpqPositionOfLastBarStart(barStart);
    position.setBarCount(static_cast<juce::int64>(barStart / kBeatsPerBar));
    position.setIsPlaying(true);
    position.setIsRecording(false);
    position.setIsLooping(false);
    return position;
}
}
