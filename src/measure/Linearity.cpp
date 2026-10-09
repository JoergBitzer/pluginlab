#include "pluginlab/measure/Linearity.h"

#include <cmath>
#include <complex>

#include "pluginlab/measure/Distortion.h"
#include "pluginlab/measure/Noise.h"
#include "pluginlab/signals/Signals.h"

namespace pluginlab::measure
{
namespace
{
struct Point
{
    double inputDbfs = 0.0;
    double outputDbfs = 0.0;
    double thdnDb = 0.0;
};

int getPowerOfTwoAtLeast(double samples)
{
    int size = 1;
    while (size < samples)
    {
        size *= 2;
    }
    return size;
}
}

MaximumLevelResult measureMaximumLevel(const Device& device, const MaximumLevelSettings& settings)
{
    MaximumLevelResult result;
    result.settings = settings;
    DistortionSettings distortion;
    distortion.sampleRate = settings.sampleRate;
    distortion.frequencyHz = settings.frequencyHz;
    distortion.settleSeconds = settings.settleSeconds;
    distortion.measureSeconds = settings.measureSeconds;
    distortion.channels = settings.channels;
    const auto evaluate = [&](double level)
    {
        distortion.levelDbfs = level;
        const DistortionResult measured = measureDistortion(device, distortion);
        result.frequencyHz = measured.frequencyHz;
        result.windowSamples = measured.windowSamples;
        ++result.evaluations;
        Point point;
        point.inputDbfs = level;
        point.outputDbfs = measured.channels[0].outputDbfs;
        point.thdnDb = measured.channels[0].thdnDb;
        return point;
    };
    double referenceGain = 0.0;
    if (settings.method == MaximumLevelMethod::Compression)
    {
        const Point reference = evaluate(settings.referenceLevelDbfs);
        referenceGain = reference.outputDbfs - reference.inputDbfs;
    }
    // the criterion: THD+N at or above the limit, or the gain fallen by the compression limit below the reference gain
    const auto exceeds = [&](const Point& point)
    {
        if (settings.method == MaximumLevelMethod::ThdN)
        {
            return point.thdnDb >= settings.thdnLimitDb;
        }
        return referenceGain - (point.outputDbfs - point.inputDbfs) >= settings.compressionLimitDb;
    };

    // from the start level: upwards in 1 dB steps to the first level that exceeds; if the start level already exceeds, downwards to the first level
    // that does not. Then bisect between the two.
    double below = settings.startDbfs;
    double above = settings.startDbfs;
    if (exceeds(evaluate(settings.startDbfs)))
    {
        bool passed = false;
        while (below > settings.lowestDbfs && ! passed)
        {
            above = below;
            below = std::max(below - 1.0, settings.lowestDbfs);
            passed = ! exceeds(evaluate(below));
        }
        if (! passed)
        {
            result.exceededEverywhere = true;
            return result;
        }
    }
    else
    {
        bool crossed = false;
        while (above < settings.highestDbfs && ! crossed)
        {
            below = above;
            above = std::min(above + 1.0, settings.highestDbfs);
            crossed = exceeds(evaluate(above));
        }
        if (! crossed)
        {
            return result;
        }
    }
    while (above - below > settings.resolutionDb)
    {
        const double middle = 0.5 * (below + above);
        if (exceeds(evaluate(middle)))
        {
            above = middle;
        }
        else
        {
            below = middle;
        }
    }
    const Point found = evaluate(0.5 * (below + above));
    result.found = true;
    result.maximumInputDbfs = found.inputDbfs;
    result.maximumOutputDbfs = found.outputDbfs;
    result.thdnAtMaximumDb = found.thdnDb;
    result.gainAtMaximumDb = found.outputDbfs - found.inputDbfs;
    // 6.6.8: +3 dB above the maximum input level; THD+N above 20 % (-14 dB) indicates rollover
    result.overloadThdnDb = evaluate(found.inputDbfs + 3.0).thdnDb;
    result.rollover = result.overloadThdnDb > -14.0;
    return result;
}

LinearityResult measureGainLinearity(const Device& device, const LinearitySettings& settings)
{
    LinearityResult result;
    result.settings = settings;
    result.windowSamples = getPowerOfTwoAtLeast(settings.measureSeconds * settings.sampleRate);
    const int length = result.windowSamples;
    result.frequencyHz = getCoherentFrequency(settings.frequencyHz, settings.sampleRate, length);
    const int start = static_cast<int>(std::round(settings.settleSeconds * settings.sampleRate));

    NoiseSettings idle;
    idle.sampleRate = settings.sampleRate;
    idle.settleSeconds = settings.settleSeconds;
    idle.measureSeconds = settings.measureSeconds;
    idle.channels = settings.channels;
    result.idleNoiseDbfs = measureIdleNoise(device, idle).channels[0].ccirRmsDbfs;

    const StandardLowPass lowPass(settings.sampleRate);
    const double binWidth = settings.sampleRate / length;
    const int firstBin = std::max(1, static_cast<int>(std::ceil((result.frequencyHz - 0.5 * settings.bandwidthHz) / binWidth)));
    const int lastBin = static_cast<int>(std::floor((result.frequencyHz + 0.5 * settings.bandwidthHz) / binWidth));
    const auto measureBand = [&](double level)
    {
        signals::SineSettings sine;
        sine.sampleRate = settings.sampleRate;
        sine.frequencyHz = result.frequencyHz;
        sine.levelDbfsPeak = level;
        sine.length = start + length;
        const juce::AudioBuffer<float> output = device(signals::makeSine(sine, settings.channels), settings.sampleRate);
        const std::vector<std::complex<double>> spectrum = getSpectrum(lowPass.process(output.getReadPointer(0), output.getNumSamples()), start, length);
        // the frequency-domain band-pass (5.2.10): the rms of the bins within +- bandwidth / 2
        double power = 0.0;
        for (int bin = firstBin; bin <= lastBin; ++bin)
        {
            power += 2.0 * std::norm(spectrum[static_cast<size_t>(bin)]) / (static_cast<double>(length) * length);
        }
        return rmsToDbfs(std::sqrt(power));
    };

    const double referenceInput = settings.maximumInputDbfs - 5.0;
    const double referenceOutput = measureBand(referenceInput);
    for (double level = referenceInput; level >= settings.lowestDbfs - 1.0e-9; level -= settings.stepDb)
    {
        double output = referenceOutput;
        if (level < referenceInput)
        {
            output = measureBand(level);
        }
        result.inputDbfs.push_back(level);
        result.outputDbfs.push_back(output);
        result.deviationDb.push_back(output - (referenceOutput + level - referenceInput));
        if (output < result.idleNoiseDbfs + 5.0)
        {
            break;
        }
    }
    return result;
}
}
