#include <cmath>

#include "TestPluginPaths.h"
#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace
{
constexpr double kRateRatio = 96000.0 / 44100.0;
constexpr int kLatencyPluginDelay = 64;
constexpr int kLiarPluginDelay = 100;
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

        beginTest("the gain plugin: a clean fingerprint, no feature in the response, one parameter changes the audio");
        const pluginlab::engine::PluginFingerprint gain = measure(testpaths::getGainPlugin());
        expect(gain.loaded, gain.message);
        expect(gain.supportsStereo && gain.supportsMono);
        expectEquals(static_cast<int>(gain.parameters.size()), 4);
        expect(! gain.reactingParameters.empty() && gain.reactingParameters[0] == 0, "the gain must change the audio");
        expectEquals(static_cast<int>(gain.rates.size()), 3);
        expect(! gain.responseFollowsSampleRate);
        expect(gain.blockSizeIndependent);
        expect(gain.deterministic);
        expect(gain.outputStaysFinite && gain.recoversFromJumps && gain.silenceStaysSilent);
        for (const pluginlab::engine::DeliveryResult& delivery : gain.delivery)
        {
            expect(delivery.passed, delivery.name + ": " + delivery.comment);
        }
        expect(gain.findings.empty(), "findings: " + juce::StringArray(gain.findings.data(), static_cast<int>(gain.findings.size())).joinIntoString("; "));
        logMessage(pluginlab::engine::createReport(gain));

        beginTest("the plugin that reports 0 samples latency but delays by 100: the report differs from the measurement at all rates");
        const pluginlab::engine::PluginFingerprint liar = measure(testpaths::getLatencyLiarPlugin());
        expectEquals(static_cast<int>(liar.rates.size()), 3);
        for (const pluginlab::engine::RateFingerprint& rate : liar.rates)
        {
            expectEquals(rate.reportedLatency, 0);
            expectEquals(rate.measuredLatency, kLiarPluginDelay);
        }
        expect(! liar.findings.empty() && liar.findings[0].contains("reports 0 samples"), "the lie must be a finding");
        const pluginlab::engine::PluginFingerprint honest = measure(testpaths::getLatencyPlugin());
        expectEquals(honest.rates[0].measuredLatency, kLatencyPluginDelay);
        expect(honest.findings.empty());

        beginTest("the correct EQ: a bell at every rate, at the same frequency, no fault");
        const pluginlab::engine::PluginFingerprint eq = measure(testpaths::getEqPlugin());
        expect(eq.loaded, eq.message);
        expect(eq.reactingParameters.size() >= 3, "gain, frequency and Q must change the audio (the VST3 wrapper adds a bypass)");
        for (const pluginlab::engine::RateFingerprint& rate : eq.rates)
        {
            expect(rate.hasFeature, "a bell was expected at " + juce::String(rate.sampleRate));
        }
        expect(! eq.responseFollowsSampleRate, "the correct EQ must not follow the sample rate (ratio " + juce::String(eq.featureRatio) + ")");
        expectWithinAbsoluteError(eq.featureRatio, 1.0, 0.15);
        expect(eq.blockSizeIndependent);
        expect(eq.deterministic);
        for (const pluginlab::engine::DeliveryResult& delivery : eq.delivery)
        {
            expect(delivery.passed, delivery.name + ": " + delivery.comment);
        }
        expect(eq.recommendedDelivery.isNotEmpty());
        logMessage(pluginlab::engine::createReport(eq));

        beginTest("the EQ designed for 44.1 kHz (the fault of the own PeakEQ): the bell moves with the sample rate");
        const pluginlab::engine::PluginFingerprint fsFault = measure(testpaths::getEqFsFaultPlugin());
        expect(fsFault.responseFollowsSampleRate, "the fault must be found");
        expectWithinAbsoluteError(fsFault.featureRatio, kRateRatio, 0.15);
        bool mentioned = false;
        for (const juce::String& finding : fsFault.findings)
        {
            mentioned = mentioned || finding.contains("designed for one sample rate");
        }
        expect(mentioned, "the finding must say it");
        logMessage(pluginlab::engine::createReport(fsFault));

        beginTest("the EQ that resets in prepareToPlay: the first delivery is lost, only a poked delivery works");
        const pluginlab::engine::PluginFingerprint prepareFault = measure(testpaths::getEqPrepareFaultPlugin());
        expectEquals(static_cast<int>(prepareFault.delivery.size()), 4);
        if (prepareFault.delivery.size() == 4)
        {
            expect(! prepareFault.delivery[0].passed, "stream: the first A is the hard-coded one, the last A the real one");
            expect(! prepareFault.delivery[0].repeatable);
            expect(! prepareFault.delivery[1].passed, "set before prepare must fail");
            expect(! prepareFault.delivery[1].correct, "A was delivered as no change: the hard-coded values stay");
            expect(! prepareFault.delivery[2].passed, "set after prepare, but not changed: lost");
            expect(! prepareFault.delivery[2].correct);
            expect(prepareFault.delivery[3].passed, "poked: " + prepareFault.delivery[3].comment);
        }
        expect(prepareFault.recommendedDelivery.contains("another value"), prepareFault.recommendedDelivery);
        logMessage(pluginlab::engine::createReport(prepareFault));
    }

private:
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
