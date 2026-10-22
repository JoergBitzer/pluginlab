#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/measure/Device.h"

namespace pluginlab::match
{
// The general matcher (W9.1, docs/design/W9-plan.md section 2): parameters optimized against an objective. It knows nothing about EQs; a
// domain (the EQ of W9.2, later dynamics or reverb) supplies the probe (what goes through a device and what is read from it), the objective
// (the residual between the master's and the target's features) and, optionally, a start point.

// One parameter the user releases: its index in the setting, its normalised range (a part of 0 ... 1 if the user narrows it) and its start
struct ReleasedParameter
{
    int index = 0;
    juce::String name;
    double lower = 0.0;
    double upper = 1.0;
    double start = 0.5;
};

// The released parameters and the setting the others keep (the locked and the ignored ones, as the user left them)
struct ParameterSpace
{
    std::vector<float> base;                     // the whole setting, normalised
    std::vector<ReleasedParameter> released;

    size_t size() const;
    std::vector<double> getStart() const;
    std::vector<float> makeSetting(const std::vector<double>& values) const; // the released values (clamped to their ranges) into the base
};

// What a domain measures: a stimulus through a device, a feature vector out (an EQ: the magnitude response in dB on a grid)
class Probe
{
public:
    virtual ~Probe() = default;
    virtual std::vector<double> measure(const measure::Device& device) const = 0;
    virtual juce::String describe() const = 0;
};

// How far the target's features are from the master's: the residual vector (for least squares) and whether it is within the tolerance
class Objective
{
public:
    virtual ~Objective() = default;
    virtual std::vector<double> getResidual(const std::vector<double>& master, const std::vector<double>& target) const = 0;
    virtual bool isWithinTolerance(const std::vector<double>& residual) const = 0;
    virtual juce::String describe() const = 0;
};

// The residual weighted per feature: w (target - master); within the tolerance when the largest unweighted difference is below it
class WeightedDifference : public Objective
{
public:
    WeightedDifference(std::vector<double> weights, double tolerance);
    std::vector<double> getResidual(const std::vector<double>& master, const std::vector<double>& target) const override;
    bool isWithinTolerance(const std::vector<double>& residual) const override;
    juce::String describe() const override;
    double getTolerance() const { return m_tolerance; }

private:
    std::vector<double> m_weights;
    double m_tolerance;
};

// The budget of one match (the author: 10 ... 15 s per target, no parallel renders)
struct Budget
{
    int evaluations = 200;
    double seconds = 15.0;
};

enum class StopReason
{
    ToleranceReached,
    Converged,          // no further improvement
    EvaluationBudget,
    TimeBudget,
    NotStarted
};

const char* getStopReasonName(StopReason reason);

struct OptimizerResult
{
    std::vector<double> values;          // the best released values
    std::vector<double> residual;        // there
    double rms = 0.0;                    // of the residual
    double largest = 0.0;                // largest absolute residual
    bool withinTolerance = false;
    std::vector<bool> atBound;           // per released parameter: within 1 % of its range of a bound
    int evaluations = 0;                 // renders (cached ones not counted)
    int iterations = 0;
    double seconds = 0.0;
    StopReason reason = StopReason::NotStarted;
    std::vector<double> history;         // rms after each iteration
};

// The residual for released values; the function the optimizer minimizes (sum of squares)
using ResidualFunction = std::function<std::vector<double>(const std::vector<double>& values)>;
using MatchProgress = std::function<void(double fraction, const juce::String& step)>;

struct LevenbergMarquardtSettings
{
    int maximumIterations = 50;
    double initialDamping = 1.0e-2;
    double derivativeStep = 1.0e-3;     // of a parameter's range, for the numerical derivatives
    double relativeImprovement = 1.0e-6; // a smaller relative improvement of the cost: converged
};

// Levenberg-Marquardt on the residual, numerical derivatives (one evaluation per released parameter and iteration), bounded (steps are
// projected onto the ranges); stops at the tolerance, when it converges, or at the budget. The best values seen are the result.
OptimizerResult minimizeLevenbergMarquardt(const ResidualFunction& residual, const std::function<bool(const std::vector<double>&)>& withinTolerance,
                                           const std::vector<double>& start, const std::vector<double>& lower, const std::vector<double>& upper,
                                           const Budget& budget, const LevenbergMarquardtSettings& settings = {}, const MatchProgress& progress = {});

// A match problem: the master's features, a device per setting of the target, the probe, the objective, the parameter space. Settings already
// measured come from a cache.
class MatchProblem
{
public:
    using DeviceFactory = std::function<measure::Device(const std::vector<float>& setting)>;

    MatchProblem(std::vector<double> masterFeatures, DeviceFactory target, std::shared_ptr<const Probe> probe, std::shared_ptr<const Objective> objective,
                 ParameterSpace space);

    std::vector<double> getFeatures(const std::vector<float>& setting); // the target's features (measured or cached)
    std::vector<double> getResidual(const std::vector<double>& values);
    int getMeasurements() const { return m_measurements; }
    const ParameterSpace& getSpace() const { return m_space; }
    const Objective& getObjective() const { return *m_objective; }
    const std::vector<double>& getMasterFeatures() const { return m_master; }

    OptimizerResult solve(const Budget& budget, const LevenbergMarquardtSettings& settings = {}, const MatchProgress& progress = {});

    // A start without a model (W11.3g: no mapping of a compressor yet): one pass over the released parameters, each on `points` evenly spaced
    // values of its range (the others where the pass left them), the best kept; it becomes the start of solve(). Returns the renders it took.
    // It gets Levenberg-Marquardt out of valleys where two parameters trade off (a compressor's threshold and ratio)
    int scanStart(int points);

private:
    std::vector<double> m_master;
    DeviceFactory m_target;
    std::shared_ptr<const Probe> m_probe;
    std::shared_ptr<const Objective> m_objective;
    ParameterSpace m_space;
    std::map<std::vector<float>, std::vector<double>> m_cache;
    int m_measurements = 0;
};
}
