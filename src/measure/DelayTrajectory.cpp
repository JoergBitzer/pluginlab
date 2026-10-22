#include "pluginlab/measure/DelayTrajectory.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include <juce_dsp/juce_dsp.h>

#include "pluginlab/measure/EchoResponse.h"

namespace pluginlab::measure
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kGridPerDuration = 8.0;   // the rate grid: 1 / (8 T)
constexpr double kHighestRateHz = 50.0;
constexpr double kSamplingShare = 0.45;    // the rates searched up to this share of the points' sampling rate
constexpr int kRefineSteps = 60;           // golden-section steps
constexpr int kPhaseSteps = 720;
constexpr int kToneStep = 16;              // the tone's trajectory: every 16th sample
constexpr double kGolden = 0.6180339887498949;

double getTriangle(double phase)
{
    return 2.0 / kPi * std::asin(std::sin(phase));
}

struct LinearFit
{
    std::vector<double> coefficients;
    double residual = 0.0; // sum of squares
};

// least squares y = sum_k c_k basis_k(t): normal equations solved by Gauss elimination (2 or 3 unknowns)
template <class Basis>
LinearFit fitLinear(const std::vector<TrajectoryPoint>& points, int unknowns, Basis basis)
{
    std::vector<std::vector<double>> normal(static_cast<size_t>(unknowns), std::vector<double>(static_cast<size_t>(unknowns + 1), 0.0));
    std::vector<double> row(static_cast<size_t>(unknowns));
    for (const TrajectoryPoint& point : points)
    {
        basis(point.timeSamples, row);
        for (size_t i = 0; i < row.size(); ++i)
        {
            for (size_t j = 0; j < row.size(); ++j)
            {
                normal[i][j] += row[i] * row[j];
            }
            normal[i][row.size()] += row[i] * point.delaySamples;
        }
    }
    const size_t size = row.size();
    for (size_t pivot = 0; pivot < size; ++pivot)
    {
        size_t best = pivot;
        for (size_t candidate = pivot + 1; candidate < size; ++candidate)
        {
            if (std::abs(normal[candidate][pivot]) > std::abs(normal[best][pivot]))
            {
                best = candidate;
            }
        }
        std::swap(normal[pivot], normal[best]);
        if (std::abs(normal[pivot][pivot]) < 1.0e-300)
        {
            continue;
        }
        for (size_t other = 0; other < size; ++other)
        {
            if (other == pivot)
            {
                continue;
            }
            const double factor = normal[other][pivot] / normal[pivot][pivot];
            for (size_t column = pivot; column <= size; ++column)
            {
                normal[other][column] -= factor * normal[pivot][column];
            }
        }
    }
    LinearFit fit;
    fit.coefficients.assign(size, 0.0);
    for (size_t i = 0; i < size; ++i)
    {
        if (std::abs(normal[i][i]) > 1.0e-300)
        {
            fit.coefficients[i] = normal[i][size] / normal[i][i];
        }
    }
    for (const TrajectoryPoint& point : points)
    {
        basis(point.timeSamples, row);
        double model = 0.0;
        for (size_t i = 0; i < size; ++i)
        {
            model += fit.coefficients[i] * row[i];
        }
        fit.residual += (point.delaySamples - model) * (point.delaySamples - model);
    }
    return fit;
}

LinearFit fitSine(const std::vector<TrajectoryPoint>& points, double omega)
{
    return fitLinear(points, 3, [omega](double t, std::vector<double>& row)
                     {
                         row[0] = 1.0;
                         row[1] = std::cos(omega * t);
                         row[2] = std::sin(omega * t);
                     });
}

LinearFit fitTriangle(const std::vector<TrajectoryPoint>& points, double omega, double phase)
{
    return fitLinear(points, 2, [omega, phase](double t, std::vector<double>& row)
                     {
                         row[0] = 1.0;
                         row[1] = getTriangle(omega * t + phase);
                     });
}

// the minimum of f on [low, high] (golden section)
template <class Function>
double findMinimum(double low, double high, Function function)
{
    double a = high - kGolden * (high - low);
    double b = low + kGolden * (high - low);
    double fa = function(a);
    double fb = function(b);
    for (int step = 0; step < kRefineSteps; ++step)
    {
        if (fa < fb)
        {
            high = b;
            b = a;
            fb = fa;
            a = high - kGolden * (high - low);
            fa = function(a);
        }
        else
        {
            low = a;
            a = b;
            fa = fb;
            b = low + kGolden * (high - low);
            fb = function(b);
        }
    }
    return 0.5 * (low + high);
}

int getOrderAtLeast(int samples)
{
    int order = 0;
    while ((1 << order) < samples)
    {
        ++order;
    }
    return order;
}

// the analytic signal by FFT: the negative frequencies removed, the positive ones doubled
std::vector<std::complex<double>> getAnalytic(const std::vector<double>& signal)
{
    const int length = static_cast<int>(signal.size());
    const int order = getOrderAtLeast(length);
    const int size = 1 << order;
    juce::dsp::FFT fft(order);
    std::vector<juce::dsp::Complex<float>> time(static_cast<size_t>(size)), spectrum(static_cast<size_t>(size));
    for (int sample = 0; sample < length; ++sample)
    {
        time[static_cast<size_t>(sample)] = static_cast<float>(signal[static_cast<size_t>(sample)]);
    }
    fft.perform(time.data(), spectrum.data(), false);
    for (int bin = 1; bin < size / 2; ++bin)
    {
        spectrum[static_cast<size_t>(bin)] *= 2.0f;
    }
    for (int bin = size / 2 + 1; bin < size; ++bin)
    {
        spectrum[static_cast<size_t>(bin)] = 0.0f;
    }
    fft.perform(spectrum.data(), time.data(), true);
    std::vector<std::complex<double>> analytic(static_cast<size_t>(length));
    for (int sample = 0; sample < length; ++sample)
    {
        analytic[static_cast<size_t>(sample)] = {time[static_cast<size_t>(sample)].real(), time[static_cast<size_t>(sample)].imag()};
    }
    return analytic;
}

double getMedian(std::vector<double> values)
{
    if (values.empty())
    {
        return 0.0;
    }
    const size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(middle), values.end());
    return values[middle];
}
}

std::vector<float> makeImpulseTrain(int count, int periodSamples, float level)
{
    std::vector<float> signal(static_cast<size_t>(count) * static_cast<size_t>(periodSamples), 0.0f);
    for (int index = 0; index < count; ++index)
    {
        signal[static_cast<size_t>(index) * static_cast<size_t>(periodSamples)] = level;
    }
    return signal;
}

DelayTrajectory measureTrajectoryFromImpulses(const std::vector<double>& output, int count, int periodSamples, const TrajectorySettings& settings)
{
    std::vector<TrajectoryPoint> points;
    EchoSettings echoSettings;
    echoSettings.sampleRate = settings.sampleRate;
    echoSettings.latencySamples = settings.latencySamples;
    for (int index = 0; index < count; ++index)
    {
        const size_t start = static_cast<size_t>(index) * static_cast<size_t>(periodSamples);
        if (start >= output.size())
        {
            break;
        }
        const size_t end = std::min(output.size(), start + static_cast<size_t>(periodSamples));
        const std::vector<double> segment(output.begin() + static_cast<std::ptrdiff_t>(start), output.begin() + static_cast<std::ptrdiff_t>(end));
        const EchoAnalysis analysis = analyseEchoes(segment, echoSettings);
        if (analysis.echoes.empty())
        {
            continue;
        }
        double delay = analysis.echoes.front().timeSamples;
        if (settings.expectedDelaySamples > 0.0)
        {
            for (const Arrival& echo : analysis.echoes)
            {
                if (std::abs(echo.timeSamples - settings.expectedDelaySamples) < std::abs(delay - settings.expectedDelaySamples))
                {
                    delay = echo.timeSamples;
                }
            }
        }
        points.push_back({static_cast<double>(start) + delay, delay});
    }
    return fitTrajectory(std::move(points), settings);
}

std::vector<float> makeTone(double frequencyHz, int length, double sampleRate, float level)
{
    std::vector<float> signal(static_cast<size_t>(length));
    for (int sample = 0; sample < length; ++sample)
    {
        signal[static_cast<size_t>(sample)] = level * static_cast<float>(std::sin(2.0 * kPi * frequencyHz * sample / sampleRate));
    }
    return signal;
}

DelayTrajectory measureTrajectoryFromTone(const std::vector<double>& output, double frequencyHz, double roughDelaySamples, int edgeSamples,
                                          const TrajectorySettings& settings)
{
    const int length = static_cast<int>(output.size());
    const std::vector<std::complex<double>> time = getAnalytic(output);

    // y = sin(w (n - tau)): the analytic phase is w (n - tau) - pi/2, so tau = n - (phase + pi/2) / w, up to whole periods of the tone
    const double omega = 2.0 * kPi * frequencyHz / settings.sampleRate;
    std::vector<TrajectoryPoint> points;
    double previous = 0.0;
    double unwrap = 0.0;
    bool first = true;
    for (int sample = edgeSamples; sample < length - edgeSamples; ++sample)
    {
        double phase = std::arg(time[static_cast<size_t>(sample)]);
        // unwrap against the phase the tone would have advanced
        if (! first)
        {
            const double expected = previous + omega;
            unwrap += 2.0 * kPi * std::round((expected - (phase + unwrap)) / (2.0 * kPi));
        }
        phase += unwrap;
        previous = phase;
        first = false;
        if ((sample - edgeSamples) % kToneStep == 0)
        {
            points.push_back({static_cast<double>(sample), sample - (phase + kPi / 2.0) / omega});
        }
    }
    if (points.empty())
    {
        return {};
    }
    // whole periods: the mean moved next to the rough delay
    double mean = 0.0;
    for (const TrajectoryPoint& point : points)
    {
        mean += point.delaySamples;
    }
    mean /= static_cast<double>(points.size());
    const double period = settings.sampleRate / frequencyHz;
    const double shift = period * std::round((roughDelaySamples - mean) / period);
    for (TrajectoryPoint& point : points)
    {
        point.delaySamples += shift;
    }
    return fitTrajectory(std::move(points), settings);
}

DelayTrajectory fitTrajectory(std::vector<TrajectoryPoint> points, const TrajectorySettings& settings)
{
    DelayTrajectory result;
    result.points = std::move(points);
    const std::vector<TrajectoryPoint>& data = result.points;
    constexpr size_t kFewestPoints = 4;
    if (data.size() < kFewestPoints)
    {
        return result;
    }
    double mean = 0.0;
    for (const TrajectoryPoint& point : data)
    {
        mean += point.delaySamples;
    }
    mean /= static_cast<double>(data.size());
    result.meanSamples = mean;
    double largest = 0.0;
    for (const TrajectoryPoint& point : data)
    {
        largest = std::max(largest, std::abs(point.delaySamples - mean));
    }
    if (largest < settings.minimumDepthSamples)
    {
        return result;
    }

    // the rate: a grid up to the points' sampling limit, then refined
    const double duration = (data.back().timeSamples - data.front().timeSamples) / settings.sampleRate;
    const double interval = (data.back().timeSamples - data.front().timeSamples) / static_cast<double>(data.size() - 1) / settings.sampleRate;
    const double highest = std::min(kHighestRateHz, kSamplingShare / interval);
    const double step = 1.0 / (kGridPerDuration * duration);
    const auto toOmega = [&settings](double rate) { return 2.0 * kPi * rate / settings.sampleRate; };
    double bestRate = settings.lowestRateHz;
    double bestResidual = 1.0e300;
    for (double rate = settings.lowestRateHz; rate <= highest; rate += step)
    {
        const double residual = fitSine(data, toOmega(rate)).residual;
        if (residual < bestResidual)
        {
            bestResidual = residual;
            bestRate = rate;
        }
    }
    const double rate = findMinimum(std::max(settings.lowestRateHz * 0.5, bestRate - step), bestRate + step,
                                    [&](double candidate) { return fitSine(data, toOmega(candidate)).residual; });
    const LinearFit sine = fitSine(data, toOmega(rate));
    const double count = static_cast<double>(data.size());
    result.rateHz = rate;
    result.meanSamples = sine.coefficients[0];
    const double sineDepth = std::hypot(sine.coefficients[1], sine.coefficients[2]);
    result.sineResidualSamples = std::sqrt(sine.residual / count);

    // the triangle of that rate: the phase on a grid, then refined
    double bestPhase = 0.0;
    double bestTriangle = 1.0e300;
    for (int index = 0; index < kPhaseSteps; ++index)
    {
        const double phase = 2.0 * kPi * index / kPhaseSteps;
        const double residual = fitTriangle(data, toOmega(rate), phase).residual;
        if (residual < bestTriangle)
        {
            bestTriangle = residual;
            bestPhase = phase;
        }
    }
    const double phaseStep = 2.0 * kPi / kPhaseSteps;
    const double phase = findMinimum(bestPhase - phaseStep, bestPhase + phaseStep,
                                     [&](double candidate) { return fitTriangle(data, toOmega(rate), candidate).residual; });
    const LinearFit triangle = fitTriangle(data, toOmega(rate), phase);
    const double triangleDepth = std::abs(triangle.coefficients[1]);
    result.triangleResidualSamples = std::sqrt(triangle.residual / count);

    // the shape: the better fit if it is good enough
    double slope = 2.0 * kPi * rate * sineDepth / settings.sampleRate; // dtau/dt of the sine, per sample
    result.depthSamples = sineDepth;
    result.shape = LfoShapeFound::Other;
    if (result.sineResidualSamples <= result.triangleResidualSamples && result.sineResidualSamples < settings.shapeResidualShare * sineDepth)
    {
        result.shape = LfoShapeFound::Sine;
    }
    if (result.triangleResidualSamples < result.sineResidualSamples && result.triangleResidualSamples < settings.shapeResidualShare * triangleDepth)
    {
        result.shape = LfoShapeFound::Triangle;
        result.depthSamples = triangleDepth;
        result.meanSamples = triangle.coefficients[0];
        slope = 4.0 * rate * triangleDepth / settings.sampleRate;
    }
    // the larger of the two pitch deviations (the delay growing: lower; shrinking: higher)
    if (slope < 1.0)
    {
        result.pitchDeviationCents = 1200.0 * std::log2(1.0 / (1.0 - slope));
    }
    return result;
}

TimeChangeAnalysis analyseTimeChange(const std::vector<double>& output, double frequencyHz, double roughDelayBefore, double roughDelayAfter, int edgeSamples,
                                     double sampleRate)
{
    // the phase has left its value: 10 % of its whole change, within these limits (the analytic signal's tail after a step falls like 1/n)
    constexpr double kLowestTolerance = 0.005;
    constexpr double kHighestTolerance = 0.05;
    constexpr int kJumpSamples = 64;          // the analytic signal spreads a phase step over some samples (1.3 ms at 48 kHz)
    constexpr double kDipDb = -0.5;
    constexpr double kGlideResidual = 0.05;  // radians
    TimeChangeAnalysis result;
    const int length = static_cast<int>(output.size());
    const int first = edgeSamples;
    const int last = length - edgeSamples;
    const int quarter = (last - first) / 4;
    if (quarter < 1)
    {
        return result;
    }
    const std::vector<std::complex<double>> analytic = getAnalytic(output);
    const double omega = 2.0 * kPi * frequencyHz / sampleRate;
    // r(n) = phase(n) - w n, unwrapped against the tone's own advance
    std::vector<double> residual(static_cast<size_t>(length), 0.0);
    std::vector<double> envelope(static_cast<size_t>(length), 0.0);
    double previous = 0.0;
    for (int sample = first; sample < last; ++sample)
    {
        const double expected = previous + omega;
        double phase = std::arg(analytic[static_cast<size_t>(sample)]);
        if (sample > first)
        {
            phase += 2.0 * kPi * std::round((expected - phase) / (2.0 * kPi));
        }
        previous = phase;
        residual[static_cast<size_t>(sample)] = phase - omega * sample;
        envelope[static_cast<size_t>(sample)] = std::abs(analytic[static_cast<size_t>(sample)]);
    }
    const auto median = [&](const std::vector<double>& values, int from, int to)
    { return getMedian(std::vector<double>(values.begin() + from, values.begin() + to)); };
    const double before = median(residual, first, first + quarter);
    const double after = median(residual, last - quarter, last);
    const double level = median(envelope, first, first + quarter);

    // the delays: tau = -(r + pi/2) / w, whole periods from the rough delays
    const double period = sampleRate / frequencyHz;
    const auto toDelay = [&](double phaseResidual, double rough)
    {
        const double tau = -(phaseResidual + kPi / 2.0) / omega;
        return tau + period * std::round((rough - tau) / period);
    };
    result.delayBeforeSamples = toDelay(before, roughDelayBefore);
    result.delayAfterSamples = toDelay(after, roughDelayAfter);

    const double tolerance = std::clamp(0.1 * std::abs(after - before), kLowestTolerance, kHighestTolerance);
    int start = -1;
    int end = -1;
    for (int sample = first; sample < last; ++sample)
    {
        if (start < 0 && std::abs(residual[static_cast<size_t>(sample)] - before) > tolerance)
        {
            start = sample;
        }
        if (std::abs(residual[static_cast<size_t>(sample)] - after) > tolerance)
        {
            end = sample;
        }
    }
    if (start < 0 || end < start)
    {
        return result;
    }
    result.startSamples = start;
    result.lengthSamples = end - start + 1;
    double lowest = level;
    for (int sample = start; sample <= end; ++sample)
    {
        lowest = std::min(lowest, envelope[static_cast<size_t>(sample)]);
    }
    result.envelopeDipDb = 20.0 * std::log10(std::max(lowest, 1.0e-12) / level);
    if (result.lengthSamples <= kJumpSamples)
    {
        result.kind = TimeChangeKind::Jump;
        return result;
    }
    // a straight line through the middle 80 % of the change
    const int margin = static_cast<int>(0.1 * result.lengthSamples);
    std::vector<TrajectoryPoint> points;
    for (int sample = start + margin; sample <= end - margin; ++sample)
    {
        points.push_back({static_cast<double>(sample), residual[static_cast<size_t>(sample)]});
    }
    double meanT = 0.0;
    double meanR = 0.0;
    for (const TrajectoryPoint& point : points)
    {
        meanT += point.timeSamples;
        meanR += point.delaySamples;
    }
    meanT /= static_cast<double>(points.size());
    meanR /= static_cast<double>(points.size());
    double covariance = 0.0;
    double variance = 0.0;
    for (const TrajectoryPoint& point : points)
    {
        covariance += (point.timeSamples - meanT) * (point.delaySamples - meanR);
        variance += (point.timeSamples - meanT) * (point.timeSamples - meanT);
    }
    const double slope = covariance / variance;
    double squares = 0.0;
    for (const TrajectoryPoint& point : points)
    {
        const double model = meanR + slope * (point.timeSamples - meanT);
        squares += (point.delaySamples - model) * (point.delaySamples - model);
    }
    result.glideResidualRadians = std::sqrt(squares / static_cast<double>(points.size()));
    // a glide moves the phase by the whole w (tau_after - tau_before), many turns; a crossfade only by the wrapped difference (at most pi)
    const double unexplained = std::abs((after - before) + omega * (result.delayAfterSamples - result.delayBeforeSamples));
    constexpr double kUnexplained = 0.5; // radians
    if (result.envelopeDipDb > kDipDb && result.glideResidualRadians < kGlideResidual && unexplained < kUnexplained)
    {
        result.kind = TimeChangeKind::Glide;
        result.glidePitchCents = 1200.0 * std::log2(1.0 + slope / omega); // f_out / f = 1 + r'/w = 1 - dtau/dt
        return result;
    }
    result.kind = TimeChangeKind::Crossfade;
    return result;
}
}
