#include "pluginlab/match/Match.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace pluginlab::match
{
namespace
{
constexpr double kAtBound = 0.01;           // of a range: a value this close to a bound counts as at the bound
constexpr double kMaximumDamping = 1.0e10;
constexpr double kDampingDown = 0.3;
constexpr double kDampingUp = 10.0;
constexpr double kTinyPivot = 1.0e-300;

double getSumOfSquares(const std::vector<double>& residual)
{
    double sum = 0.0;
    for (const double value : residual)
    {
        sum += value * value;
    }
    return sum;
}

double getRms(const std::vector<double>& residual)
{
    return residual.empty() ? 0.0 : std::sqrt(getSumOfSquares(residual) / static_cast<double>(residual.size()));
}

double getLargest(const std::vector<double>& residual)
{
    double largest = 0.0;
    for (const double value : residual)
    {
        largest = std::max(largest, std::abs(value));
    }
    return largest;
}

// A x = b for a small symmetric system, Gaussian elimination with partial pivoting; false if it is singular
bool solve(std::vector<std::vector<double>> a, std::vector<double> b, std::vector<double>& x)
{
    const size_t n = b.size();
    for (size_t column = 0; column < n; ++column)
    {
        size_t pivot = column;
        for (size_t row = column + 1; row < n; ++row)
        {
            if (std::abs(a[row][column]) > std::abs(a[pivot][column]))
            {
                pivot = row;
            }
        }
        if (std::abs(a[pivot][column]) < kTinyPivot)
        {
            return false;
        }
        std::swap(a[pivot], a[column]);
        std::swap(b[pivot], b[column]);
        for (size_t row = column + 1; row < n; ++row)
        {
            const double factor = a[row][column] / a[column][column];
            for (size_t k = column; k < n; ++k)
            {
                a[row][k] -= factor * a[column][k];
            }
            b[row] -= factor * b[column];
        }
    }
    x.assign(n, 0.0);
    for (size_t row = n; row-- > 0;)
    {
        double sum = b[row];
        for (size_t k = row + 1; k < n; ++k)
        {
            sum -= a[row][k] * x[k];
        }
        x[row] = sum / a[row][row];
    }
    return true;
}
}

size_t ParameterSpace::size() const
{
    return released.size();
}

std::vector<double> ParameterSpace::getStart() const
{
    std::vector<double> start;
    for (const ReleasedParameter& parameter : released)
    {
        start.push_back(juce::jlimit(parameter.lower, parameter.upper, parameter.start));
    }
    return start;
}

std::vector<float> ParameterSpace::makeSetting(const std::vector<double>& values) const
{
    std::vector<float> setting = base;
    for (size_t index = 0; index < released.size() && index < values.size(); ++index)
    {
        const ReleasedParameter& parameter = released[index];
        if (parameter.index >= 0 && parameter.index < static_cast<int>(setting.size()))
        {
            setting[static_cast<size_t>(parameter.index)] = static_cast<float>(juce::jlimit(parameter.lower, parameter.upper, values[index]));
        }
    }
    return setting;
}

WeightedDifference::WeightedDifference(std::vector<double> weights, double tolerance)
    : m_weights(std::move(weights)), m_tolerance(tolerance)
{
}

std::vector<double> WeightedDifference::getResidual(const std::vector<double>& master, const std::vector<double>& target) const
{
    std::vector<double> residual(std::min(master.size(), target.size()));
    for (size_t index = 0; index < residual.size(); ++index)
    {
        const double weight = index < m_weights.size() ? m_weights[index] : 1.0;
        residual[index] = weight * (target[index] - master[index]);
    }
    return residual;
}

bool WeightedDifference::isWithinTolerance(const std::vector<double>& residual) const
{
    for (size_t index = 0; index < residual.size(); ++index)
    {
        const double weight = index < m_weights.size() ? m_weights[index] : 1.0;
        if (weight > 0.0 && std::abs(residual[index] / weight) > m_tolerance)
        {
            return false;
        }
    }
    return true;
}

juce::String WeightedDifference::describe() const
{
    return "weighted difference, tolerance " + juce::String(m_tolerance, 3) + " (largest unweighted difference)";
}

const char* getStopReasonName(StopReason reason)
{
    switch (reason)
    {
        case StopReason::ToleranceReached:
            return "tolerance reached";
        case StopReason::Converged:
            return "converged (no further improvement)";
        case StopReason::EvaluationBudget:
            return "budget of evaluations used up";
        case StopReason::TimeBudget:
            return "time budget used up";
        case StopReason::NotStarted:
            return "not started";
    }
    return "";
}

OptimizerResult minimizeLevenbergMarquardt(const ResidualFunction& residual, const std::function<bool(const std::vector<double>&)>& withinTolerance,
                                           const std::vector<double>& start, const std::vector<double>& lower, const std::vector<double>& upper,
                                           const Budget& budget, const LevenbergMarquardtSettings& settings, const MatchProgress& progress)
{
    const double started = juce::Time::getMillisecondCounterHiRes();
    const size_t n = start.size();
    OptimizerResult result;
    const auto elapsed = [started] { return (juce::Time::getMillisecondCounterHiRes() - started) / 1000.0; };
    const auto clamp = [&](std::vector<double> values) {
        for (size_t i = 0; i < n; ++i)
        {
            values[i] = juce::jlimit(lower[i], upper[i], values[i]);
        }
        return values;
    };
    const auto evaluate = [&](const std::vector<double>& values) {
        ++result.evaluations;
        return residual(values);
    };
    const auto outOfBudget = [&]() -> bool {
        if (result.evaluations >= budget.evaluations)
        {
            result.reason = StopReason::EvaluationBudget;
            return true;
        }
        if (elapsed() >= budget.seconds)
        {
            result.reason = StopReason::TimeBudget;
            return true;
        }
        return false;
    };

    std::vector<double> x = clamp(start);
    std::vector<double> r = evaluate(x);
    double cost = getSumOfSquares(r);
    result.values = x;
    result.residual = r;
    double damping = settings.initialDamping;
    result.reason = StopReason::Converged;
    for (int iteration = 0; iteration < settings.maximumIterations; ++iteration)
    {
        if (withinTolerance(r))
        {
            result.reason = StopReason::ToleranceReached;
            break;
        }
        if (n == 0 || outOfBudget())
        {
            break;
        }
        if (progress)
        {
            progress(std::min(1.0, std::max(static_cast<double>(result.evaluations) / budget.evaluations, elapsed() / budget.seconds)),
                     "iteration " + juce::String(iteration + 1) + ", rms " + juce::String(getRms(r), 4));
        }
        // numerical Jacobian: a forward step per parameter (backwards at the upper bound)
        std::vector<std::vector<double>> jacobian(r.size(), std::vector<double>(n, 0.0));
        bool budgetEnded = false;
        for (size_t j = 0; j < n; ++j)
        {
            if (outOfBudget())
            {
                budgetEnded = true;
                break;
            }
            double step = settings.derivativeStep * std::max(upper[j] - lower[j], 1.0e-12);
            if (x[j] + step > upper[j])
            {
                step = -step;
            }
            std::vector<double> shifted = x;
            shifted[j] += step;
            const std::vector<double> rs = evaluate(shifted);
            for (size_t i = 0; i < r.size() && i < rs.size(); ++i)
            {
                jacobian[i][j] = (rs[i] - r[i]) / step;
            }
        }
        if (budgetEnded)
        {
            break;
        }
        // normal equations J^T J and the gradient J^T r
        std::vector<std::vector<double>> normal(n, std::vector<double>(n, 0.0));
        std::vector<double> gradient(n, 0.0);
        for (size_t i = 0; i < r.size(); ++i)
        {
            for (size_t a = 0; a < n; ++a)
            {
                gradient[a] += jacobian[i][a] * r[i];
                for (size_t b = 0; b < n; ++b)
                {
                    normal[a][b] += jacobian[i][a] * jacobian[i][b];
                }
            }
        }
        bool improved = false;
        while (damping < kMaximumDamping && ! improved)
        {
            if (outOfBudget())
            {
                budgetEnded = true;
                break;
            }
            std::vector<std::vector<double>> damped = normal;
            for (size_t a = 0; a < n; ++a)
            {
                damped[a][a] += damping * std::max(normal[a][a], 1.0e-12);
            }
            std::vector<double> negative(n);
            for (size_t a = 0; a < n; ++a)
            {
                negative[a] = -gradient[a];
            }
            std::vector<double> delta;
            if (! solve(damped, negative, delta))
            {
                damping *= kDampingUp;
                continue;
            }
            std::vector<double> next = x;
            for (size_t a = 0; a < n; ++a)
            {
                next[a] += delta[a];
            }
            next = clamp(next);
            if (next == x)
            {
                damping *= kDampingUp; // the step was cut away by the bounds entirely
                continue;
            }
            const std::vector<double> rn = evaluate(next);
            const double nextCost = getSumOfSquares(rn);
            if (nextCost < cost)
            {
                const double relative = (cost - nextCost) / std::max(cost, 1.0e-300);
                x = next;
                r = rn;
                cost = nextCost;
                damping = std::max(damping * kDampingDown, 1.0e-12);
                improved = true;
                if (relative < settings.relativeImprovement)
                {
                    damping = kMaximumDamping; // converged: leave the outer loop below
                }
            }
            else
            {
                damping *= kDampingUp;
            }
        }
        ++result.iterations;
        result.history.push_back(getRms(r));
        if (budgetEnded || ! improved || damping >= kMaximumDamping)
        {
            if (withinTolerance(r))
            {
                result.reason = StopReason::ToleranceReached;
            }
            break;
        }
    }
    result.values = x;
    result.residual = r;
    result.rms = getRms(r);
    result.largest = getLargest(r);
    result.withinTolerance = withinTolerance(r);
    if (result.withinTolerance)
    {
        result.reason = StopReason::ToleranceReached;
    }
    result.atBound.resize(n);
    for (size_t i = 0; i < n; ++i)
    {
        const double range = upper[i] - lower[i];
        result.atBound[i] = range > 0.0 && (x[i] - lower[i] < kAtBound * range || upper[i] - x[i] < kAtBound * range);
    }
    result.seconds = elapsed();
    if (progress)
    {
        progress(1.0, getStopReasonName(result.reason));
    }
    return result;
}

MatchProblem::MatchProblem(std::vector<double> masterFeatures, DeviceFactory target, std::shared_ptr<const Probe> probe,
                           std::shared_ptr<const Objective> objective, ParameterSpace space)
    : m_master(std::move(masterFeatures)), m_target(std::move(target)), m_probe(std::move(probe)), m_objective(std::move(objective)), m_space(std::move(space))
{
}

std::vector<double> MatchProblem::getFeatures(const std::vector<float>& setting)
{
    const auto found = m_cache.find(setting);
    if (found != m_cache.end())
    {
        return found->second;
    }
    ++m_measurements;
    std::vector<double> features = m_probe->measure(m_target(setting));
    m_cache.emplace(setting, features);
    return features;
}

std::vector<double> MatchProblem::getResidual(const std::vector<double>& values)
{
    return m_objective->getResidual(m_master, getFeatures(m_space.makeSetting(values)));
}

OptimizerResult MatchProblem::solve(const Budget& budget, const LevenbergMarquardtSettings& settings, const MatchProgress& progress)
{
    std::vector<double> lower;
    std::vector<double> upper;
    for (const ReleasedParameter& parameter : m_space.released)
    {
        lower.push_back(parameter.lower);
        upper.push_back(parameter.upper);
    }
    const int before = m_measurements;
    OptimizerResult result = minimizeLevenbergMarquardt([this](const std::vector<double>& values) { return getResidual(values); },
                                                        [this](const std::vector<double>& residual) { return m_objective->isWithinTolerance(residual); },
                                                        m_space.getStart(), lower, upper, budget, settings, progress);
    result.evaluations = m_measurements - before; // renders, without the ones the cache answered
    return result;
}
}
