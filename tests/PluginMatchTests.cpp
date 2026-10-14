#include <cmath>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "TestPluginPaths.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/engine/PluginMatch.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
namespace engine = pluginlab::engine;
namespace eq = pluginlab::eqmodel;
namespace ref = pluginlab::reference;
namespace match = pluginlab::match;

constexpr double kSampleRate = 48000.0;
constexpr double kFsFaultKnobHz = 1000.0 * 44100.0 / 48000.0; // the Fs Fault designs for 44.1 kHz: at 48 kHz its knob must show 919 Hz

eq::Band makeBand(eq::DesignFamily family, double frequency, double gain, double q)
{
    eq::Band band;
    band.family = family;
    band.type = ref::FilterType::Peak;
    band.frequencyHz = frequency;
    band.gainDb = gain;
    band.q = q;
    return band;
}
}

// W9.4: matching plugins (docs/matching/plugin-matching.md): the test pairs of the plan (section 6) through the fast render path, each within
// its budget; the main case of several targets against one master.
// Two objects: the matching of W9.4 and the attribution of W9.5 (why not closer), each a CTest of its own.
class PluginMatchTests : public juce::UnitTest
{
public:
    explicit PluginMatchTests(bool attribution)
        : juce::UnitTest(attribution ? "Match attribution (W9.5)" : "Plugin matching (W9.4)", "pluginlab"), m_attribution(attribution)
    {
    }

    void runTest() override
    {
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        if (m_attribution)
        {
            testAttribution();
            m_formatManager.reset();
            return;
        }
        beginTest("the plugins of the test pairs load");
        const auto referenceEq = scan(testpaths::getReferencePlugin("Eq"));
        const auto testEq = scan(testpaths::getEqPlugin());
        const auto knobFaults = scan(testpaths::getEqKnobFaultPlugin());
        const auto fsFault = scan(testpaths::getEqFsFaultPlugin());
        const auto twoBands = scan(testpaths::getEqTwoBandsPlugin());
        const auto eqSwitch = scan(testpaths::getEqSwitchPlugin());
        const auto drive = scan(testpaths::getEqDrivePlugin());
        if (! referenceEq || ! testEq || ! knobFaults || ! fsFault || ! twoBands || ! eqSwitch || ! drive)
        {
            return;
        }
        // a user's rough start for the one-band test EQs: about +3 dB, 700 Hz, Q 1.5 (their knob laws); the switch EQ's band switched on
        const std::vector<float> roughStart = {0.5625f, 0.363f, 0.141f};

        beginTest("the main case: one master (Reference EQ, RBJ peak 1 kHz +6 dB Q 2), three targets (Test EQ, Knob Faults, Fs Fault)");
        {
            const engine::MatchMaster master = makeReferenceMaster(*referenceEq, makeBand(eq::DesignFamily::Rbj, 1000.0, 6.0, 2.0));
            const std::vector<engine::MatchTarget> targets = {makeTarget(*testEq, roughStart, {0, 1, 2}), makeTarget(*knobFaults, roughStart, {0, 1, 2}),
                                                              makeTarget(*fsFault, roughStart, {0, 1, 2})};
            const engine::PluginMatchResult result = engine::matchPlugins(*m_formatManager, master, targets);
            logMessage(engine::createPluginMatchReport(result));
            expect(result.error.isEmpty(), result.error);
            for (const engine::TargetMatchResult& item : result.targets)
            {
                expect(! item.refused, item.name + ": " + item.reason);
                expect(item.optimizer.withinTolerance, item.name + ": largest " + juce::String(item.optimizer.largest, 3) + " dB, "
                                                           + match::getStopReasonName(item.optimizer.reason));
                expectLessThan(item.seconds, 30.0, item.name + ": seconds (Debug)");
            }
            if (result.targets.size() == 3 && result.targets[2].values.size() == 3)
            {
                // the Fs Fault's frequency knob shows what a 44.1 kHz design needs to sit at 1 kHz at 48 kHz
                const double shown = result.targets[2].values[1].text.getDoubleValue();
                expectWithinAbsoluteError(shown / kFsFaultKnobHz, 1.0, 0.01, "Fs Fault frequency knob (strict: within 0.1 dB, not exact): " + result.targets[2].values[1].text);
            }
        }

        beginTest("Orfanidis against RBJ: not within the strict tolerance (the residual near Nyquist)");
        {
            const engine::MatchMaster master = makeReferenceMaster(*referenceEq, makeBand(eq::DesignFamily::Orfanidis, 10000.0, 6.0, 1.0));
            const engine::PluginMatchResult result = engine::matchPlugins(*m_formatManager, master, {makeTarget(*testEq, {0.625f, 0.75f, 0.09f}, {0, 1, 2})});
            logMessage(engine::createPluginMatchReport(result));
            expect(result.targets.size() == 1 && ! result.targets[0].refused && ! result.targets[0].optimizer.withinTolerance,
                   "an RBJ peak cannot be an Orfanidis peak near Nyquist");
        }

        beginTest("a band-model curve (two peaks) matched by the two-band test EQ (six parameters)");
        {
            engine::MatchMaster master;
            master.curve = eq::EqModel{{makeBand(eq::DesignFamily::Rbj, 300.0, 5.0, 1.0), makeBand(eq::DesignFamily::Rbj, 3000.0, -4.0, 2.0)}, 0.0};
            std::vector<float> start = engine::getDefaultSetting(*m_formatManager, *twoBands, kSampleRate);
            start[0] = 0.5625f; // +3 dB
            start[3] = 0.4583f; // -2 dB
            const engine::PluginMatchResult result = engine::matchPlugins(*m_formatManager, master, {makeTarget(*twoBands, start, {0, 1, 2, 3, 4, 5}, true)});
            logMessage(engine::createPluginMatchReport(result));
            expect(result.targets.size() == 1 && result.targets[0].optimizer.withinTolerance, "two bands");
        }

        beginTest("the switch EQ with its band switched on by the user");
        {
            const engine::MatchMaster master = makeReferenceMaster(*referenceEq, makeBand(eq::DesignFamily::Rbj, 1000.0, 6.0, 2.0));
            const engine::PluginMatchResult result =
                engine::matchPlugins(*m_formatManager, master, {makeTarget(*eqSwitch, {0.5625f, 0.363f, 0.141f, 1.0f}, {0, 1, 2})});
            expect(result.targets.size() == 1 && result.targets[0].optimizer.withinTolerance, "switch EQ");
        }

        beginTest("a non-LTI target is refused: the drive EQ with its drive on");
        {
            const engine::MatchMaster master = makeReferenceMaster(*referenceEq, makeBand(eq::DesignFamily::Rbj, 1000.0, 6.0, 2.0));
            std::vector<float> start = engine::getDefaultSetting(*m_formatManager, *drive, kSampleRate);
            start[3] = 1.0f; // drive 24 dB
            const engine::PluginMatchResult result = engine::matchPlugins(*m_formatManager, master, {makeTarget(*drive, start, {0, 1, 2})});
            expect(result.targets.size() == 1 && result.targets[0].refused && result.targets[0].reason.contains("not LTI"),
                   result.targets.empty() ? juce::String() : result.targets[0].reason);
        }

        beginTest("a far start (the band at 38 Hz for a 1 kHz master): the model start through the target's mapping");
        {
            const engine::MatchMaster master = makeReferenceMaster(*referenceEq, makeBand(eq::DesignFamily::Rbj, 1000.0, 6.0, 2.0));
            engine::MatchTarget target = makeTarget(*testEq, {0.5f, 0.15f, 0.09f}, {0, 1, 2});
            target.mapping = readMapping("PluginLab Test EQ");
            target.modelStart = true;
            const engine::PluginMatchResult result = engine::matchPlugins(*m_formatManager, master, {target});
            logMessage(engine::createPluginMatchReport(result));
            expect(target.mapping.has_value(), "the Test EQ's mapping");
            expect(result.targets.size() == 1 && result.targets[0].startedFromModel && result.targets[0].optimizer.withinTolerance, "model start");
        }
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
    // the cause the code has built in must be the first one named
    void expectCause(const engine::PluginMatchResult& result, engine::MatchCause expected)
    {
        logMessage(engine::createPluginMatchReport(result));
        expect(result.targets.size() == 1, "one target");
        if (result.targets.size() != 1)
        {
            return;
        }
        const engine::TargetMatchResult& item = result.targets.front();
        expect(! item.refused, item.reason);
        expect(! item.optimizer.withinTolerance, "must miss the tolerance (largest " + juce::String(item.optimizer.largest, 3) + " dB)");
        expect(! item.causes.empty() && item.causes.front().cause == expected,
               juce::String("expected: ") + engine::getMatchCauseName(expected) + ", named: "
                   + (item.causes.empty() ? juce::String("nothing") : juce::String(engine::getMatchCauseName(item.causes.front().cause))));
    }

    void testAttribution()
    {
        beginTest("the plugins of the attribution load");
        const auto referenceEq = scan(testpaths::getReferencePlugin("Eq"));
        const auto testEq = scan(testpaths::getEqPlugin());
        const auto limited = scan(testpaths::getEqLimitedPlugin());
        const auto parallel = scan(testpaths::getEqParallelPlugin());
        if (! referenceEq || ! testEq || ! limited || ! parallel)
        {
            return;
        }
        beginTest("a knob at the end of its range: a peak at 8 kHz, the target's frequency knob ends at 5 kHz");
        expectCause(engine::matchPlugins(*m_formatManager, makeReferenceMaster(*referenceEq, makeBand(eq::DesignFamily::Rbj, 8000.0, 6.0, 2.0)),
                                         {makeTarget(*limited, {0.5625f, 0.6f, 0.141f}, {0, 1, 2})}),
                    engine::MatchCause::KnobRange);

        beginTest("another design family: Orfanidis at 10 kHz against an RBJ target");
        expectCause(engine::matchPlugins(*m_formatManager, makeReferenceMaster(*referenceEq, makeBand(eq::DesignFamily::Orfanidis, 10000.0, 6.0, 1.0)),
                                         {makeTarget(*testEq, {0.625f, 0.75f, 0.09f}, {0, 1, 2})}),
                    engine::MatchCause::DesignFamily);

        beginTest("a band missing: two peaks against a one-band target");
        {
            engine::MatchMaster master;
            master.curve = eq::EqModel{{makeBand(eq::DesignFamily::Rbj, 300.0, 5.0, 1.0), makeBand(eq::DesignFamily::Rbj, 3000.0, -4.0, 2.0)}, 0.0};
            expectCause(engine::matchPlugins(*m_formatManager, master, {makeTarget(*testEq, {0.5625f, 0.363f, 0.141f}, {0, 1, 2})}), engine::MatchCause::BandMissing);
        }

        beginTest("the bands interact: two overlapping peaks (in series) against the parallel two-band EQ");
        {
            engine::MatchMaster master;
            master.curve = eq::EqModel{{makeBand(eq::DesignFamily::Rbj, 1000.0, 9.0, 1.0), makeBand(eq::DesignFamily::Rbj, 1414.0, 9.0, 1.0)}, 0.0};
            std::vector<float> start = engine::getDefaultSetting(*m_formatManager, *parallel, kSampleRate);
            engine::MatchTarget target = makeTarget(*parallel, start, {0, 1, 2, 3, 4, 5}, true);
            target.mapping = readMapping("PluginLab Test EQ Parallel");
            target.modelStart = true;
            expectCause(engine::matchPlugins(*m_formatManager, master, {target}), engine::MatchCause::Interaction);
        }
    }

    std::optional<juce::PluginDescription> scan(const juce::File& file)
    {
        const pluginlab::hosting::PluginScanResult result = pluginlab::hosting::PluginScanner::scanFileInProcess(*m_formatManager, file);
        expect(! result.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (result.descriptions.isEmpty())
        {
            return std::nullopt;
        }
        return result.descriptions[0];
    }

    std::optional<engine::Mapping> readMapping(const juce::String& pluginName)
    {
        juce::String error;
        return engine::Mapping::read(juce::File(PLUGINLAB_MAPPING_DIR).getChildFile(engine::getMappingFileName(pluginName)), error);
    }

    // the Reference EQ set to a band through its mapping (W8.5)
    engine::MatchMaster makeReferenceMaster(const juce::PluginDescription& referenceEq, const eq::Band& band)
    {
        engine::MatchMaster master;
        master.plugin = referenceEq;
        const std::optional<engine::Mapping> mapping = readMapping("PluginLab Reference EQ");
        expect(mapping.has_value(), "the Reference EQ's mapping");
        juce::StringArray missing;
        master.setting = engine::makeSettingForBand(*mapping, engine::getDefaultSetting(*m_formatManager, referenceEq, kSampleRate), 0, band, missing);
        expect(missing.isEmpty(), missing.joinIntoString(", "));
        return master;
    }

    engine::MatchTarget makeTarget(const juce::PluginDescription& plugin, const std::vector<float>& values, std::vector<int> released, bool whole = false)
    {
        engine::MatchTarget target;
        target.plugin = plugin;
        target.start = whole ? values : engine::getDefaultSetting(*m_formatManager, plugin, kSampleRate);
        if (! whole)
        {
            for (size_t index = 0; index < values.size() && index < target.start.size(); ++index)
            {
                target.start[index] = values[index];
            }
        }
        target.released = std::move(released);
        return target;
    }

    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
    bool m_attribution = false;
};

static PluginMatchTests pluginMatchTests(false);
static PluginMatchTests matchAttributionTests(true);
