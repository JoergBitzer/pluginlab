#include <cmath>
#include <complex>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "pluginlab/eqmodel/BandFit.h"
#include "pluginlab/eqmodel/EqModel.h"
#include "pluginlab/measure/FrequencyResponse.h"
#include "pluginlab/reference/Biquad.h"
#include "pluginlab/reference/Designs.h"

namespace
{
namespace ref = pluginlab::reference;
namespace eq = pluginlab::eqmodel;

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

// A comb filter (not an EQ): y = x + 0.5 x(n - 48)
std::complex<double> getCombResponse(double frequencyHz, double sampleRate)
{
    return 1.0 + 0.5 * std::polar(1.0, -2.0 * juce::MathConstants<double>::pi * frequencyHz * 48.0 / sampleRate);
}
}

// W8.3: band identification (docs/eqmodel/band-identification.md)
class BandFitTests : public juce::UnitTest
{
public:
    BandFitTests()
        : juce::UnitTest("EQ band identification", "pluginlab")
    {
    }

    void runTest() override
    {
        testKnownModels();
        testMeasured();
        testNotAnEq();
        logMessage("results for docs/eqmodel/band-identification.md:\n" + m_lines.joinIntoString("\n"));
    }

private:
    struct Case
    {
        juce::String name;
        double sampleRate;
        std::vector<eq::Band> bands;
        double outputGainDb = 0.0;
    };

    void testKnownModels()
    {
        beginTest("known models with 1 ... 4 bands at 44.1 / 48 / 96 kHz are recovered (frequency 0.1 %, gain 0.01 dB, Q 0.5 %, family, type)");
        using F = ref::FilterType;
        const eq::DesignFamily rbj = eq::DesignFamily::Rbj;
        const std::vector<Case> cases = {
            {"RBJ peak 1 kHz +6 dB Q 2", 48000.0, {makeBand(rbj, F::Peak, 1000.0, 6.0, 2.0)}},
            {"RBJ peak 3 kHz -8 dB Q 4", 44100.0, {makeBand(rbj, F::Peak, 3000.0, -8.0, 4.0)}},
            {"RBJ low shelf 150 Hz +5 dB", 96000.0, {makeBand(rbj, F::LowShelf, 150.0, 5.0, 0.71)}},
            {"RBJ high shelf 8 kHz -4 dB", 48000.0, {makeBand(rbj, F::HighShelf, 8000.0, -4.0, 0.71)}},
            // (order 2 is the RBJ high-pass with Q 1/sqrt(2): the same filter; order 3 is distinguishable)
            {"Butterworth high-pass order 3, 60 Hz", 48000.0, {makeBand(eq::DesignFamily::Butterworth, F::HighPass, 60.0, 0.0, 0.71, 3)}},
            {"four bands (shelf, two peaks, shelf), output -1 dB", 48000.0,
             {makeBand(rbj, F::LowShelf, 100.0, 4.0, 0.71), makeBand(rbj, F::Peak, 800.0, -6.0, 2.0), makeBand(rbj, F::Peak, 4000.0, 3.0, 1.0),
              makeBand(rbj, F::HighShelf, 12000.0, -3.0, 0.71)},
             -1.0},
            {"Orfanidis peak 15 kHz +6 dB Q 1", 48000.0, {makeBand(eq::DesignFamily::Orfanidis, F::Peak, 15000.0, 6.0, 1.0)}},
            {"linear-phase peak 1 kHz +6 dB Q 2", 48000.0, {makeBand(eq::DesignFamily::LinearPhase, F::Peak, 1000.0, 6.0, 2.0)}},
        };
        m_lines.add("| Model | Recovered | Residual rms (dB) | Largest error: f / gain / Q |\n|---|---|---|---|");
        for (const Case& item : cases)
        {
            eq::EqModel truth;
            truth.bands = item.bands;
            truth.outputGainDb = item.outputGainDb;
            const std::vector<double> grid = eq::makeLogGrid(20.0, 20000.0, 24.0);
            std::vector<std::complex<double>> response;
            for (const double f : grid)
            {
                response.push_back(eq::getModelResponse(truth, f, item.sampleRate));
            }
            eq::FitSettings settings;
            settings.sampleRate = item.sampleRate;
            const eq::FitResult fit = eq::fitEqModel(grid, response, settings);
            expectEquals(static_cast<int>(fit.model.bands.size()), static_cast<int>(item.bands.size()), item.name + ": number of bands\n" + fit.summary);
            double frequencyError = 0.0;
            double gainError = 0.0;
            double qError = 0.0;
            for (const eq::Band& expected : item.bands)
            {
                const eq::Band* found = findBand(fit.model, expected);
                expect(found != nullptr, item.name + ": no band of this type near " + juce::String(expected.frequencyHz) + "\n" + fit.summary);
                if (found == nullptr)
                {
                    continue;
                }
                expect(found->family == expected.family, item.name + ": family " + eq::getFamilyName(found->family));
                frequencyError = std::max(frequencyError, std::abs(found->frequencyHz / expected.frequencyHz - 1.0));
                if (expected.type == ref::FilterType::Peak || expected.type == ref::FilterType::LowShelf || expected.type == ref::FilterType::HighShelf)
                {
                    gainError = std::max(gainError, std::abs(found->gainDb - expected.gainDb));
                }
                if (expected.family == eq::DesignFamily::Rbj || expected.family == eq::DesignFamily::Orfanidis || expected.family == eq::DesignFamily::LinearPhase)
                {
                    qError = std::max(qError, std::abs(found->q / expected.q - 1.0));
                }
                if (expected.family == eq::DesignFamily::Butterworth)
                {
                    expectEquals(found->order, expected.order, item.name + ": order");
                }
            }
            expect(frequencyError < 0.001, item.name + ": frequency " + juce::String(frequencyError));
            expect(gainError < 0.01, item.name + ": gain " + juce::String(gainError));
            expect(qError < 0.005, item.name + ": Q " + juce::String(qError));
            expect(fit.goodDescription, item.name + ": good description");
            m_lines.add("| " + item.name + " (" + juce::String(item.sampleRate / 1000.0, 1) + " kHz) | " + fit.summary.replace("\n", "; ") + " | "
                        + juce::String(fit.rmsResidualDb, 5) + " | " + juce::String(100.0 * frequencyError, 4) + " % / " + juce::String(gainError, 4) + " dB / "
                        + juce::String(100.0 * qError, 4) + " % |");
        }
    }

    void testMeasured()
    {
        beginTest("a measured response (the synchronized sweep through an RBJ peak and a shelf): recovered within the measurement's accuracy");
        const double rate = 48000.0;
        const auto peak = ref::designRbj(ref::FilterType::Peak, rate, 2500.0, -5.0, 3.0);
        const auto shelf = ref::designRbj(ref::FilterType::LowShelf, rate, 200.0, 3.0, 0.71);
        const std::vector<ref::BiquadCoefficients> sections = {peak, shelf};
        pluginlab::measure::SweepResponseSettings sweep;
        sweep.channels = 1;
        sweep.frequencies = eq::makeLogGrid(20.0, 20000.0, 24.0);
        const pluginlab::measure::FrequencyResponse measured = pluginlab::measure::measureSweptResponse(
            pluginlab::measure::makeProcessorDevice([sections](double sampleRate) { return std::make_unique<ref::BiquadCascade>(sections, sampleRate); }), sweep);
        eq::FitSettings settings;
        const eq::FitResult fit = eq::fitEqModel(sweep.frequencies, measured.channels[0].response, settings);
        const eq::Band* foundPeak = findBand(fit.model, makeBand(eq::DesignFamily::Rbj, ref::FilterType::Peak, 2500.0, -5.0, 3.0));
        const eq::Band* foundShelf = findBand(fit.model, makeBand(eq::DesignFamily::Rbj, ref::FilterType::LowShelf, 200.0, 3.0, 0.71));
        expect(foundPeak != nullptr && foundShelf != nullptr, "both bands\n" + fit.summary);
        if (foundPeak != nullptr && foundShelf != nullptr)
        {
            expectWithinAbsoluteError(foundPeak->frequencyHz, 2500.0, 2.5, "peak frequency");
            expectWithinAbsoluteError(foundPeak->gainDb, -5.0, 0.02, "peak gain");
            expectWithinAbsoluteError(foundShelf->gainDb, 3.0, 0.02, "shelf gain");
        }
        m_lines.add("\nmeasured (sweep) RBJ peak 2.5 kHz -5 dB Q 3 + low shelf 200 Hz +3 dB: " + fit.summary.replace("\n", "; "));
    }

    void testNotAnEq()
    {
        beginTest("a comb filter (not an EQ) is reported as no good description instead of invented bands");
        const double rate = 48000.0;
        const std::vector<double> grid = eq::makeLogGrid(20.0, 20000.0, 24.0);
        std::vector<std::complex<double>> response;
        for (const double f : grid)
        {
            response.push_back(getCombResponse(f, rate));
        }
        eq::FitSettings settings;
        settings.latencySamples = 0;
        const eq::FitResult fit = eq::fitEqModel(grid, response, settings);
        expect(! fit.goodDescription, "comb filter: residual " + juce::String(fit.rmsResidualDb));
        m_lines.add("comb filter y = x + 0.5 x(n - 48): " + juce::String(static_cast<int>(fit.model.bands.size())) + " bands tried, residual "
                    + juce::String(fit.rmsResidualDb, 2) + " dB rms: no good description");
    }

    // the band of the same type nearest in frequency (within half an octave)
    static const eq::Band* findBand(const eq::EqModel& model, const eq::Band& expected)
    {
        const eq::Band* best = nullptr;
        for (const eq::Band& band : model.bands)
        {
            if (band.type != expected.type)
            {
                continue;
            }
            const double distance = std::abs(std::log2(band.frequencyHz / expected.frequencyHz));
            if (distance < 0.5 && (best == nullptr || distance < std::abs(std::log2(best->frequencyHz / expected.frequencyHz))))
            {
                best = &band;
            }
        }
        return best;
    }

    juce::StringArray m_lines;
};

static BandFitTests bandFitTests;
