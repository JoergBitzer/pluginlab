#include <cmath>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/eqmodel/EqModel.h"
#include "pluginlab/match/EqDomain.h"
#include "pluginlab/match/Match.h"
#include "pluginlab/reference/Biquad.h"

namespace
{
namespace match = pluginlab::match;
namespace eq = pluginlab::eqmodel;
namespace ref = pluginlab::reference;

constexpr double kSampleRate = 48000.0;
constexpr double kExactDb = 0.01;            // twins: the match within 0.01 dB
constexpr double kFrequencyTolerance = 1.0e-3;
constexpr double kGainToleranceDb = 0.01;
constexpr double kQTolerance = 5.0e-3;
// the budget of the tests: Debug renders are slow (the author's 15 s per target is for the Release build; the fast render path is W9.3)
const match::Budget kTestBudget{400, 120.0};

// the knob laws of a typical target EQ: frequency log 20 Hz ... 20 kHz, gain -24 ... 24 dB, Q 0.1 ... 10 (linear)
double toFrequency(double v) { return 20.0 * std::pow(1000.0, v); }
double toGain(double v) { return -24.0 + 48.0 * v; }
double toQ(double v) { return 0.1 + 9.9 * v; }
double fromFrequency(double hz) { return std::log(hz / 20.0) / std::log(1000.0); }
double fromGain(double db) { return (db + 24.0) / 48.0; }
double fromQ(double q) { return (q - 0.1) / 9.9; }

eq::Band makeBand(eq::DesignFamily family, ref::FilterType type, double frequency, double gain, double q, int order = 2)
{
    eq::Band band;
    band.family = family;
    band.type = type;
    band.frequencyHz = frequency;
    band.gainDb = gain;
    band.q = q;
    band.order = order;
    return band;
}

// a target with bands of fixed family and type (as the user set its switches); per band three parameters: frequency, gain, Q
match::MatchProblem::DeviceFactory makeTarget(std::vector<eq::Band> bands)
{
    return [bands](const std::vector<float>& setting) {
        std::vector<eq::Band> set = bands;
        for (size_t band = 0; band < set.size(); ++band)
        {
            set[band].frequencyHz = toFrequency(setting[3 * band]);
            set[band].gainDb = toGain(setting[3 * band + 1]);
            set[band].q = toQ(setting[3 * band + 2]);
        }
        return pluginlab::measure::makeProcessorDevice([set](double rate) {
            std::vector<ref::BiquadCoefficients> sections;
            for (const eq::Band& band : set)
            {
                const eq::BandDesign design = eq::designBand(band, rate);
                sections.insert(sections.end(), design.sections.begin(), design.sections.end());
            }
            return std::make_unique<ref::BiquadCascade>(sections, rate);
        });
    };
}

match::ParameterSpace makeSpace(const std::vector<eq::Band>& start, bool releaseGainAndQ = true)
{
    match::ParameterSpace space;
    for (size_t band = 0; band < start.size(); ++band)
    {
        const juce::String number(static_cast<int>(band) + 1);
        space.base.push_back(static_cast<float>(fromFrequency(start[band].frequencyHz)));
        space.base.push_back(static_cast<float>(fromGain(start[band].gainDb)));
        space.base.push_back(static_cast<float>(fromQ(start[band].q)));
        space.released.push_back({static_cast<int>(3 * band), "Frequency " + number, 0.0, 1.0, fromFrequency(start[band].frequencyHz)});
        if (releaseGainAndQ)
        {
            space.released.push_back({static_cast<int>(3 * band + 1), "Gain " + number, 0.0, 1.0, fromGain(start[band].gainDb)});
            space.released.push_back({static_cast<int>(3 * band + 2), "Q " + number, 0.0, 1.0, fromQ(start[band].q)});
        }
    }
    return space;
}
}

// W9.2: the EQ domain of the matcher (docs/matching/eq-domain.md) on reference processors: no plugin. Twins are matched exactly from the
// user's start; a different design family leaves its known residual.
class MatchEqTests : public juce::UnitTest
{
public:
    MatchEqTests()
        : juce::UnitTest("Matcher, EQ domain (W9.2)", "pluginlab")
    {
    }

    void runTest() override
    {
        match::MagnitudeProbeSettings probeSettings;
        probeSettings.sampleRate = kSampleRate;
        const auto probe = std::make_shared<match::MagnitudeProbe>(probeSettings);
        const std::vector<double>& grid = probe->getFrequencies();

        beginTest("the probe: a measured band equals its band-model curve");
        {
            const eq::Band band = makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 1000.0, 6.0, 2.0);
            const std::vector<double> curve = match::getCurveMagnitude(eq::EqModel{{band}, 0.0}, grid, kSampleRate);
            const std::vector<float> setting = {static_cast<float>(fromFrequency(1000.0)), static_cast<float>(fromGain(6.0)), static_cast<float>(fromQ(2.0))};
            const std::vector<double> measured = probe->measure(makeTarget({band})(setting));
            double worst = 0.0;
            for (size_t index = 0; index < grid.size(); ++index)
            {
                worst = std::max(worst, std::abs(measured[index] - curve[index]));
            }
            expectLessThan(worst, 0.005, "probe against curve");
        }

        struct Twin
        {
            juce::String name;
            eq::Band master;
            eq::Band start;
            bool releaseGainAndQ;
        };
        const std::vector<Twin> twins = {
            {"RBJ peak 1 kHz +6 dB Q 2 from 600 Hz +3 dB Q 1.2", makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 1000.0, 6.0, 2.0),
             makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 600.0, 3.0, 1.2), true},
            {"RBJ low shelf 200 Hz +4 dB Q 0.71 from 500 Hz +1 dB Q 1", makeBand(eq::DesignFamily::Rbj, ref::FilterType::LowShelf, 200.0, 4.0, 0.71),
             makeBand(eq::DesignFamily::Rbj, ref::FilterType::LowShelf, 500.0, 1.0, 1.0), true},
            {"Butterworth low-pass order 4 at 2 kHz from 800 Hz (frequency only)", makeBand(eq::DesignFamily::Butterworth, ref::FilterType::LowPass, 2000.0, 0.0, 0.71, 4),
             makeBand(eq::DesignFamily::Butterworth, ref::FilterType::LowPass, 800.0, 0.0, 0.71, 4), false},
        };
        for (const Twin& twin : twins)
        {
            beginTest("twins: " + twin.name);
            const std::vector<double> master = match::getCurveMagnitude(eq::EqModel{{twin.master}, 0.0}, grid, kSampleRate);
            match::MatchProblem problem(master, makeTarget({twin.start}), probe, match::makeMagnitudeObjective(grid.size(), kExactDb),
                                        makeSpace({twin.start}, twin.releaseGainAndQ));
            const match::OptimizerResult result = problem.solve(kTestBudget);
            expect(result.withinTolerance, juce::String(match::getStopReasonName(result.reason)) + ", largest " + juce::String(result.largest, 4) + " dB");
            const double frequency = toFrequency(result.values[0]);
            expectWithinAbsoluteError(frequency / twin.master.frequencyHz, 1.0, kFrequencyTolerance, "frequency " + juce::String(frequency));
            if (twin.releaseGainAndQ)
            {
                expectWithinAbsoluteError(toGain(result.values[1]), twin.master.gainDb, kGainToleranceDb, "gain");
                expectWithinAbsoluteError(toQ(result.values[2]) / twin.master.q, 1.0, kQTolerance, "Q " + juce::String(toQ(result.values[2])));
            }
            logMessage(twin.name + ": " + juce::String(result.evaluations) + " renders, " + juce::String(result.iterations) + " iterations, "
                       + juce::String(result.seconds, 2) + " s, largest " + juce::String(result.largest, 5) + " dB");
        }

        beginTest("Orfanidis master, RBJ target: the known residual near Nyquist (the teaching case)");
        {
            const eq::Band master = makeBand(eq::DesignFamily::Orfanidis, ref::FilterType::Peak, 10000.0, 6.0, 1.0);
            const eq::Band start = makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 10000.0, 6.0, 1.0);
            const std::vector<double> masterCurve = match::getCurveMagnitude(eq::EqModel{{master}, 0.0}, grid, kSampleRate);
            match::MatchProblem problem(masterCurve, makeTarget({start}), probe, match::makeMagnitudeObjective(grid.size(), 0.1), makeSpace({start}));
            const match::OptimizerResult result = problem.solve(kTestBudget);
            size_t worst = 0;
            for (size_t index = 0; index < result.residual.size(); ++index)
            {
                if (std::abs(result.residual[index]) > std::abs(result.residual[worst]))
                {
                    worst = index;
                }
            }
            logMessage("Orfanidis 10 kHz +6 dB Q 1 against RBJ: best RBJ " + juce::String(toFrequency(result.values[0]), 0) + " Hz, "
                       + juce::String(toGain(result.values[1]), 2) + " dB, Q " + juce::String(toQ(result.values[2]), 3) + "; largest deviation "
                       + juce::String(result.residual[worst], 3) + " dB at " + juce::String(grid[worst], 0) + " Hz, rms " + juce::String(result.rms, 3) + " dB");
            expect(! result.withinTolerance, "an RBJ peak cannot be an Orfanidis peak near Nyquist");
            expectGreaterThan(grid[worst], 12000.0, "the residual sits near Nyquist");
        }

        beginTest("two bands: a band-model curve (two peaks) matched by a target with two RBJ peaks (six parameters)");
        {
            const eq::EqModel master{{makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 300.0, 5.0, 1.0),
                                      makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 3000.0, -4.0, 2.0)},
                                     0.0};
            const std::vector<eq::Band> start = {makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 200.0, 2.0, 0.7),
                                                 makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 4000.0, -1.0, 1.0)};
            match::MatchProblem problem(match::getCurveMagnitude(master, grid, kSampleRate), makeTarget(start), probe,
                                        match::makeMagnitudeObjective(grid.size(), kExactDb), makeSpace(start));
            const match::OptimizerResult result = problem.solve(kTestBudget);
            expect(result.withinTolerance, juce::String(match::getStopReasonName(result.reason)) + ", largest " + juce::String(result.largest, 4) + " dB");
            expectWithinAbsoluteError(toFrequency(result.values[0]) / 300.0, 1.0, kFrequencyTolerance, "frequency 1");
            expectWithinAbsoluteError(toFrequency(result.values[3]) / 3000.0, 1.0, kFrequencyTolerance, "frequency 2");
            logMessage("two bands: " + juce::String(result.evaluations) + " renders, " + juce::String(result.iterations) + " iterations, "
                       + juce::String(result.seconds, 2) + " s");
        }
    }
};

static MatchEqTests matchEqTests;
