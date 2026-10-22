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

    double quietDbfs = std::max(kQuietestDbfs, std::min(-40.0, result.fit.thresholdDb - 0.5 * result.fit.kneeDb - kBelowKneeDb));
    if (settings.quietDbfs < 0.0)
    {
        quietDbfs = settings.quietDbfs;
    }
    result.quietDbfs = quietDbfs;
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

namespace
{
DynamicsVerdict judgeDb(double differenceDb, const DynamicsComparisonLimits& limits)
{
    if (std::abs(differenceDb) <= limits.sameDb)
    {
        return DynamicsVerdict::Same;
    }
    if (std::abs(differenceDb) <= limits.closeDb)
    {
        return DynamicsVerdict::Close;
    }
    return DynamicsVerdict::Different;
}

// a ratio of times: the same within 1 + sameShare either way
DynamicsVerdict judgeRatio(double ratio, const DynamicsComparisonLimits& limits)
{
    const double distance = std::abs(std::log(std::max(ratio, 1.0e-12)));
    if (distance <= std::log(1.0 + limits.sameShare))
    {
        return DynamicsVerdict::Same;
    }
    if (distance <= std::log(1.0 + limits.closeShare))
    {
        return DynamicsVerdict::Close;
    }
    return DynamicsVerdict::Different;
}

// the gain of the first step (both phases), relative to its value before the rise
std::vector<double> getStepShape(const StepResponse& step)
{
    std::vector<double> shape;
    if (step.attackStart <= 0 || step.gainDb.empty())
    {
        return shape;
    }
    const int end = std::min(static_cast<int>(step.gainDb.size()), 2 * step.releaseStart - step.attackStart);
    const double before = step.gainDb[static_cast<size_t>(step.attackStart - 1)];
    for (int sample = step.attackStart; sample < end; ++sample)
    {
        shape.push_back(step.gainDb[static_cast<size_t>(sample)] - before);
    }
    return shape;
}
}

const char* getDynamicsVerdictName(DynamicsVerdict verdict)
{
    if (verdict == DynamicsVerdict::Same)
    {
        return "the same";
    }
    if (verdict == DynamicsVerdict::Close)
    {
        return "close";
    }
    return "different";
}

DynamicsComparison compareDynamics(const DynamicsMeasurement& a, const DynamicsMeasurement& b, const DynamicsComparisonLimits& limits)
{
    DynamicsComparison result;
    // the static curves at the levels both have (the same staircase: the same levels)
    for (const StaticPoint& pointA : a.curve.rising)
    {
        for (const StaticPoint& pointB : b.curve.rising)
        {
            if (std::abs(pointA.inputDbfs - pointB.inputDbfs) < 1.0e-6)
            {
                const double difference = pointB.gainDb - pointA.gainDb;
                result.levelsDbfs.push_back(pointA.inputDbfs);
                result.gainDifferenceDb.push_back(difference);
                if (std::abs(difference) > std::abs(result.largestCurveDifferenceDb))
                {
                    result.largestCurveDifferenceDb = difference;
                    result.largestAtDbfs = pointA.inputDbfs;
                }
                break;
            }
        }
    }
    result.curve = judgeDb(result.largestCurveDifferenceDb, limits);
    result.sameTimeModel = a.compressor == b.compressor;
    if (a.compressor && b.compressor)
    {
        result.attackRatio = b.step.best.parameters.attackSeconds / a.step.best.parameters.attackSeconds;
        result.releaseRatio = b.step.best.parameters.releaseSeconds / a.step.best.parameters.releaseSeconds;
        result.attack = judgeRatio(result.attackRatio, limits);
        result.release = judgeRatio(result.releaseRatio, limits);
        result.sameTimeModel = a.step.best.model == b.step.best.model;
        result.sameDetector = a.detector.kind == b.detector.kind;
        result.sameBands = a.transfer.kind == b.transfer.kind;
        const std::vector<double> shapeA = getStepShape(a.step);
        const std::vector<double> shapeB = getStepShape(b.step);
        const size_t count = std::min(shapeA.size(), shapeB.size());
        double squares = 0.0;
        for (size_t index = 0; index < count; ++index)
        {
            squares += (shapeB[index] - shapeA[index]) * (shapeB[index] - shapeA[index]);
        }
        result.stepRmsDifferenceDb = std::sqrt(squares / static_cast<double>(std::max<size_t>(1, count)));
    }
    return result;
}
}
