#include "pluginlab/signals/SteppedSine.h"

#include <cmath>

#include "pluginlab/signals/Signals.h"

namespace pluginlab::signals
{
namespace
{
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kOctave = 2.0;
}

std::vector<double> getSteppedSineFrequencies(const SteppedSineSettings& settings)
{
    std::vector<double> frequencies;
    const double octaves = std::log2(settings.startHz / settings.stopHz);
    const int steps = std::max(1, static_cast<int>(std::round(octaves * settings.stepsPerOctave)));
    for (int step = 0; step <= steps; ++step)
    {
        frequencies.push_back(settings.startHz * std::pow(kOctave, -octaves * step / steps));
    }
    return frequencies;
}

SteppedSine makeSteppedSine(const SteppedSineSettings& settings, int channels)
{
    SteppedSine result;
    result.settings = settings;
    const double amplitude = dbToGain(settings.levelDbfsPeak);
    int position = 0;
    std::vector<int> stepLengths;
    for (const double frequency : getSteppedSineFrequencies(settings))
    {
        SineStep step;
        step.frequencyHz = frequency;
        step.start = position;
        step.measureStart = position + settings.latencySamples + static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
        // a whole number of periods, at least minimumPeriods and at least measureSeconds
        const double periods = std::max(static_cast<double>(settings.minimumPeriods), std::ceil(settings.measureSeconds * frequency));
        step.measureLength = static_cast<int>(std::round(periods * settings.sampleRate / frequency));
        const int length = step.measureStart + step.measureLength - position;
        result.steps.push_back(step);
        stepLengths.push_back(length);
        position += length;
    }
    result.signal = makeSilence(position, channels);
    for (size_t index = 0; index < result.steps.size(); ++index)
    {
        const SineStep& step = result.steps[index];
        for (int sample = 0; sample < stepLengths[index]; ++sample)
        {
            // every step starts with phase 0; the settling time takes the switch from the step before
            const float value = static_cast<float>(amplitude * std::sin(kTwoPi * step.frequencyHz * sample / settings.sampleRate));
            for (int channel = 0; channel < channels; ++channel)
            {
                result.signal.setSample(channel, step.start + sample, value);
            }
        }
    }
    return result;
}
}
