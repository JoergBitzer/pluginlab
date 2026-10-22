#include "pluginlab/measure/Dynamics.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <limits>

#include "pluginlab/measure/Distortion.h"

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

const char* getTimeModelName(TimeModel model)
{
    if (model == TimeModel::LevelLog)
    {
        return "level smoothed in dB";
    }
    if (model == TimeModel::TwoStage)
    {
        return "detector power and gain smoothed (two stages)";
    }
    return "gain reduction smoothed in dB";
}

namespace
{
constexpr double kSilence = -200.0;

double getCoefficient(double seconds, double sampleRate)
{
    return std::exp(-1.0 / (std::max(seconds, 1.0e-7) * sampleRate));
}

double getReduction(const StaticFit& curve, double levelDb)
{
    return curve.makeupDb - getModelGainDb(curve, levelDb);
}
}

std::vector<double> simulateGainDb(const std::vector<double>& levelsDb, const StaticFit& curve, TimeModel model, const TimeParameters& parameters, double sampleRate)
{
    const double attack = getCoefficient(parameters.attackSeconds, sampleRate);
    const double release = getCoefficient(parameters.releaseSeconds, sampleRate);
    const double detectorAttack = getCoefficient(parameters.detectorAttackSeconds, sampleRate);
    const double detectorRelease = getCoefficient(parameters.detectorReleaseSeconds, sampleRate);
    const int hold = static_cast<int>(std::lround(parameters.holdSeconds * sampleRate));
    std::vector<double> gain(levelsDb.size());
    double smoothed = 0.0;
    double detector = 0.0;
    if (model == TimeModel::LevelLog)
    {
        detector = kSilence;
    }
    int holding = 0;
    for (size_t sample = 0; sample < levelsDb.size(); ++sample)
    {
        double level = levelsDb[sample];
        if (model == TimeModel::LevelLog)
        {
            double coefficient = release;
            if (level > detector)
            {
                coefficient = attack;
            }
            detector = coefficient * detector + (1.0 - coefficient) * level;
            smoothed = getReduction(curve, detector);
        }
        else
        {
            if (model == TimeModel::TwoStage)
            {
                const double power = std::pow(10.0, level / 10.0);
                double coefficient = detectorRelease;
                if (power > detector)
                {
                    coefficient = detectorAttack;
                }
                detector = coefficient * detector + (1.0 - coefficient) * power;
                level = 10.0 * std::log10(std::max(detector, 1.0e-20));
            }
            const double reduction = getReduction(curve, level);
            if (reduction > smoothed)
            {
                smoothed = attack * smoothed + (1.0 - attack) * reduction;
                holding = hold;
            }
            else if (holding > 0)
            {
                --holding;
            }
            else
            {
                smoothed = release * smoothed + (1.0 - release) * reduction;
            }
        }
        gain[sample] = curve.makeupDb - smoothed;
    }
    return gain;
}

namespace
{
// the RMS dB error of the model over the attack and the release phase
double getModelError(const StepResponse& step, const StaticFit& curve, TimeModel model, const TimeParameters& parameters, double sampleRate)
{
    const std::vector<double> simulated = simulateGainDb(step.levelsDb, curve, model, parameters, sampleRate);
    double squares = 0.0;
    size_t count = 0;
    for (size_t sample = static_cast<size_t>(step.attackStart); sample < step.gainDb.size(); ++sample)
    {
        const double error = simulated[sample] - step.gainDb[sample];
        squares += error * error;
        ++count;
    }
    return std::sqrt(squares / std::max<size_t>(1, count));
}

constexpr int kSimplexEvaluations = 600;

// Nelder-Mead in a box (points clamped to it): reflection, expansion, contraction, shrink; a dimension with step 0 stays fixed
std::vector<double> minimizeSimplex(const std::function<double(const std::vector<double>&)>& cost, const std::vector<double>& start,
                                    const std::vector<double>& steps, const std::vector<double>& lower, const std::vector<double>& upper, int evaluations)
{
    const size_t dimensions = start.size();
    const auto clamp = [&](std::vector<double> point)
    {
        for (size_t index = 0; index < dimensions; ++index)
        {
            point[index] = std::clamp(point[index], lower[index], upper[index]);
        }
        return point;
    };
    std::vector<std::vector<double>> points{clamp(start)};
    for (size_t index = 0; index < dimensions; ++index)
    {
        if (steps[index] <= 0.0)
        {
            continue;
        }
        std::vector<double> point = start;
        point[index] += steps[index];
        if (point[index] > upper[index])
        {
            point[index] = start[index] - steps[index];
        }
        points.push_back(clamp(point));
    }
    std::vector<double> values;
    for (const std::vector<double>& point : points)
    {
        values.push_back(cost(point));
    }
    int used = static_cast<int>(points.size());
    const size_t vertices = points.size();
    while (used < evaluations && vertices > 1)
    {
        // order: best first
        std::vector<size_t> order(vertices);
        for (size_t index = 0; index < vertices; ++index)
        {
            order[index] = index;
        }
        std::sort(order.begin(), order.end(), [&values](size_t a, size_t b) { return values[a] < values[b]; });
        std::vector<std::vector<double>> sortedPoints;
        std::vector<double> sortedValues;
        for (const size_t index : order)
        {
            sortedPoints.push_back(points[index]);
            sortedValues.push_back(values[index]);
        }
        points = sortedPoints;
        values = sortedValues;
        if (std::abs(values.back() - values.front()) < 1.0e-12)
        {
            break;
        }
        std::vector<double> centroid(dimensions, 0.0);
        for (size_t vertex = 0; vertex + 1 < vertices; ++vertex)
        {
            for (size_t index = 0; index < dimensions; ++index)
            {
                centroid[index] += points[vertex][index] / static_cast<double>(vertices - 1);
            }
        }
        const auto along = [&](double factor)
        {
            std::vector<double> point(dimensions);
            for (size_t index = 0; index < dimensions; ++index)
            {
                point[index] = centroid[index] + factor * (points.back()[index] - centroid[index]);
            }
            return clamp(point);
        };
        const std::vector<double> reflected = along(-1.0);
        const double reflectedValue = cost(reflected);
        ++used;
        if (reflectedValue < values.front())
        {
            const std::vector<double> expanded = along(-2.0);
            const double expandedValue = cost(expanded);
            ++used;
            if (expandedValue < reflectedValue)
            {
                points.back() = expanded;
                values.back() = expandedValue;
            }
            else
            {
                points.back() = reflected;
                values.back() = reflectedValue;
            }
            continue;
        }
        if (reflectedValue < values[vertices - 2])
        {
            points.back() = reflected;
            values.back() = reflectedValue;
            continue;
        }
        const std::vector<double> contracted = along(0.5);
        const double contractedValue = cost(contracted);
        ++used;
        if (contractedValue < values.back())
        {
            points.back() = contracted;
            values.back() = contractedValue;
            continue;
        }
        // shrink towards the best
        for (size_t vertex = 1; vertex < vertices; ++vertex)
        {
            for (size_t index = 0; index < dimensions; ++index)
            {
                points[vertex][index] = points.front()[index] + 0.5 * (points[vertex][index] - points.front()[index]);
            }
            values[vertex] = cost(points[vertex]);
            ++used;
        }
    }
    size_t bestIndex = 0;
    for (size_t index = 1; index < values.size(); ++index)
    {
        if (values[index] < values[bestIndex])
        {
            bestIndex = index;
        }
    }
    return points[bestIndex];
}

// the time constants in log10(seconds): a grid, then coordinate descent with halving steps; the hold (seconds) free between 0 and the measured
// value (a measured hold can be the slow start of a release that a smoothed detector delays)
TimeFit fitModel(const StepResponse& step, const StaticFit& curve, TimeModel model, double measuredHold, double sampleRate)
{
    constexpr double kLowest = -4.0;   // 0.1 ms
    constexpr double kHighest = 0.5;   // 3 s
    constexpr int kRefine = 80;
    size_t count = 2; // attack and release; the two-stage model also the detector's
    if (model == TimeModel::TwoStage)
    {
        count = 4;
    }
    const auto toParameters = [count](const std::vector<double>& values)
    {
        TimeParameters parameters;
        parameters.attackSeconds = std::pow(10.0, values[0]);
        parameters.releaseSeconds = std::pow(10.0, values[1]);
        if (count > 2)
        {
            parameters.detectorAttackSeconds = std::pow(10.0, values[2]);
            parameters.detectorReleaseSeconds = std::pow(10.0, values[3]);
        }
        parameters.holdSeconds = values[count];
        return parameters;
    };
    std::vector<double> best(count + 1, -2.0);
    best[count] = measuredHold;
    double bestError = 1.0e300;
    // the grid of the time constants (half decades; whole decades for the four of the two-stage model), the hold as measured
    double gridStepDecades = 0.5;
    if (count > 2)
    {
        gridStepDecades = 1.0;
    }
    std::vector<double> current = best;
    const auto gridStep = [&](auto& self, size_t index) -> void
    {
        if (index == count)
        {
            const double error = getModelError(step, curve, model, toParameters(current), sampleRate);
            if (error < bestError)
            {
                bestError = error;
                best = current;
            }
            return;
        }
        for (double value = kLowest; value <= kHighest + 1.0e-9; value += gridStepDecades)
        {
            current[index] = value;
            self(self, index + 1);
        }
    };
    gridStep(gridStep, 0);
    // coordinate descent: the time constants in decades, the hold in seconds
    double stepSize = 0.25;
    double holdStep = 0.25 * measuredHold;
    for (int iteration = 0; iteration < kRefine && (stepSize > 1.0e-6 || holdStep > 1.0e-7); ++iteration)
    {
        bool improved = false;
        for (size_t index = 0; index <= count; ++index)
        {
            double size = stepSize;
            if (index == count)
            {
                size = holdStep;
                if (size <= 0.0)
                {
                    continue;
                }
            }
            for (const double delta : {-size, size})
            {
                std::vector<double> trial = best;
                if (index == count)
                {
                    trial[index] = std::clamp(trial[index] + delta, 0.0, measuredHold);
                }
                else
                {
                    trial[index] = std::clamp(trial[index] + delta, kLowest, kHighest);
                }
                const double error = getModelError(step, curve, model, toParameters(trial), sampleRate);
                if (error < bestError)
                {
                    bestError = error;
                    best = trial;
                    improved = true;
                }
            }
        }
        if (! improved)
        {
            stepSize /= 2.0;
            holdStep /= 2.0;
        }
    }
    // then the simplex of Nelder and Mead (the paper's method), twice: coordinate descent stops in valleys where two time constants trade off
    std::vector<double> lower(count + 1, kLowest);
    std::vector<double> upper(count + 1, kHighest);
    lower[count] = 0.0;
    upper[count] = measuredHold;
    std::vector<double> steps(count + 1, 0.3);
    steps[count] = 0.25 * measuredHold;
    const auto cost = [&](const std::vector<double>& values) { return getModelError(step, curve, model, toParameters(values), sampleRate); };
    for (int restart = 0; restart < 2; ++restart)
    {
        best = minimizeSimplex(cost, best, steps, lower, upper, kSimplexEvaluations);
    }
    bestError = cost(best);
    TimeFit fit;
    fit.model = model;
    fit.parameters = toParameters(best);
    fit.rmsErrorDb = bestError;
    return fit;
}
}

StepResponse measureStepResponse(const Device& device, int channels, const StaticFit& curve, const StepSettings& settings)
{
    StepResponse step;
    const double fs = settings.sampleRate;
    StaircaseSettings staircase;
    staircase.sampleRate = fs;
    staircase.stepSeconds = settings.phaseSeconds;
    // two steps of different heights: two smoothers in series (the two-stage model) are nearly interchangeable on one step; the static curve
    // between them acts differently on a second, lower one
    const double middle = 0.5 * (std::max(curve.thresholdDb, settings.lowDbfs) + settings.highDbfs);
    staircase.levelsDbfs = {settings.lowDbfs, settings.highDbfs, settings.lowDbfs, middle, settings.lowDbfs};
    const std::vector<float> signal = makeStaircase(staircase);
    juce::AudioBuffer<float> buffer(channels, static_cast<int>(signal.size()));
    for (int channel = 0; channel < channels; ++channel)
    {
        buffer.copyFrom(channel, 0, signal.data(), static_cast<int>(signal.size()));
    }
    const juce::AudioBuffer<float> output = device(buffer, fs);
    std::vector<double> out;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        out.push_back(output.getSample(0, sample));
    }
    const int lag = static_cast<int>(std::lround(settings.latencySamples));
    const std::vector<double> envelope = getGainEnvelopeDb(std::vector<double>(signal.begin(), signal.end()), out, lag);
    // the gain at time n belongs to the detector's level at n (the input of n, the output of n + lag): the envelope shifted back by the lag, less
    // the look-ahead
    const size_t length = signal.size();
    step.gainDb.assign(length, 0.0);
    step.levelsDb.assign(length, 0.0);
    const int phase = static_cast<int>(std::lround(settings.phaseSeconds * fs));
    // the look-ahead: a gain that moves before the level reaches the output leads the audio it acts on (a plain latency does not); the first
    // sample that moved by more than 0.001 dB before the rise, seen through the lag
    constexpr double kMovedDb = 0.001;
    if (phase - lag - 1 >= 0 && static_cast<size_t>(phase) < envelope.size())
    {
        const double before = envelope[static_cast<size_t>(phase - 1)];
        for (int sample = phase - lag; sample < phase; ++sample)
        {
            if (std::abs(envelope[static_cast<size_t>(sample + lag)] - before) > kMovedDb)
            {
                step.leadSamples = phase - sample;
                break;
            }
        }
    }
    const int shift = lag - step.leadSamples;
    for (size_t sample = 0; sample < length; ++sample)
    {
        step.levelsDb[sample] = 20.0 * std::log10(std::max(std::abs(static_cast<double>(signal[sample])), 1.0e-15));
        if (sample + static_cast<size_t>(shift) < envelope.size())
        {
            step.gainDb[sample] = envelope[sample + static_cast<size_t>(shift)];
        }
    }
    step.attackStart = phase;
    step.releaseStart = 2 * phase;
    const int end = static_cast<int>(length) - lag;
    const int firstStepEnd = 3 * phase;

    // hold: after the fall, the time until the gain has moved by 0.01 dB
    const double atFall = step.gainDb[static_cast<size_t>(step.releaseStart - 1)];
    for (int sample = step.releaseStart; sample < firstStepEnd; ++sample)
    {
        if (std::abs(step.gainDb[static_cast<size_t>(sample)] - atFall) > 0.01)
        {
            step.holdSeconds = (sample - step.releaseStart) / fs;
            break;
        }
    }
    // the standards' convention: the time until the gain is within the tolerance of its final value (the mean of the phase's last quarter)
    const auto settle = [&](int from, int to, double tolerance)
    {
        double final = 0.0;
        const int quarter = (to - from) / 4;
        for (int sample = to - quarter; sample < to; ++sample)
        {
            final += step.gainDb[static_cast<size_t>(sample)] / quarter;
        }
        for (int sample = from; sample < to; ++sample)
        {
            if (std::abs(step.gainDb[static_cast<size_t>(sample)] - final) <= tolerance)
            {
                return (sample - from) / fs;
            }
        }
        return (to - from) / fs;
    };
    step.attackSettleSeconds = settle(step.attackStart, step.releaseStart, settings.attackToleranceDb);
    step.releaseSettleSeconds = settle(step.releaseStart, firstStepEnd, settings.releaseToleranceDb);

    // the models, the hold free up to the measured one
    const double hold = step.holdSeconds;
    step.gainDb.resize(static_cast<size_t>(end));
    step.levelsDb.resize(static_cast<size_t>(end));
    for (const TimeModel model : {TimeModel::GainLog, TimeModel::LevelLog, TimeModel::TwoStage})
    {
        step.fits.push_back(fitModel(step, curve, model, hold, fs));
        if (step.fits.back().rmsErrorDb < step.best.rmsErrorDb)
        {
            step.best = step.fits.back();
        }
    }
    return step;
}

int measureLookahead(const Device& device, int channels, double sampleRate, double levelDbfs, double maximumSeconds)
{
    const int length = static_cast<int>(sampleRate);
    juce::Random random(5);
    const float level = static_cast<float>(std::pow(10.0, levelDbfs / 20.0));
    std::vector<float> noise(static_cast<size_t>(length));
    for (float& value : noise)
    {
        value = level * (2.0f * random.nextFloat() - 1.0f);
    }
    juce::AudioBuffer<float> buffer(channels, length);
    for (int channel = 0; channel < channels; ++channel)
    {
        buffer.copyFrom(channel, 0, noise.data(), length);
    }
    const juce::AudioBuffer<float> output = device(buffer, sampleRate);
    const int maximumLag = static_cast<int>(maximumSeconds * sampleRate);
    int bestLag = 0;
    double best = -1.0;
    for (int lag = 0; lag <= maximumLag; ++lag)
    {
        double sum = 0.0;
        for (int sample = lag; sample < std::min(length, output.getNumSamples()); ++sample)
        {
            sum += noise[static_cast<size_t>(sample - lag)] * output.getSample(0, sample);
        }
        if (std::abs(sum) > best)
        {
            best = std::abs(sum);
            bestLag = lag;
        }
    }
    return bestLag;
}

const char* getDetectorKindName(DetectorKind kind)
{
    if (kind == DetectorKind::Peak)
    {
        return "peak";
    }
    if (kind == DetectorKind::Rms)
    {
        return "RMS";
    }
    return "unclear";
}

const char* getBandKindName(BandKind kind)
{
    if (kind == BandKind::SingleBand)
    {
        return "single band";
    }
    if (kind == BandKind::Multiband)
    {
        return "multiband";
    }
    if (kind == BandKind::DynamicEq)
    {
        return "dynamic EQ";
    }
    return "not level dependent";
}

namespace
{
// the device's output (channel 0) for a signal on all inputs
std::vector<double> renderThrough(const Device& device, int channels, const std::vector<float>& signal, double sampleRate, int rightChannelFrom = -1,
                                  const std::vector<float>* right = nullptr)
{
    juce::AudioBuffer<float> buffer(channels, static_cast<int>(signal.size()));
    for (int channel = 0; channel < channels; ++channel)
    {
        if (channel == rightChannelFrom && right != nullptr)
        {
            buffer.copyFrom(channel, 0, right->data(), static_cast<int>(right->size()));
            continue;
        }
        buffer.copyFrom(channel, 0, signal.data(), static_cast<int>(signal.size()));
    }
    const juce::AudioBuffer<float> output = device(buffer, sampleRate);
    std::vector<double> out;
    for (int sample = 0; sample < output.getNumSamples(); ++sample)
    {
        out.push_back(output.getSample(0, sample));
    }
    return out;
}

// the mean gain in dB over the last quarter where the input is not zero
double getSteadyGainDb(const std::vector<float>& input, const std::vector<double>& output, int lag)
{
    double sum = 0.0;
    int count = 0;
    const size_t from = input.size() * 3 / 4;
    for (size_t sample = from; sample < input.size(); ++sample)
    {
        const size_t at = sample + static_cast<size_t>(lag);
        if (std::abs(input[sample]) > 0.0f && at < output.size())
        {
            sum += 20.0 * std::log10(std::max(std::abs(output[at] / input[sample]), 1.0e-15));
            ++count;
        }
    }
    return sum / std::max(1, count);
}

// the amplitude of a sine at a frequency over the last second (an integer number of periods for whole hertz)
double getToneAmplitude(const std::vector<double>& signal, double frequencyHz, double sampleRate)
{
    const size_t length = static_cast<size_t>(sampleRate);
    const size_t from = signal.size() - std::min(signal.size(), length);
    std::complex<double> sum;
    for (size_t sample = from; sample < signal.size(); ++sample)
    {
        sum += signal[sample] * std::polar(1.0, -2.0 * kPi * frequencyHz * static_cast<double>(sample) / sampleRate);
    }
    return 2.0 * std::abs(sum) / static_cast<double>(signal.size() - from);
}

std::vector<float> makeSine(double frequencyHz, double levelDbfs, int length, double sampleRate)
{
    std::vector<float> signal(static_cast<size_t>(length));
    const double amplitude = std::pow(10.0, levelDbfs / 20.0);
    for (int sample = 0; sample < length; ++sample)
    {
        signal[static_cast<size_t>(sample)] = static_cast<float>(amplitude * std::sin(2.0 * kPi * frequencyHz * sample / sampleRate));
    }
    return signal;
}
}

DetectorTest measureDetector(const Device& device, int channels, const StaticFit& curve, const TimeFit& time, double sampleRate, double levelDbfs,
                             double latencySamples)
{
    DetectorTest result;
    const int lag = static_cast<int>(std::lround(latencySamples));
    StaircaseSettings staircase;
    staircase.sampleRate = sampleRate;
    staircase.stepSeconds = 1.0;

    // the decision: the same RMS, the peak 12 dB higher
    constexpr double kCrestDb = 12.0;
    constexpr int kPulseSpacing = 16; // 16 = 10^(12/20)^2 within 0.1 dB: the pulses have the square's RMS
    staircase.levelsDbfs = {levelDbfs - kCrestDb};
    const std::vector<float> square = makeStaircase(staircase);
    std::vector<float> pulses(square.size(), 0.0f);
    const float height = static_cast<float>(std::pow(10.0, levelDbfs / 20.0));
    for (size_t sample = 0; sample < pulses.size(); sample += kPulseSpacing)
    {
        pulses[sample] = height;
        if ((sample / kPulseSpacing) % 2 == 1)
        {
            pulses[sample] = -height;
        }
    }
    const double squareGain = getSteadyGainDb(square, renderThrough(device, channels, square, sampleRate), lag);
    const double pulseGain = getSteadyGainDb(pulses, renderThrough(device, channels, pulses, sampleRate), lag);
    result.crestChangeDb = pulseGain - squareGain;
    result.peakChangeDb = getModelGainDb(curve, levelDbfs) - getModelGainDb(curve, levelDbfs - kCrestDb);

    // the decision: the paper's first test with short gaps: a 4 kHz square, every second period zero (0.25 ms: a peak detector's release barely
    // moves in it, an RMS window of a millisecond or more halves the power)
    constexpr double kGapSquareHz = 4000.0;
    staircase.levelsDbfs = {levelDbfs};
    staircase.frequencyHz = kGapSquareHz;
    const std::vector<float> full = makeStaircase(staircase);
    const int period = 2 * std::max(1, static_cast<int>(std::lround(sampleRate / staircase.frequencyHz / 2.0)));
    std::vector<float> gapped = full;
    for (size_t sample = 0; sample < gapped.size(); ++sample)
    {
        if ((sample / static_cast<size_t>(period)) % 2 == 1)
        {
            gapped[sample] = 0.0f;
        }
    }
    result.gainChangeDb = getSteadyGainDb(gapped, renderThrough(device, channels, gapped, sampleRate), lag)
                          - getSteadyGainDb(full, renderThrough(device, channels, full, sampleRate), lag);
    result.rmsChangeDb = getModelGainDb(curve, levelDbfs - 3.0103) - getModelGainDb(curve, levelDbfs);
    // the time model as a peak detector: the level of every sample (silence in the gaps), its gain, the change of the mean over the non-zero
    // samples of the last quarter
    const auto modelGain = [&](const std::vector<float>& signal)
    {
        std::vector<double> levels(signal.size());
        for (size_t sample = 0; sample < signal.size(); ++sample)
        {
            levels[sample] = kSilence;
            if (std::abs(signal[sample]) > 0.0f)
            {
                levels[sample] = 20.0 * std::log10(std::abs(static_cast<double>(signal[sample])));
            }
        }
        const std::vector<double> gain = simulateGainDb(levels, curve, time.model, time.parameters, sampleRate);
        double sum = 0.0;
        int count = 0;
        for (size_t sample = signal.size() * 3 / 4; sample < signal.size(); ++sample)
        {
            if (std::abs(signal[sample]) > 0.0f)
            {
                sum += gain[sample];
                ++count;
            }
        }
        return sum / std::max(1, count);
    };
    result.peakModelChangeDb = modelGain(gapped) - modelGain(full);
    constexpr double kApartDb = 0.3;
    if (std::abs(result.rmsChangeDb - result.peakModelChangeDb) >= kApartDb)
    {
        result.kind = DetectorKind::Peak;
        if (std::abs(result.gainChangeDb - result.rmsChangeDb) < std::abs(result.gainChangeDb - result.peakModelChangeDb))
        {
            result.kind = DetectorKind::Rms;
        }
    }
    return result;
}

double measureThresholdAt(const Device& device, int channels, double frequencyHz, double sampleRate, double reductionDb, double lowestDbfs, double highestDbfs)
{
    std::vector<double> levels;
    for (double level = lowestDbfs; level <= highestDbfs + 1.0e-9; level += 1.0)
    {
        levels.push_back(level);
    }
    StaircaseSettings staircase;
    staircase.sampleRate = sampleRate;
    staircase.frequencyHz = frequencyHz;
    staircase.shape = StaircaseShape::Sine;
    staircase.stepSeconds = 0.3;
    staircase.levelsDbfs = levels;
    const std::vector<float> signal = makeStaircase(staircase);
    const std::vector<double> output = renderThrough(device, channels, signal, sampleRate);
    // per step: the output's amplitude over the step's last 0.1 s against the input's, relative to the lowest step
    const int step = static_cast<int>(std::lround(staircase.stepSeconds * sampleRate));
    const int tail = static_cast<int>(0.1 * sampleRate);
    std::vector<double> gains;
    for (size_t index = 0; index < levels.size(); ++index)
    {
        const size_t end = (index + 1) * static_cast<size_t>(step);
        double input = 0.0;
        double out = 0.0;
        for (size_t sample = end - static_cast<size_t>(tail); sample < end; ++sample)
        {
            input += static_cast<double>(signal[sample]) * signal[sample];
            out += output[sample] * output[sample];
        }
        gains.push_back(10.0 * std::log10(std::max(out, 1.0e-30) / std::max(input, 1.0e-30)));
    }
    for (size_t index = 1; index < gains.size(); ++index)
    {
        const double reduction = gains.front() - gains[index];
        if (reduction >= reductionDb)
        {
            const double previous = gains.front() - gains[index - 1];
            const double fraction = (reductionDb - previous) / std::max(reduction - previous, 1.0e-9);
            return levels[index - 1] + fraction * (levels[index] - levels[index - 1]);
        }
    }
    return std::numeric_limits<double>::quiet_NaN();
}

DynamicTransfer measureDynamicTransfer(const Device& device, int channels, double sampleRate, double toneHz, double toneDbfs, double probeDbfs,
                                       const std::vector<double>& probeFrequencies)
{
    DynamicTransfer result;
    const int length = static_cast<int>(1.5 * sampleRate);
    const std::vector<float> tone = makeSine(std::round(toneHz), toneDbfs, length, sampleRate);
    for (const double requested : probeFrequencies)
    {
        const double frequency = std::round(requested); // whole hertz: an integer number of periods in the analysed second
        if (std::abs(std::log2(frequency / toneHz)) < 1.0 / 6.0)
        {
            continue; // the two tones beat; the gain follows the beating and its sidebands fall on the probe
        }
        const std::vector<float> probe = makeSine(frequency, probeDbfs, length, sampleRate);
        std::vector<float> both(probe.size());
        for (size_t sample = 0; sample < both.size(); ++sample)
        {
            both[sample] = probe[sample] + tone[sample];
        }
        const double alone = getToneAmplitude(renderThrough(device, channels, probe, sampleRate), frequency, sampleRate);
        const double together = getToneAmplitude(renderThrough(device, channels, both, sampleRate), frequency, sampleRate);
        result.frequencies.push_back(frequency);
        result.reductionDb.push_back(20.0 * std::log10(std::max(alone, 1.0e-15) / std::max(together, 1.0e-15)));
    }
    if (result.reductionDb.empty())
    {
        return result;
    }
    const double largest = *std::max_element(result.reductionDb.begin(), result.reductionDb.end());
    const double smallest = *std::min_element(result.reductionDb.begin(), result.reductionDb.end());
    constexpr double kNoneDb = 0.5;
    constexpr double kPlateauDb = 3.0;
    constexpr double kBandOctaves = 1.0;
    if (largest < kNoneDb)
    {
        result.kind = BandKind::None;
        return result;
    }
    if (smallest > 0.5 * largest)
    {
        result.kind = BandKind::SingleBand;
        return result;
    }
    // the region within 3 dB of the largest reduction: its width in octaves (lowest to highest probe in it)
    double lowest = 1.0e300;
    double highest = 0.0;
    for (size_t index = 0; index < result.frequencies.size(); ++index)
    {
        if (result.reductionDb[index] >= largest - kPlateauDb)
        {
            lowest = std::min(lowest, result.frequencies[index]);
            highest = std::max(highest, result.frequencies[index]);
        }
    }
    result.plateauOctaves = std::log2(highest / lowest);
    result.kind = BandKind::DynamicEq;
    if (result.plateauOctaves >= kBandOctaves)
    {
        result.kind = BandKind::Multiband;
    }
    return result;
}

double measureStereoLink(const Device& device, double sampleRate, double highDbfs, double lowDbfs)
{
    StaircaseSettings staircase;
    staircase.sampleRate = sampleRate;
    staircase.stepSeconds = 1.0;
    staircase.levelsDbfs = {highDbfs};
    const std::vector<float> loud = makeStaircase(staircase);
    staircase.levelsDbfs = {lowDbfs};
    const std::vector<float> quiet = makeStaircase(staircase);
    // each channel alone at its level (no reduction for the quiet one: the reference), then both together
    const auto gainOf = [&](const std::vector<float>& left, const std::vector<float>& right, int channel)
    {
        juce::AudioBuffer<float> buffer(2, static_cast<int>(left.size()));
        buffer.copyFrom(0, 0, left.data(), static_cast<int>(left.size()));
        buffer.copyFrom(1, 0, right.data(), static_cast<int>(right.size()));
        const juce::AudioBuffer<float> output = device(buffer, sampleRate);
        std::vector<double> out;
        for (int sample = 0; sample < output.getNumSamples(); ++sample)
        {
            out.push_back(output.getSample(channel, sample));
        }
        if (channel == 0)
        {
            return getSteadyGainDb(left, out, 0);
        }
        return getSteadyGainDb(right, out, 0);
    };
    const double quietAlone = gainOf(quiet, quiet, 1);
    const double loudAlone = gainOf(loud, loud, 0);
    const double quietTogether = gainOf(loud, quiet, 1);
    const double loudReduction = quietAlone - loudAlone;
    if (loudReduction < 0.1)
    {
        return 0.0;
    }
    return (quietAlone - quietTogether) / loudReduction;
}

DynamicsDistortion measureDynamicsDistortion(const Device& device, int channels, double sampleRate, double levelDbfs, const std::vector<double>& frequencies)
{
    DynamicsDistortion result;
    DistortionSettings settings;
    settings.sampleRate = sampleRate;
    settings.levelDbfs = levelDbfs;
    settings.channels = channels;
    for (const double frequency : frequencies)
    {
        settings.frequencyHz = frequency;
        const DistortionResult distortion = measureDistortion(device, settings);
        result.frequencies.push_back(distortion.frequencyHz);
        result.thdDb.push_back(distortion.channels.front().thdDb);
    }
    return result;
}

double getSamplePeakDbfs(const std::vector<double>& signal)
{
    double peak = 0.0;
    for (const double value : signal)
    {
        peak = std::max(peak, std::abs(value));
    }
    return 20.0 * std::log10(std::max(peak, 1.0e-15));
}

double getTruePeakDbfs(const std::vector<double>& signal)
{
    constexpr int kFactor = 4;
    constexpr int kZeroCrossings = 48;
    // the interpolator's phases: h_p(k) = sinc(k - p/4) w(k - p/4), k = -16 ... 16
    std::vector<std::vector<double>> phases(kFactor);
    for (int phase = 1; phase < kFactor; ++phase)
    {
        for (int k = -kZeroCrossings; k <= kZeroCrossings; ++k)
        {
            const double x = k - static_cast<double>(phase) / kFactor;
            double sinc = 1.0;
            if (std::abs(x) > 1.0e-12)
            {
                sinc = std::sin(kPi * x) / (kPi * x);
            }
            const double position = (x + kZeroCrossings + 1) / (2.0 * kZeroCrossings + 2);
            const double window = 0.42 - 0.5 * std::cos(2.0 * kPi * position) + 0.08 * std::cos(4.0 * kPi * position);
            phases[static_cast<size_t>(phase)].push_back(sinc * window);
        }
        // unity gain at DC for every phase
        double sum = 0.0;
        for (const double tap : phases[static_cast<size_t>(phase)])
        {
            sum += tap;
        }
        for (double& tap : phases[static_cast<size_t>(phase)])
        {
            tap /= sum;
        }
    }
    double peak = 0.0;
    const long long length = static_cast<long long>(signal.size());
    for (long long sample = 0; sample < length; ++sample)
    {
        peak = std::max(peak, std::abs(signal[static_cast<size_t>(sample)]));
        // the values between sample and sample + 1, where the interpolator has all its taps inside the signal (outside it the zeros would be
        // an abrupt edge of their own, ringing by a per cent)
        if (sample < kZeroCrossings || sample + kZeroCrossings >= length)
        {
            continue;
        }
        for (int phase = 1; phase < kFactor; ++phase)
        {
            double value = 0.0;
            const std::vector<double>& taps = phases[static_cast<size_t>(phase)];
            for (int k = -kZeroCrossings; k <= kZeroCrossings; ++k)
            {
                value += signal[static_cast<size_t>(sample + k)] * taps[static_cast<size_t>(k + kZeroCrossings)]; // x(n + k) sinc(k - p/4) w
            }
            peak = std::max(peak, std::abs(value));
        }
    }
    return 20.0 * std::log10(std::max(peak, 1.0e-15));
}

LimiterTest measureLimiter(const Device& device, int channels, double sampleRate, double driveDbfs)
{
    LimiterTest result;
    // the sine at fs/4 with 45 degrees: samples +-a/sqrt(2)
    const int length = static_cast<int>(sampleRate);
    const double amplitude = std::pow(10.0, driveDbfs / 20.0);
    std::vector<float> sine(static_cast<size_t>(length));
    for (int sample = 0; sample < length; ++sample)
    {
        sine[static_cast<size_t>(sample)] = static_cast<float>(amplitude * std::sin(kPi / 2.0 * sample + kPi / 4.0));
    }
    const std::vector<double> output = renderThrough(device, channels, sine, sampleRate);
    const std::vector<double> steady(output.begin() + static_cast<std::ptrdiff_t>(output.size() / 2), output.end());
    result.samplePeakDbfs = getSamplePeakDbfs(steady);
    result.truePeakDbfs = getTruePeakDbfs(steady);

    // the step: the peak in the first 50 ms after it against the steady peak at its end
    StaircaseSettings staircase;
    staircase.sampleRate = sampleRate;
    staircase.stepSeconds = 0.5;
    staircase.levelsDbfs = {-40.0, driveDbfs};
    const std::vector<double> stepOutput = renderThrough(device, channels, makeStaircase(staircase), sampleRate);
    const size_t step = static_cast<size_t>(0.5 * sampleRate);
    const size_t early = static_cast<size_t>(0.05 * sampleRate);
    const std::vector<double> attack(stepOutput.begin() + static_cast<std::ptrdiff_t>(step), stepOutput.begin() + static_cast<std::ptrdiff_t>(step + early));
    const std::vector<double> settled(stepOutput.end() - static_cast<std::ptrdiff_t>(early), stepOutput.end());
    result.overshootDb = getSamplePeakDbfs(attack) - getSamplePeakDbfs(settled);
    return result;
}
}
