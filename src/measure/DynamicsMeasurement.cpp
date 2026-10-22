#include <algorithm>
#include <cmath>

#include "pluginlab/measure/Dynamics.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kLevelDependentDb = 0.5;
constexpr double kBelowKneeDb = 10.0;      // the quiet levels: this far below the knee's lower end
constexpr double kQuietestDbfs = -80.0;
constexpr double kToneHz = 1000.0;         // the author's answer to question 3: 1 kHz
constexpr double kLoudDbfs = 0.0;
constexpr double kDistortionHeadroomDb = 6.0;
constexpr double kLowestProbeHz = 50.0;
constexpr double kHighestProbeHz = 15000.0;
}

DynamicsMeasurement measureDynamicsResponse(const Device& device, const DynamicsMeasurementSettings& settings)
{
    DynamicsMeasurement result;
    const int channels = std::max(1, settings.channels);
    const double fs = settings.sampleRate;

    StaticCurveSettings curveSettings;
    curveSettings.sampleRate = fs;
    curveSettings.latencySamples = settings.latencySamples;
    result.curve = measureStaticCurve(device, channels, curveSettings);
    if (result.curve.rising.empty())
    {
        return result;
    }
    double highest = -1.0e300;
    double lowest = 1.0e300;
    for (const StaticPoint& point : result.curve.rising)
    {
        highest = std::max(highest, point.gainDb);
        lowest = std::min(lowest, point.gainDb);
    }
    result.gainRangeDb = highest - lowest;
    result.levelDependent = result.gainRangeDb > kLevelDependentDb;
    result.fit = fitStaticCurve(result.curve.rising);
    result.compressor = result.levelDependent && result.fit.compresses;
    if (! result.compressor)
    {
        return result;
    }

    const double quietDbfs = std::max(kQuietestDbfs, std::min(-40.0, result.fit.thresholdDb - 0.5 * result.fit.kneeDb - kBelowKneeDb));
    StepSettings stepSettings;
    stepSettings.sampleRate = fs;
    stepSettings.latencySamples = settings.latencySamples;
    stepSettings.lowDbfs = quietDbfs;
    stepSettings.highDbfs = kLoudDbfs;
    result.step = measureStepResponse(device, channels, result.fit, stepSettings);
    result.audioDelaySamples = measureLookahead(device, channels, fs, quietDbfs);
    result.detector = measureDetector(device, channels, result.fit, result.step.best, fs, kLoudDbfs, settings.latencySamples);
    if (settings.bands)
    {
        std::vector<double> probes;
        for (double frequency = kLowestProbeHz; frequency <= kHighestProbeHz; frequency *= std::pow(2.0, 1.0 / 6.0))
        {
            probes.push_back(frequency);
        }
        result.toneHz = kToneHz;
        result.transfer = measureDynamicTransfer(device, channels, fs, kToneHz, kLoudDbfs, quietDbfs, probes); // the probe alone is not reduced
    }
    if (channels >= 2)
    {
        result.stereo = true;
        result.stereoLink = measureStereoLink(device, fs, kLoudDbfs, quietDbfs);
    }
    result.distortionLevelDbfs = kLoudDbfs;
    if (result.fit.thresholdDb + 0.5 * result.fit.kneeDb + kDistortionHeadroomDb < -kDistortionHeadroomDb)
    {
        result.distortionLevelDbfs = -kDistortionHeadroomDb;
    }
    result.distortion = measureDynamicsDistortion(device, channels, fs, result.distortionLevelDbfs, {50.0, 100.0, 1000.0});
    result.limiter = measureLimiter(device, channels, fs);
    return result;
}
}
