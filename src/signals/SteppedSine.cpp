#include "pluginlab/signals/SteppedSine.h"

#include <cmath>

#include "pluginlab/signals/Signals.h"

namespace pluginlab::signals
{
namespace
{
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kOctave = 2.0;
constexpr double kPi = 3.14159265358979323846;

// The rising half of a Hann window over fadeLength samples: 0 at the first sample, close to 1 at the last
double getFadeGain(int sample, int fadeLength)
{
    return 0.5 - 0.5 * std::cos(kPi * sample / fadeLength);
}
}

std::vector<double> getSteppedSineFrequencies(const SteppedSineSettings& settings)
{
    if (!settings.frequencies.empty())
    {
        return settings.frequencies;
    }
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
    const int fadeLength = static_cast<int>(std::round(settings.fadeSeconds * settings.sampleRate));
    const int settleLength = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));
    int position = 0;
    for (const double frequency : getSteppedSineFrequencies(settings))
    {
        SineStep step;
        step.frequencyHz = frequency;
        step.start = position;
        step.measureStart = position + fadeLength + settings.latencySamples + settleLength;
        // a whole number of periods, at least minimumPeriods and at least measureSeconds
        const double periods = std::max(static_cast<double>(settings.minimumPeriods), std::ceil(settings.measureSeconds * frequency));
        step.measureLength = static_cast<int>(std::round(periods * settings.sampleRate / frequency));
        step.length = step.measureStart + step.measureLength + fadeLength - position;
        result.steps.push_back(step);
        position += step.length;
    }
    result.signal = makeSilence(position, channels);
    for (const SineStep& step : result.steps)
    {
        for (int sample = 0; sample < step.length; ++sample)
        {
            // every step starts with phase 0, faded in from and out to silence
            double gain = 1.0;
            if (sample < fadeLength)
            {
                gain = getFadeGain(sample, fadeLength);
            }
            if (sample >= step.length - fadeLength)
            {
                gain = getFadeGain(step.length - 1 - sample, fadeLength);
            }
            const float value = static_cast<float>(gain * amplitude * std::sin(kTwoPi * step.frequencyHz * sample / settings.sampleRate));
            for (int channel = 0; channel < channels; ++channel)
            {
                result.signal.setSample(channel, step.start + sample, value);
            }
        }
    }
    return result;
}
}
