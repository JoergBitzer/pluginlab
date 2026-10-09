#include <cmath>
#include <complex>
#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "MeasureTestDevices.h"
#include "pluginlab/measure/NullTest.h"
#include "pluginlab/reference/Delays.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kPi = 3.14159265358979323846;
constexpr double kSampleRate = 48000.0;

using Factory = std::function<std::unique_ptr<ref::Processor>(double)>;
using Response = std::function<std::complex<double>(double)>;

measure::Device makeDevice(Factory factory)
{
    return measure::makeProcessorDevice(std::move(factory));
}

Factory makeChain(std::function<void(pluginlab::test::Chain&, double)> build)
{
    return [build](double rate)
    {
        auto chain = std::make_unique<pluginlab::test::Chain>();
        build(*chain, rate);
        return chain;
    };
}

// The null depth for white noise through two linear devices, with B aligned by the measured gain and delay: sum |H_A - g e^{-j w d} H_B|^2 / sum |H_A|^2
// over the analysis bins of 20 Hz ... 20 kHz (the same frequencies the unit sums)
double getExpectedNullDb(const Response& a, const Response& b, double gain, double delay, int length)
{
    const double binWidth = kSampleRate / length;
    double residual = 0.0;
    double power = 0.0;
    for (int bin = static_cast<int>(std::ceil(20.0 / binWidth)); bin <= static_cast<int>(std::floor(20000.0 / binWidth)); ++bin)
    {
        const double f = bin * binWidth;
        const std::complex<double> responseA = a(f);
        residual += std::norm(responseA - gain * std::polar(1.0, -2.0 * kPi * f * delay / kSampleRate) * b(f));
        power += std::norm(responseA);
    }
    return 10.0 * std::log10(residual / power);
}

// The best null an alignment can reach: for every delay on a grid around the found one, the least-squares gain in closed form
// (g = Re sum H_A conj(H_B e^{-jwd}) / sum |H_B|^2), the smallest null of all; returns {null dB, delay}
std::pair<double, double> getBestNullDb(const Response& a, const Response& b, double around, int length)
{
    const double binWidth = kSampleRate / length;
    std::vector<double> frequencies;
    for (int bin = static_cast<int>(std::ceil(20.0 / binWidth)); bin <= static_cast<int>(std::floor(20000.0 / binWidth)); bin += 7)
    {
        frequencies.push_back(bin * binWidth);
    }
    std::pair<double, double> best = {1000.0, around};
    for (double delay = around - 0.5; delay <= around + 0.5; delay += 0.001)
    {
        double cross = 0.0;
        double powerB = 0.0;
        double powerA = 0.0;
        for (const double f : frequencies)
        {
            const std::complex<double> shifted = std::polar(1.0, -2.0 * kPi * f * delay / kSampleRate) * b(f);
            cross += (a(f) * std::conj(shifted)).real();
            powerB += std::norm(shifted);
            powerA += std::norm(a(f));
        }
        const double gain = cross / powerB;
        double residual = 0.0;
        for (const double f : frequencies)
        {
            residual += std::norm(a(f) - gain * std::polar(1.0, -2.0 * kPi * f * delay / kSampleRate) * b(f));
        }
        const double null = 10.0 * std::log10(residual / powerA);
        if (null < best.first)
        {
            best = {null, delay};
        }
    }
    return best;
}
}

// W7.10: null test with alignment (docs/measurements/null-test.md)
class MeasureNullTests : public juce::UnitTest
{
public:
    MeasureNullTests()
        : juce::UnitTest("Measure: null test", "pluginlab")
    {
    }

    void runTest() override
    {
        testExactNulls();
        testKnownDifferences();
        testNoise();
        logMessage("results for docs/measurements/null-test.md:\n" + m_lines.joinIntoString("\n"));
    }

private:
    void testExactNulls()
    {
        beginTest("the same device twice, a delayed and attenuated copy, an inverted copy: delay and gain found exactly, the null at the float floor");
        struct Case
        {
            juce::String name;
            Factory a;
            Factory b;
            double delay;
            double gainDb;
            bool inverted;
        };
        const std::vector<ref::BiquadCoefficients> peak = {ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 2.0)};
        const std::vector<Case> cases = {
            {"RBJ peak against itself", [peak](double rate) { return std::make_unique<ref::BiquadCascade>(peak, rate); },
             [peak](double rate) { return std::make_unique<ref::BiquadCascade>(peak, rate); }, 0.0, 0.0, false},
            {"A = B delayed by 37 samples and -0.5 dB", makeChain([](pluginlab::test::Chain& chain, double rate)
                                                                  {
                                                                      chain.add(std::make_unique<ref::Gain>(-0.5, false));
                                                                      chain.add(std::make_unique<ref::DirectFormFilter>(ref::makeIntegerDelay(37, rate)));
                                                                  }),
             [](double) { return std::make_unique<ref::Gain>(0.0, false); }, 37.0, -0.5, false},
            {"A = B 1000 samples earlier (B delayed)", [](double) { return std::make_unique<ref::Gain>(0.0, false); },
             [](double rate) { return std::make_unique<ref::DirectFormFilter>(ref::makeIntegerDelay(1000, rate)); }, -1000.0, 0.0, false},
            {"A = B inverted, +3 dB", [](double) { return std::make_unique<ref::Gain>(3.0, true); }, [](double) { return std::make_unique<ref::Gain>(0.0, false); }, 0.0,
             3.0, true},
        };
        m_lines.add("| A | expected delay / gain | measured delay | measured gain | inverted | null, aligned | null, unaligned |\n|---|---|---|---|---|---|---|");
        for (const Case& item : cases)
        {
            measure::NullTestSettings settings;
            settings.channels = 1;
            const measure::NullTestResult result = measure::measureNull(makeDevice(item.a), makeDevice(item.b), settings);
            const measure::ChannelNull& measured = result.channels[0];
            expectWithinAbsoluteError(measured.delaySamples, item.delay, 1.0e-3, item.name + ": delay");
            expectWithinAbsoluteError(measured.gainDb, item.gainDb, 1.0e-4, item.name + ": gain");
            expect(measured.inverted == item.inverted, item.name + ": polarity");
            expect(measured.nullDepthDb < -120.0, item.name + ": null " + juce::String(measured.nullDepthDb));
            m_lines.add("| " + item.name + " | " + juce::String(item.delay, 0) + " / " + juce::String(item.gainDb, 1) + " dB | " + juce::String(measured.delaySamples, 4) + " | "
                        + juce::String(measured.gainDb, 4) + " dB | " + yesNo(measured.inverted) + " | " + juce::String(measured.nullDepthDb, 1) + " dB | "
                        + juce::String(measured.unalignedNullDb, 1) + " dB |");
        }
    }

    void testKnownDifferences()
    {
        beginTest("devices that differ by a known amount: the null equals sum |H_A - g e^(-jwd) H_B|^2 / sum |H_A|^2 with the found g and d");
        struct Case
        {
            juce::String name;
            Factory a;
            Factory b;
            Response responseA;
            Response responseB;
        };
        const auto peakAt = [](double gain) { return std::vector<ref::BiquadCoefficients>{ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, gain, 2.0)}; };
        const std::vector<ref::BiquadCoefficients> peak3 = peakAt(3.0);
        const std::vector<ref::BiquadCoefficients> peak35 = peakAt(3.5);
        const std::vector<ref::BiquadCoefficients> rbjShelf = {ref::designRbj(ref::FilterType::HighShelf, kSampleRate, 5000.0, 6.0, 0.7071)};
        const ref::DirectFormFilter thiran = ref::makeThiranDelay(37.25, 3, kSampleRate);
        const ref::DirectFormFilter lagrange = ref::makeLagrangeDelay(37.25, 3, kSampleRate);
        const Factory identity = [](double) { return std::make_unique<ref::Gain>(0.0, false); };
        const Response flat = [](double) { return std::complex<double>(1.0, 0.0); };
        const std::vector<Case> cases = {
            {"RBJ peak 1 kHz Q 2: +3 dB against +3.5 dB", [peak3](double rate) { return std::make_unique<ref::BiquadCascade>(peak3, rate); },
             [peak35](double rate) { return std::make_unique<ref::BiquadCascade>(peak35, rate); },
             [peak3](double f) { return ref::getCascadeResponse(peak3, f, kSampleRate); }, [peak35](double f) { return ref::getCascadeResponse(peak35, f, kSampleRate); }},
            {"RBJ high shelf 5 kHz +6 dB against nothing", [rbjShelf](double rate) { return std::make_unique<ref::BiquadCascade>(rbjShelf, rate); }, identity,
             [rbjShelf](double f) { return ref::getCascadeResponse(rbjShelf, f, kSampleRate); }, flat},
            {"Thiran 37.25 samples (order 3) against nothing", [](double rate) { return std::make_unique<ref::DirectFormFilter>(ref::makeThiranDelay(37.25, 3, rate)); },
             identity, [thiran](double f) { return thiran.getResponse(f); }, flat},
            {"Lagrange 37.25 samples (order 3) against nothing", [](double rate) { return std::make_unique<ref::DirectFormFilter>(ref::makeLagrangeDelay(37.25, 3, rate)); },
             identity, [lagrange](double f) { return lagrange.getResponse(f); }, flat},
        };
        m_lines.add("\n| A against B | measured delay | measured gain | expected null (exact responses, found g and d) | measured null | best possible null (searched) |\n|---|---|---|---|---|---|");
        for (const Case& item : cases)
        {
            measure::NullTestSettings settings;
            settings.channels = 1;
            const measure::NullTestResult result = measure::measureNull(makeDevice(item.a), makeDevice(item.b), settings);
            const measure::ChannelNull& measured = result.channels[0];
            double gain = std::pow(10.0, measured.gainDb / 20.0);
            if (measured.inverted)
            {
                gain = -gain;
            }
            const double expected = getExpectedNullDb(item.responseA, item.responseB, gain, measured.delaySamples, settings.windowSamples);
            expectWithinAbsoluteError(measured.nullDepthDb, expected, 0.5, item.name + ": null");
            // the alignment is close to the best possible one (the delay that aligns a whole band best is not the low-frequency phase delay
            // when the group delay varies, as for Thiran and Lagrange)
            const std::pair<double, double> best = getBestNullDb(item.responseA, item.responseB, measured.delaySamples, settings.windowSamples);
            expect(measured.nullDepthDb < best.first + 0.3, item.name + ": null " + juce::String(measured.nullDepthDb) + " against the best " + juce::String(best.first));
            m_lines.add("| " + item.name + " | " + juce::String(measured.delaySamples, 4) + " | " + juce::String(measured.gainDb, 4) + " dB | " + juce::String(expected, 2) + " dB | "
                        + juce::String(measured.nullDepthDb, 2) + " dB | " + juce::String(best.first, 2) + " dB at " + juce::String(best.second, 3) + " |");
        }
    }

    void testNoise()
    {
        beginTest("two dithered 16-bit quantizers with independent dither: the null is the sum of both noises, 10 lg(2 (q^2/4) / sigma^2)");
        measure::NullTestSettings settings;
        settings.channels = 1;
        const measure::NullTestResult result = measure::measureNull(makeDevice([](double) { return std::make_unique<ref::Quantizer>(16, true, 1); }),
                                                                    makeDevice([](double) { return std::make_unique<ref::Quantizer>(16, true, 2); }), settings);
        const double step = std::pow(2.0, -15.0);
        const double sigma = measure::dbfsToRms(settings.levelDbfs);
        const double expected = 10.0 * std::log10(2.0 * step * step / 4.0 / (sigma * sigma));
        expectWithinAbsoluteError(result.channels[0].nullDepthDb, expected, 0.2, "dithered quantizers");
        expectWithinAbsoluteError(result.channels[0].delaySamples, 0.0, 1.0e-3, "dithered quantizers: delay");
        double spread = 0.0;
        for (const double band : result.channels[0].thirdOctaveResidualDb)
        {
            spread = std::max(spread, std::abs(band - expected));
        }
        m_lines.add("\ntwo 16-bit quantizers with independent TPDF dither, white noise -20 dBFS: null " + juce::String(result.channels[0].nullDepthDb, 2) + " dB (expected "
                    + juce::String(expected, 2) + " dB), residual " + juce::String(result.channels[0].residualDbfs, 2) + " dBFS; third-octave bands within "
                    + juce::String(spread, 1) + " dB of the broadband value");
    }

    static juce::String yesNo(bool value)
    {
        if (value)
        {
            return "yes";
        }
        return "no";
    }

    juce::StringArray m_lines;
};

static MeasureNullTests measureNullTests;
