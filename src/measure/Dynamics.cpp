#include "pluginlab/measure/Dynamics.h"

#include <algorithm>
#include <cmath>

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kGridThresholdStepDb = 0.25;
constexpr double kGridKneeStepDb = 0.5;
constexpr double kLargestKneeDb = 24.0;
constexpr int kRefineSteps = 40;

double toDb(double ratio)
{
    return 20.0 * std::log10(std::max(std::abs(ratio), 1.0e-15));
}

// the shape of the gain computer without its two linear parameters: phi = 0 below the knee, (x - T + W/2)^2 / (2 W) in it, x - T above
double getShape(double level, double threshold, double knee)
{
    const double over = level - threshold;
    if (2.0 * over > knee)
    {
        return over;
    }
    if (knee > 0.0 && 2.0 * std::abs(over) <= knee)
    {
        return (over + knee / 2.0) * (over + knee / 2.0) / (2.0 * knee);
    }
    return 0.0;
}

struct LinearFit
{
    double slope = 0.0;    // 1/R - 1
    double offset = 0.0;   // makeup
    double squares = 1.0e300;
};

// gain = offset + slope phi(level) by least squares
LinearFit fitLinear(const std::vector<StaticPoint>& points, double threshold, double knee)
{
    double n = 0.0;
    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;
    for (const StaticPoint& point : points)
    {
        const double phi = getShape(point.inputDbfs, threshold, knee);
        n += 1.0;
        sx += phi;
        sy += point.gainDb;
        sxx += phi * phi;
        sxy += phi * point.gainDb;
    }
    LinearFit fit;
    const double denominator = n * sxx - sx * sx;
    if (n < 2.0)
    {
        return fit;
    }
    if (std::abs(denominator) < 1.0e-12)
    {
        fit.offset = sy / n; // nothing above the knee: a plain gain
    }
    else
    {
        fit.slope = (n * sxy - sx * sy) / denominator;
        fit.offset = (sy - fit.slope * sx) / n;
    }
    fit.squares = 0.0;
    for (const StaticPoint& point : points)
    {
        const double error = point.gainDb - (fit.offset + fit.slope * getShape(point.inputDbfs, threshold, knee));
        fit.squares += error * error;
    }
    return fit;
}
}

std::vector<float> makeStaircase(const StaircaseSettings& settings)
{
    const int stepLength = static_cast<int>(std::lround(settings.stepSeconds * settings.sampleRate));
    // an even number of samples per half period: the square wave never hits zero and has the same length of +A and -A
    const int halfPeriod = std::max(1, static_cast<int>(std::lround(settings.sampleRate / settings.frequencyHz / 2.0)));
    std::vector<float> signal;
    signal.reserve(settings.levelsDbfs.size() * static_cast<size_t>(stepLength));
    long long index = 0;
    for (const double level : settings.levelsDbfs)
    {
        const double amplitude = std::pow(10.0, level / 20.0);
        for (int sample = 0; sample < stepLength; ++sample, ++index)
        {
            double value = 0.0;
            if (settings.shape == StaircaseShape::Square)
            {
                value = amplitude;
                if ((index / halfPeriod) % 2 == 1)
                {
                    value = -amplitude;
                }
            }
            else
            {
                value = amplitude * std::sin(2.0 * kPi * settings.frequencyHz * static_cast<double>(index) / settings.sampleRate);
            }
            signal.push_back(static_cast<float>(value));
        }
    }
    return signal;
}

std::vector<double> getGainEnvelopeDb(const std::vector<double>& input, const std::vector<double>& output, int lagSamples)
{
    std::vector<double> gain(output.size(), 0.0);
    for (size_t sample = 0; sample < output.size(); ++sample)
    {
        const long long source = static_cast<long long>(sample) - lagSamples;
        if (source >= 0 && static_cast<size_t>(source) < input.size() && std::abs(input[static_cast<size_t>(source)]) > 0.0)
        {
            gain[sample] = toDb(output[sample] / input[static_cast<size_t>(source)]);
        }
    }
    return gain;
}

StaticCurve measureStaticCurve(const Device& device, int channels, const StaticCurveSettings& settings)
{
    StaticCurve result;
    std::vector<double> levels;
    for (double level = settings.lowestDbfs; level <= settings.highestDbfs + 1.0e-9; level += settings.stepDb)
    {
        levels.push_back(level);
    }
    const int stepLength = static_cast<int>(std::lround(settings.stepSeconds * settings.sampleRate));
    const int lag = static_cast<int>(std::lround(settings.latencySamples));
    for (const bool rising : {true, false})
    {
        StaircaseSettings staircase;
        staircase.sampleRate = settings.sampleRate;
        staircase.stepSeconds = settings.stepSeconds;
        staircase.levelsDbfs = levels;
        if (! rising)
        {
            std::reverse(staircase.levelsDbfs.begin(), staircase.levelsDbfs.end());
        }
        const std::vector<float> signal = makeStaircase(staircase);
        juce::AudioBuffer<float> buffer(channels, static_cast<int>(signal.size()));
        for (int channel = 0; channel < channels; ++channel)
        {
            buffer.copyFrom(channel, 0, signal.data(), static_cast<int>(signal.size()));
        }
        const juce::AudioBuffer<float> output = device(buffer, settings.sampleRate);
        const std::vector<double> in(signal.begin(), signal.end());
        std::vector<double> out;
        for (int sample = 0; sample < output.getNumSamples(); ++sample)
        {
            out.push_back(output.getSample(0, sample));
        }
        const std::vector<double> gain = getGainEnvelopeDb(in, out, lag);
        std::vector<StaticPoint>* points = &result.rising;
        if (! rising)
        {
            points = &result.falling;
        }
        for (size_t step = 0; step < staircase.levelsDbfs.size(); ++step)
        {
            // the last quarter of the step (shifted by the lag): its mean gain, and whether it still moved
            const size_t end = std::min(gain.size(), (step + 1) * static_cast<size_t>(stepLength) + static_cast<size_t>(lag));
            const size_t from = end - static_cast<size_t>(stepLength / 4);
            double sum = 0.0;
            double lowest = 1.0e300;
            double highest = -1.0e300;
            for (size_t sample = from; sample < end; ++sample)
            {
                sum += gain[sample];
                lowest = std::min(lowest, gain[sample]);
                highest = std::max(highest, gain[sample]);
            }
            StaticPoint point;
            point.inputDbfs = staircase.levelsDbfs[step];
            point.gainDb = sum / static_cast<double>(end - from);
            point.outputDbfs = point.inputDbfs + point.gainDb;
            point.settled = highest - lowest < settings.settleDb;
            if (! point.settled)
            {
                ++result.unsettledSteps;
            }
            points->push_back(point);
        }
    }
    std::reverse(result.falling.begin(), result.falling.end()); // both from the lowest level up
    for (size_t index = 0; index < result.rising.size() && index < result.falling.size(); ++index)
    {
        result.hysteresisDb = std::max(result.hysteresisDb, std::abs(result.rising[index].gainDb - result.falling[index].gainDb));
    }
    return result;
}

StaticFit fitStaticCurve(const std::vector<StaticPoint>& points)
{
    StaticFit result;
    if (points.size() < 3)
    {
        return result;
    }
    double lowest = points.front().inputDbfs;
    double highest = points.front().inputDbfs;
    for (const StaticPoint& point : points)
    {
        lowest = std::min(lowest, point.inputDbfs);
        highest = std::max(highest, point.inputDbfs);
    }
    // the grid
    double bestThreshold = lowest;
    double bestKnee = 0.0;
    LinearFit best;
    for (double threshold = lowest; threshold <= highest; threshold += kGridThresholdStepDb)
    {
        for (double knee = 0.0; knee <= kLargestKneeDb; knee += kGridKneeStepDb)
        {
            const LinearFit fit = fitLinear(points, threshold, knee);
            if (fit.squares < best.squares)
            {
                best = fit;
                bestThreshold = threshold;
                bestKnee = knee;
            }
        }
    }
    // refine: coordinate descent with halving steps
    double thresholdStep = kGridThresholdStepDb;
    double kneeStep = kGridKneeStepDb;
    for (int step = 0; step < kRefineSteps; ++step)
    {
        bool improved = false;
        for (const double dt : {-thresholdStep, thresholdStep})
        {
            const LinearFit fit = fitLinear(points, bestThreshold + dt, bestKnee);
            if (fit.squares < best.squares)
            {
                best = fit;
                bestThreshold += dt;
                improved = true;
            }
        }
        for (const double dk : {-kneeStep, kneeStep})
        {
            if (bestKnee + dk < 0.0)
            {
                continue;
            }
            const LinearFit fit = fitLinear(points, bestThreshold, bestKnee + dk);
            if (fit.squares < best.squares)
            {
                best = fit;
                bestKnee += dk;
                improved = true;
            }
        }
        if (! improved)
        {
            thresholdStep /= 2.0;
            kneeStep /= 2.0;
        }
    }
    result.thresholdDb = bestThreshold;
    result.kneeDb = bestKnee;
    result.makeupDb = best.offset;
    const double inverse = 1.0 + best.slope;
    result.ratio = 1.0e6;
    if (inverse > 1.0e-6)
    {
        result.ratio = 1.0 / inverse;
    }
    result.rmsErrorDb = std::sqrt(best.squares / static_cast<double>(points.size()));
    result.compresses = result.ratio > 1.01;
    return result;
}

double getModelGainDb(const StaticFit& fit, double levelDb)
{
    return fit.makeupDb + (1.0 / fit.ratio - 1.0) * getShape(levelDb, fit.thresholdDb, fit.kneeDb);
}
}
