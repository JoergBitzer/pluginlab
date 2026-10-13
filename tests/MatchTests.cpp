#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/match/Match.h"

namespace
{
namespace match = pluginlab::match;

constexpr double kTolerance = 1.0e-6;

// a probe that reads the setting back from a "device" that ignores its input: the features are the first samples of its output
class SettingProbe : public match::Probe
{
public:
    std::vector<double> measure(const pluginlab::measure::Device& device) const override
    {
        juce::AudioBuffer<float> input(1, 8);
        input.clear();
        const juce::AudioBuffer<float> output = device(input, 48000.0);
        std::vector<double> features;
        for (int index = 0; index < output.getNumSamples(); ++index)
        {
            features.push_back(output.getSample(0, index));
        }
        return features;
    }

    juce::String describe() const override
    {
        return "the setting itself";
    }
};
}

// W9.1: the general layer of the matcher (docs/design/W9-plan.md): Levenberg-Marquardt with bounds and a budget, the parameter space, the
// match problem with its cache. No plugin, no EQ.
class MatchTests : public juce::UnitTest
{
public:
    MatchTests()
        : juce::UnitTest("Matcher (W9.1)", "pluginlab")
    {
    }

    void runTest() override
    {
        const auto tolerance = [](double limit) { return [limit](const std::vector<double>& r) {
            for (const double value : r)
            {
                if (std::abs(value) > limit)
                {
                    return false;
                }
            }
            return true;
        }; };

        beginTest("linear least squares: the exact solution in a few iterations");
        {
            // r = A x - b with the solution x = (0.3, 0.7)
            const auto residual = [](const std::vector<double>& x) {
                return std::vector<double>{2.0 * x[0] + x[1] - 1.3, x[0] - x[1] + 0.4, 3.0 * x[1] - 2.1};
            };
            const match::OptimizerResult result =
                match::minimizeLevenbergMarquardt(residual, tolerance(kTolerance), {0.5, 0.5}, {0.0, 0.0}, {1.0, 1.0}, match::Budget{});
            expect(result.withinTolerance, match::getStopReasonName(result.reason));
            expectWithinAbsoluteError(result.values[0], 0.3, 1.0e-5);
            expectWithinAbsoluteError(result.values[1], 0.7, 1.0e-5);
            expect(result.iterations <= 5, "iterations: " + juce::String(result.iterations));
        }

        beginTest("Rosenbrock (a curved valley) inside the box: found within the budget");
        {
            // the valley 10 (y - x^2), 1 - x with x, y mapped from 0 ... 1 to -2 ... 2: the minimum at x = y = 1, i.e. 0.75 / 0.75
            const auto residual = [](const std::vector<double>& v) {
                const double x = 4.0 * v[0] - 2.0;
                const double y = 4.0 * v[1] - 2.0;
                return std::vector<double>{10.0 * (y - x * x), 1.0 - x};
            };
            const match::OptimizerResult result =
                match::minimizeLevenbergMarquardt(residual, tolerance(1.0e-7), {0.2, 0.9}, {0.0, 0.0}, {1.0, 1.0}, match::Budget{500, 15.0});
            expect(result.withinTolerance, match::getStopReasonName(result.reason) + juce::String(", rms ") + juce::String(result.rms));
            expectWithinAbsoluteError(result.values[0], 0.75, 1.0e-4);
            expectWithinAbsoluteError(result.values[1], 0.75, 1.0e-4);
            logMessage("Rosenbrock: " + juce::String(result.iterations) + " iterations, " + juce::String(result.evaluations) + " evaluations");
        }

        beginTest("a minimum outside the range: the parameter stops at its bound and is reported there");
        {
            const auto residual = [](const std::vector<double>& x) { return std::vector<double>{x[0] - 1.4, x[1] - 0.5}; };
            const match::OptimizerResult result =
                match::minimizeLevenbergMarquardt(residual, tolerance(kTolerance), {0.5, 0.2}, {0.0, 0.0}, {1.0, 1.0}, match::Budget{});
            expect(! result.withinTolerance);
            expectWithinAbsoluteError(result.values[0], 1.0, 1.0e-9);
            expectWithinAbsoluteError(result.values[1], 0.5, 1.0e-5);
            expect(result.atBound[0] && ! result.atBound[1]);
            expectWithinAbsoluteError(result.largest, 0.4, 1.0e-5);
        }

        beginTest("the budget: evaluations and time");
        {
            const auto residual = [](const std::vector<double>& v) {
                const double x = 4.0 * v[0] - 2.0;
                const double y = 4.0 * v[1] - 2.0;
                return std::vector<double>{100.0 * (y - x * x), 1.0 - x};
            };
            const match::OptimizerResult few =
                match::minimizeLevenbergMarquardt(residual, tolerance(1.0e-12), {0.0, 1.0}, {0.0, 0.0}, {1.0, 1.0}, match::Budget{10, 15.0});
            expect(few.reason == match::StopReason::EvaluationBudget, match::getStopReasonName(few.reason));
            expect(few.evaluations <= 11, "evaluations: " + juce::String(few.evaluations));
            const auto slow = [&residual](const std::vector<double>& v) {
                juce::Thread::sleep(20);
                return residual(v);
            };
            const match::OptimizerResult timed =
                match::minimizeLevenbergMarquardt(slow, tolerance(1.0e-12), {0.0, 1.0}, {0.0, 0.0}, {1.0, 1.0}, match::Budget{100000, 0.3});
            expect(timed.reason == match::StopReason::TimeBudget, match::getStopReasonName(timed.reason));
            expect(timed.seconds < 0.6, "seconds: " + juce::String(timed.seconds));
        }

        beginTest("the parameter space: locked values kept, released ones clamped to their (narrowed) ranges");
        {
            match::ParameterSpace space;
            space.base = {0.1f, 0.2f, 0.3f, 0.4f};
            space.released = {{1, "b", 0.0, 1.0, 0.5}, {3, "d", 0.25, 0.5, 0.9}};
            const std::vector<double> start = space.getStart();
            expectWithinAbsoluteError(start[1], 0.5, 1.0e-12, "a start outside the range is clamped");
            const std::vector<float> setting = space.makeSetting({0.7, 0.1});
            expectWithinAbsoluteError(setting[0], 0.1f, 1.0e-6f);
            expectWithinAbsoluteError(setting[1], 0.7f, 1.0e-6f);
            expectWithinAbsoluteError(setting[2], 0.3f, 1.0e-6f);
            expectWithinAbsoluteError(setting[3], 0.25f, 1.0e-6f);
        }

        beginTest("the match problem: a device per setting, the probe, the objective; a setting is measured once");
        {
            // the target's "device" outputs its first three parameters; the master's features are known: the match is that setting
            const auto target = [](const std::vector<float>& setting) {
                return pluginlab::measure::Device([setting](const juce::AudioBuffer<float>&, double) {
                    juce::AudioBuffer<float> output(1, 3);
                    for (int index = 0; index < 3; ++index)
                    {
                        output.setSample(0, index, setting[static_cast<size_t>(index)]);
                    }
                    return output;
                });
            };
            match::ParameterSpace space;
            space.base = {0.5f, 0.5f, 0.5f};
            space.released = {{0, "a", 0.0, 1.0, 0.5}, {2, "c", 0.0, 1.0, 0.5}};
            match::MatchProblem problem({0.2, 0.5, 0.8}, target, std::make_shared<SettingProbe>(),
                                        std::make_shared<match::WeightedDifference>(std::vector<double>{1.0, 1.0, 1.0}, 1.0e-5), space);
            const match::OptimizerResult result = problem.solve(match::Budget{});
            expect(result.withinTolerance, match::getStopReasonName(result.reason));
            expectWithinAbsoluteError(result.values[0], 0.2, 1.0e-4);
            expectWithinAbsoluteError(result.values[1], 0.8, 1.0e-4);
            const int measured = problem.getMeasurements();
            problem.getFeatures(space.makeSetting(result.values));
            expectEquals(problem.getMeasurements(), measured, "a setting already measured comes from the cache");
            expect(result.evaluations <= measured);
        }
    }
};

static MatchTests matchTests;
