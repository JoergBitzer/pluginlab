#include <cmath>
#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/Interactions.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/ParameterSurvey.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
namespace engine = pluginlab::engine;

// two RBJ peaks of +6 dB (A = 10^(6/20)) at one frequency, in parallel: 1 + 2 (A - 1) instead of A^2 at the centre; +6 and -6 dB:
// 1 + (A - 1) + (1/A - 1) instead of 1
const double kGain = std::pow(10.0, 6.0 / 20.0);
const double kParallelEqualDb = 20.0 * std::log10(1.0 + 2.0 * (kGain - 1.0)) - 12.0;               // -2.49 dB
const double kParallelOppositeDb = 20.0 * std::log10(1.0 + (kGain - 1.0) + (1.0 / kGain - 1.0));   // +3.50 dB
constexpr double kCentreTolerance = 0.2;   // the grid (12 per octave) does not hit the centre exactly
}

// W8.7: interactions between bands (docs/eqmodel/interactions.md). One test per plugin (PluginLabInteractions_*).
class InteractionTests : public juce::UnitTest
{
public:
    using Check = std::function<void(InteractionTests&, const engine::InteractionResult&)>;

    InteractionTests(const juce::String& pluginName, std::function<juce::File()> file, Check check)
        : juce::UnitTest("Interactions (W8.7): " + pluginName, "pluginlab"), m_pluginName(pluginName), m_file(std::move(file)), m_check(std::move(check))
    {
    }

    void runTest() override
    {
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        beginTest(m_pluginName + ": the bands add or interact as the code says");
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, m_file());
        juce::String error;
        const std::optional<engine::Mapping> mapping =
            engine::Mapping::read(juce::File(PLUGINLAB_MAPPING_DIR).getChildFile(engine::getMappingFileName("PluginLab " + m_pluginName)), error);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + m_file().getFullPathName());
        expect(mapping.has_value(), error);
        if (! scan.descriptions.isEmpty() && mapping)
        {
            const engine::SurveySubject subject = engine::makePluginSubject(*m_formatManager, scan.descriptions[0], 48000.0);
            const engine::InteractionResult result = engine::measureInteractions(subject, *mapping);
            logMessage(engine::createInteractionReport(result));
            m_check(*this, result);
        }
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

    void expectAdditive(const engine::InteractionResult& result)
    {
        expect(result.measured, result.conclusion);
        expect(result.additive, result.conclusion);
        for (const engine::InteractionCase& item : result.cases)
        {
            expect(item.note.isEmpty(), item.note);
        }
    }

    void expectParallel(const engine::InteractionResult& result)
    {
        expect(result.measured && ! result.additive, result.conclusion);
        for (const engine::InteractionCase& item : result.cases)
        {
            expect(item.note.isEmpty(), item.note);
            if (std::abs(item.frequencyB - item.frequencyA) < 1.0e-6)
            {
                const double expected = item.gainB > 0.0 ? kParallelEqualDb : kParallelOppositeDb;
                expect(item.interacts);
                expectWithinAbsoluteError(item.largestDeviationDb, expected, kCentreTolerance,
                                          "same frequency, gains " + juce::String(item.gainA) + " / " + juce::String(item.gainB));
            }
        }
    }

    void expectNothingToCombine(const engine::InteractionResult& result)
    {
        expect(! result.measured && result.cases.empty(), result.conclusion);
        expect(result.conclusion.contains("one band"), result.conclusion);
    }

private:
    juce::String m_pluginName;
    std::function<juce::File()> m_file;
    Check m_check;
    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static InteractionTests interactionsTwoBands("Test EQ Two Bands", [] { return testpaths::getEqTwoBandsPlugin(); },
                                             [](InteractionTests& test, const engine::InteractionResult& result) { test.expectAdditive(result); });
static InteractionTests interactionsParallel("Test EQ Parallel", [] { return testpaths::getEqParallelPlugin(); },
                                             [](InteractionTests& test, const engine::InteractionResult& result) { test.expectParallel(result); });
static InteractionTests interactionsReferenceEq("Reference EQ", [] { return testpaths::getReferencePlugin("Eq"); },
                                                [](InteractionTests& test, const engine::InteractionResult& result) { test.expectNothingToCombine(result); });
