#include <functional>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

#include "MeasureTestDevices.h"
#include "pluginlab/measure/SystemClass.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/reference/Nonlinear.h"
#include "pluginlab/reference/Utility.h"

namespace
{
namespace ref = pluginlab::reference;
namespace measure = pluginlab::measure;

constexpr double kSampleRate = 48000.0;
const std::vector<double> kPolynomial = {0.0, 1.0, 0.1, 0.05};
}

// W8.1: the system classification against devices of known class (docs/measurements/system-class.md)
class MeasureSystemClassTests : public juce::UnitTest
{
public:
    MeasureSystemClassTests()
        : juce::UnitTest("Measure: system class", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("devices of known class are classified correctly (LTI, time-variant, memoryless, Hammerstein, Wiener, generalized Hammerstein, other)");
        struct Case
        {
            juce::String name;
            std::function<std::unique_ptr<ref::Processor>(double)> factory;
            measure::SystemClass expected;
        };
        const auto lowPass = ref::designButterworth(ref::Pass::Low, 2, kSampleRate, 1500.0);
        const auto highPass = ref::designButterworth(ref::Pass::High, 2, kSampleRate, 1000.0);
        const auto peak = std::vector<ref::BiquadCoefficients>{ref::designRbj(ref::FilterType::Peak, kSampleRate, 1000.0, 6.0, 2.0)};
        const std::vector<Case> cases = {
            {"gain -6 dB", [](double) { return std::make_unique<ref::Gain>(-6.0, false); }, measure::SystemClass::Lti},
            {"RBJ peak 1 kHz +6 dB", [peak](double rate) { return std::make_unique<ref::BiquadCascade>(peak, rate); }, measure::SystemClass::Lti},
            {"tremolo 3 Hz, depth 0.3", [](double rate) { return std::make_unique<ref::Tremolo>(rate, 3.0, 0.3); }, measure::SystemClass::TimeVariant},
            {"polynomial x + 0.1x^2 + 0.05x^3", [](double) { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makePolynomial(kPolynomial)); },
             measure::SystemClass::Memoryless},
            {"tanh(2x)", [](double) { return std::make_unique<ref::Waveshaper>(ref::Waveshaper::makeSoftClip(2.0)); }, measure::SystemClass::Memoryless},
            {"polynomial, then low-pass 1.5 kHz (Hammerstein)",
             [lowPass](double rate) { return std::make_unique<pluginlab::test::Hammerstein>(kPolynomial, lowPass, rate); }, measure::SystemClass::Hammerstein},
            {"low-pass 1.5 kHz, then polynomial (Wiener)",
             [lowPass](double rate) { return std::make_unique<pluginlab::test::Wiener>(lowPass, kPolynomial, rate); }, measure::SystemClass::Wiener},
            {"x + LP(0.1x^2) + HP(0.05x^3) (generalized Hammerstein)",
             [lowPass, highPass](double rate) { return std::make_unique<pluginlab::test::GeneralizedHammerstein>(0.1, lowPass, 0.05, highPass, rate); },
             measure::SystemClass::GeneralizedHammerstein},
            {"compressor -30 dBFS 4:1 (dynamics)", [](double rate) { return std::make_unique<pluginlab::test::Compressor>(-30.0, 4.0, rate); },
             measure::SystemClass::Other},
        };
        juce::StringArray lines;
        lines.add("| Device | Expected | Classified | Reason | Time var. | Largest H_n | H_1 level spread | H_1 flat | H_n flat | Hammerstein | Wiener | Shape |");
        lines.add("|---|---|---|---|---|---|---|---|---|---|---|---|");
        for (const Case& item : cases)
        {
            const measure::SystemClassResult result = measure::classifySystem(measure::makeProcessorDevice(item.factory), measure::SystemClassSettings());
            const measure::SystemClassEvidence& e = result.evidence;
            expect(result.systemClass == item.expected, item.name + ": " + measure::getSystemClassName(result.systemClass) + " (" + result.reason + ")");
            lines.add("| " + item.name + " | " + measure::getSystemClassName(item.expected) + " | " + measure::getSystemClassName(result.systemClass) + " | "
                      + result.reason + " | " + juce::String(e.timeVarianceDb, 3) + " | " + juce::String(e.largestHarmonicDb, 1) + " | "
                      + juce::String(e.linearLevelSpreadDb, 3) + " | " + juce::String(e.linearFlatnessDb, 3) + " | " + juce::String(e.harmonicFlatnessDb, 3) + " | "
                      + juce::String(e.hammersteinSpreadDb, 3) + " | " + juce::String(e.wienerSpreadDb, 3) + " | " + juce::String(e.shapeSpreadDb, 3) + " |");
        }
        logMessage("results for docs/measurements/system-class.md:\n" + lines.joinIntoString("\n"));
    }
};

static MeasureSystemClassTests measureSystemClassTests;
