#include <cmath>

#include "TestPluginPaths.h"
#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/engine/FingerprintSettings.h"
#include "pluginlab/engine/ReportText.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr int kLatencyPluginDelay = 64;
constexpr int kLiarPluginDelay = 100;
constexpr int kLinearPhaseDelay = 127;
}

// The fingerprint against plugins with known behavior: it must find the faults that were built in and none in the correct plugins.
class FingerprintTests : public juce::UnitTest
{
public:
    FingerprintTests()
        : juce::UnitTest("PluginFingerprint", "pluginlab")
    {
    }

    void runTest() override
    {
        pluginlab::hosting::addHeadlessFormats(m_formats);
        testSettings();

        beginTest("the gain plugin: a clean fingerprint, the gain changes the audio, steps and contexts are named");
        const pluginlab::engine::PluginFingerprint gain = measure(testpaths::getGainPlugin());
        expect(gain.loaded, gain.message);
        expect(gain.supportsStereo && gain.supportsMono);
        expectEquals(static_cast<int>(gain.parameters.size()), 4);
        expectEquals(gain.numberOfParameters, 4);
        expect(! gain.reactingParameters.empty() && gain.reactingParameters[0] == 0, "the gain must change the audio");
        expectEquals(gain.parameters[0].measuredWith, juce::String("defaults"));
        expect(gain.parameters[3].measuredWith.contains("the others at"), "the bypass reacts only with the gain moved: " + gain.parameters[3].measuredWith);
        expect(gain.timeInvariant);
        expect(gain.recoversContinuous && gain.recoversDiscrete);
        expectEquals(gain.scanBase, juce::String("the defaults"));
        expect(! gain.settingB.empty() && gain.settingB[0].name == "Gain" && gain.settingB[0].textB.contains("12"), "B: gain at +12 dB");
        expectEquals(static_cast<int>(gain.rates.size()), 3);
        for (const pluginlab::engine::RateFingerprint& rate : gain.rates)
        {
            expect(rate.outputFound);
            expectEquals(rate.measuredLatency, 0);
            expect(rate.outputBeforePeakDb <= -199.0, "no output before the peak of a gain");
            expect(rate.outputBeforeImpulseDbfs <= -199.0);
        }
        expect(gain.blockSizeIndependent);
        expect(gain.deterministic);
        expect(gain.outputStaysFinite && gain.recoversFromJumps && gain.silenceStaysSilent);
        for (const pluginlab::engine::DeliveryResult& delivery : gain.delivery)
        {
            expect(delivery.passed, delivery.name + ": " + delivery.comment);
        }
        expect(gain.findings.empty(), "findings: " + juce::StringArray(gain.findings.data(), static_cast<int>(gain.findings.size())).joinIntoString("; "));
        const juce::String report = pluginlab::engine::createReport(gain);
        expect(report.contains("continuous") && report.contains("switch"), "the steps are named");
        expect(! report.contains("2147483647"));
        expect(report.contains("## Settings used"));
        expect(! report.contains("feature"), "the response analysis is gone (it belongs to the analyzer)");
        expect(report.indexOf("## Summary") < report.indexOf("## Findings"), "the summary comes first");
        const std::vector<pluginlab::engine::SummaryItem> gainSummary = pluginlab::engine::summarize(gain);
        for (const pluginlab::engine::SummaryItem& item : gainSummary)
        {
            expect(item.good || item.key == "parameters", "the gain plugin: " + item.key + " = " + item.result);
        }
        const std::vector<pluginlab::engine::SummaryItem> roundTrip =
            pluginlab::engine::parseSummaryJson(pluginlab::engine::createSummaryJson(gain));
        expectEquals(static_cast<int>(roundTrip.size()), static_cast<int>(gainSummary.size()));
        if (roundTrip.size() == gainSummary.size())
        {
            for (size_t index = 0; index < roundTrip.size(); ++index)
            {
                expectEquals(roundTrip[index].key, gainSummary[index].key);
                expectEquals(roundTrip[index].result, gainSummary[index].result);
                expect(roundTrip[index].good == gainSummary[index].good);
            }
        }
        expect(pluginlab::engine::parseSummaryJson("not json").empty());
        logMessage(report);

        beginTest("channels: the gain plugin takes mono and stereo, not surround; its channels are independent; no side chain");
        expectEquals(gain.measuredChannels, 2);
        for (const pluginlab::engine::LayoutFingerprint& layout : gain.layouts)
        {
            const bool expected = layout.name == "mono" || layout.name == "stereo";
            expect(layout.accepted == expected, layout.name);
        }
        expect(! gain.hasSideChain);
        expect(gain.couplingMeasured && gain.channelsIndependent);

        beginTest("channels: cross feed is found (R gets half of L: -6 dB), a width control reacts only with L != R, a side chain is listed and switched off");
        const pluginlab::engine::PluginFingerprint crossFeed = measure(testpaths::getCrossFeedPlugin());
        expect(crossFeed.couplingMeasured && ! crossFeed.channelsIndependent, "the cross feed must be found");
        expectWithinAbsoluteError(crossFeed.couplingLeftToRight.relativeDb, -6.02, 0.1);
        expect(crossFeed.couplingRightToLeft.relativeDb < -100.0, "nothing goes from R to L");
        const pluginlab::engine::PluginFingerprint width = measure(testpaths::getWidthPlugin());
        expect(width.parameters[0].changesTheAudio, "the width gain changes the audio with L != R");
        expect(width.parameters[0].changeSame.relativeDb < -80.0, "with L = R the width does nothing: " + juce::String(width.parameters[0].changeSame.relativeDb));
        expect(width.parameters[0].changeDifferent.relativeDb > -80.0);
        const pluginlab::engine::PluginFingerprint sideChain = measure(testpaths::getSideChainPlugin());
        expect(sideChain.loaded, sideChain.message);
        expect(sideChain.hasSideChain);
        bool listed = false;
        for (const pluginlab::engine::BusFingerprint& bus : sideChain.buses)
        {
            listed = listed || (bus.isInput && bus.index == 1 && bus.name == "Sidechain");
        }
        expect(listed, "the side-chain bus must be listed");
        expect(! sideChain.reactingParameters.empty(), "the plugin is measured with the side chain switched off");
        const pluginlab::engine::PluginFingerprint mono = measure(testpaths::getMonoPlugin());
        expectEquals(mono.measuredChannels, 1);
        expect(! mono.couplingMeasured);

        beginTest("time-varying: the tremolo is found, and determinism, recovery and delivery are not counted as faults");
        const pluginlab::engine::PluginFingerprint tremolo = measure(testpaths::getTremoloPlugin());
        expect(! tremolo.timeInvariant, "the tremolo must be time-varying");
        bool timeVaryingMentioned = false;
        for (const juce::String& finding : tremolo.findings)
        {
            timeVaryingMentioned = timeVaryingMentioned || finding.contains("time-varying");
        }
        expect(timeVaryingMentioned);
        for (const pluginlab::engine::SummaryItem& item : pluginlab::engine::summarize(tremolo))
        {
            expect(item.good || item.key == "parameters", "time-varying, so not a fault: " + item.key + " = " + item.result);
        }

        beginTest("parameters that act only together: the EQ band is off by default, the scan flips the switch and finds gain, frequency and Q");
        const pluginlab::engine::PluginFingerprint eqSwitch = measure(testpaths::getEqSwitchPlugin());
        expect(eqSwitch.reactingParameters.size() >= 3, "gain, frequency and Q: " + juce::String(static_cast<int>(eqSwitch.reactingParameters.size())));
        expect(eqSwitch.scanBase.contains("switch"), eqSwitch.scanBase);
        expect(! eqSwitch.settingB.empty());
        for (const pluginlab::engine::DeliveryResult& delivery : eqSwitch.delivery)
        {
            expect(delivery.passed, delivery.name + ": " + delivery.comment);
        }

        beginTest("the plugin that reports 0 samples latency but delays by 100: the report differs from the measurement at all rates");
        const pluginlab::engine::PluginFingerprint liar = measure(testpaths::getLatencyLiarPlugin());
        for (const pluginlab::engine::RateFingerprint& rate : liar.rates)
        {
            expectEquals(rate.reportedAfterPrepare, 0);
            expectEquals(rate.reportedAfterAudio, 0);
            expectEquals(rate.measuredLatency, kLiarPluginDelay);
        }
        expect(! liar.findings.empty() && liar.findings[0].contains("reports 0 samples, measured 100"), "the lie must be a finding");
        bool latencyFlagged = false;
        for (const pluginlab::engine::SummaryItem& item : pluginlab::engine::summarize(liar))
        {
            if (item.key == "latency")
            {
                latencyFlagged = ! item.good && item.result == "0 / 100";
            }
        }
        expect(latencyFlagged, "the summary must show 0 / 100 as worth a look");
        const pluginlab::engine::PluginFingerprint honest = measure(testpaths::getLatencyPlugin());
        expectEquals(honest.rates[0].measuredLatency, kLatencyPluginDelay);
        expect(honest.findings.empty());

        beginTest("the linear-phase plugin: latency 127 measured with the delayed impulse, output before the peak (pre-ringing)");
        const pluginlab::engine::PluginFingerprint linear = measure(testpaths::getLinearPhasePlugin());
        for (const pluginlab::engine::RateFingerprint& rate : linear.rates)
        {
            expectEquals(rate.measuredLatency, kLinearPhaseDelay);
            expectEquals(rate.reportedAfterAudio, kLinearPhaseDelay);
            expect(rate.outputBeforePeakDb > -60.0, "pre-ringing expected: " + juce::String(rate.outputBeforePeakDb));
        }
        expect(linear.findings.empty());

        beginTest("block sizes: smoothing per block differs only in the transient, a low-pass reset per block also in the steady state");
        const pluginlab::engine::PluginFingerprint smoothing = measure(testpaths::getBlockSmoothingPlugin());
        expect(smoothing.blockSizeIndependent, "the steady state must not depend on the block size");
        bool transientDiffers = false;
        for (const pluginlab::engine::BlockSizeResult& block : smoothing.blockSizes)
        {
            transientDiffers = transientDiffers || block.whole.relativeDb > -100.0;
        }
        expect(transientDiffers, "the transient of the smoothing differs between block sizes");
        const pluginlab::engine::PluginFingerprint fault = measure(testpaths::getBlockFaultPlugin());
        expect(! fault.blockSizeIndependent, "the fault must be found");
        bool mentioned = false;
        for (const juce::String& finding : fault.findings)
        {
            mentioned = mentioned || finding.contains("depends on the block size");
        }
        expect(mentioned);

        beginTest("the EQ that resets in prepareToPlay: the first delivery is lost, only a poked delivery works");
        const pluginlab::engine::PluginFingerprint prepareFault = measure(testpaths::getEqPrepareFaultPlugin());
        expectEquals(static_cast<int>(prepareFault.delivery.size()), 4);
        if (prepareFault.delivery.size() == 4)
        {
            expect(! prepareFault.delivery[0].passed, "stream: the first A is the hard-coded one, the last A the real one");
            expect(! prepareFault.delivery[0].repeatable);
            expect(! prepareFault.delivery[1].passed, "set before prepare must fail");
            expect(! prepareFault.delivery[1].correct);
            expect(! prepareFault.delivery[2].passed, "set after prepare, but not changed: lost");
            expect(! prepareFault.delivery[2].correct);
            expect(prepareFault.delivery[3].passed, "poked: " + prepareFault.delivery[3].comment);
        }
        expect(prepareFault.recommendedDelivery.contains("another value"), prepareFault.recommendedDelivery);

        beginTest("the tables of a report are aligned for a window with a monospaced font, the other lines stay");
        const juce::String markdown = "# Title\n\ntext line\n| a | long header |\n|---|---|\n| wide cell | b |\n\n- list\n";
        const juce::String aligned = pluginlab::engine::alignMarkdownTables(markdown);
        expect(aligned.contains("# Title\n"));
        expect(aligned.contains("a          long header\n"), aligned);
        expect(aligned.contains("---------  -----------\n"), aligned);
        expect(aligned.contains("wide cell  b\n"), aligned);
        expect(aligned.contains("- list\n"));
        expect(! pluginlab::engine::alignMarkdownTables(report).contains("|---"), "no Markdown separator line may be left");
    }

private:
    void testSettings()
    {
        beginTest("the settings: a missing file is written with the defaults, a changed value is read back, a broken file gives the defaults");
        const juce::TemporaryFile file(".json");
        juce::String warning;
        const pluginlab::engine::FingerprintSettings defaults = pluginlab::engine::FingerprintSettings::loadOrCreate(file.getFile(), warning);
        expect(warning.isEmpty(), warning);
        expect(file.getFile().existsAsFile(), "the defaults must be written");
        expectEquals(defaults.reactsAboveDb, -80.0);
        expectEquals(static_cast<int>(defaults.blockSizes.size()), 7);

        pluginlab::engine::FingerprintSettings changed = defaults;
        changed.reactsAboveDb = -70.0;
        changed.blockSizes = {64, 128};
        expect(changed.save(file.getFile()));
        const pluginlab::engine::FingerprintSettings readBack = pluginlab::engine::FingerprintSettings::loadOrCreate(file.getFile(), warning);
        expectEquals(readBack.reactsAboveDb, -70.0);
        expectEquals(static_cast<int>(readBack.blockSizes.size()), 2);
        expectEquals(readBack.sameBelowDb, defaults.sameBelowDb);

        expect(file.getFile().replaceWithText("{ \"reactsAboveDb\": -50.0 }"));
        const pluginlab::engine::FingerprintSettings partial = pluginlab::engine::FingerprintSettings::loadOrCreate(file.getFile(), warning);
        expectEquals(partial.reactsAboveDb, -50.0);
        expectEquals(partial.settleSeconds, defaults.settleSeconds); // a missing key keeps its default

        expect(file.getFile().replaceWithText("this is not json"));
        warning.clear();
        const pluginlab::engine::FingerprintSettings broken = pluginlab::engine::FingerprintSettings::loadOrCreate(file.getFile(), warning);
        expect(warning.isNotEmpty());
        expectEquals(broken.reactsAboveDb, defaults.reactsAboveDb);

        expectEquals(pluginlab::engine::describeSteps(0x7fffffff), juce::String("continuous"));
        expectEquals(pluginlab::engine::describeSteps(2), juce::String("switch"));
        expectEquals(pluginlab::engine::describeSteps(14), juce::String("14"));
    }

    pluginlab::engine::PluginFingerprint measure(const juce::File& file)
    {
        const pluginlab::hosting::PluginScanResult scan = pluginlab::hosting::PluginScanner::scanFileInProcess(m_formats, file);
        expect(! scan.descriptions.isEmpty(), "cannot scan " + file.getFullPathName());
        if (scan.descriptions.isEmpty())
        {
            return {};
        }
        return pluginlab::engine::measureFingerprint(m_formats, scan.descriptions[0]);
    }

    juce::AudioPluginFormatManager m_formats;
};

static FingerprintTests fingerprintTests;
