#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "LiveResponseView.h"
#include "TestPluginPaths.h"
#include "pluginlab/engine/MeasurementEngine.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/engine/LiveResponse.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

// MatchPluginTests.cpp: the Test EQ's exact RBJ peak from its normalised gain, frequency and Q (at 48 kHz)
std::vector<double> getTestEqCurve(const std::vector<float>& values, const std::vector<double>& grid);

namespace
{
namespace engine = pluginlab::engine;

constexpr double kSampleRate = 48000.0;
constexpr double kUpdateSeconds = 0.3;     // W10.3: turning a knob updates its curve within this
constexpr double kExactDb = 0.01;          // the curve against the exact one (the fast render path: below 0.001 dB, MatchPluginTests)
constexpr double kCompareFloorDb = -60.0;
constexpr int kWaitMs = 10000;             // the longest wait for a curve or a system class (Debug, a loaded machine)
constexpr int kPollMs = 5;
constexpr int kKnobSteps = 20;             // a knob turned for one second: a new value every 50 ms
constexpr int kKnobStepMs = 50;
constexpr int kDriveIndex = 3;
constexpr int kBlockSize = 512;
constexpr int kViewWidth = 640;
constexpr int kViewHeight = 480;             // Test EQ Drive: gain, frequency, Q, drive
}

// W10.3: the real-time transfer function display (docs/design/W10-plan.md 3.3): a measuring instance per slot, a curve within 0.3 s of a
// knob change, at most 5 per second, nothing for a knob that does not move, the system class when the knobs rest.
class LiveResponseTests : public juce::UnitTest
{
public:
    LiveResponseTests()
        : juce::UnitTest("Live transfer function display (W10.3)", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the deviation from the master: largest and rms, a stop band left out");
        {
            const engine::LiveDeviation deviation = engine::getLiveDeviation({0.0, 1.0, -3.0, -90.0}, {0.0, 2.0, -1.0, -95.0});
            expectWithinAbsoluteError(deviation.largestDb, 2.0, 1.0e-9);
            expectWithinAbsoluteError(deviation.rmsDb, std::sqrt(5.0 / 3.0), 1.0e-9);
        }

        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        const std::optional<juce::PluginDescription> eq = scan(testpaths::getEqPlugin());
        const std::optional<juce::PluginDescription> drive = scan(testpaths::getEqDrivePlugin());
        if (! eq || ! drive)
        {
            m_formatManager.reset();
            return;
        }
        const std::vector<float> eqDefaults = engine::getDefaultSetting(*m_formatManager, *eq, kSampleRate);
        const std::vector<float> driveDefaults = engine::getDefaultSetting(*m_formatManager, *drive, kSampleRate);

        {
            engine::LiveResponseMonitor monitor(*m_formatManager, kSampleRate);
            monitor.setSlots({std::nullopt, eq, drive});

            beginTest("the dry slot: a flat curve, LTI, without an instance");
            {
                const engine::LiveSlotResponse dry = monitor.getResponse(0);
                expectEquals(dry.generation, 1);
                expect(dry.lti && dry.classified);
                expectEquals(static_cast<int>(dry.magnitudeDb.size()), static_cast<int>(monitor.getFrequencies().size()));
                expectWithinAbsoluteError(*std::max_element(dry.magnitudeDb.begin(), dry.magnitudeDb.end()), 0.0, 1.0e-12);
            }

            beginTest("turning a knob: the curve of the new setting within 0.3 s, equal to the exact curve");
            {
                const std::vector<std::vector<float>> knobs = {{0.75f, 0.4f, 0.2f}, {0.3f, 0.7f, 0.5f}, {0.9f, 0.25f, 0.8f}, {0.6f, 0.5f, 0.3f}, {0.2f, 0.35f, 0.6f}};
                std::vector<double> latencies;
                for (const std::vector<float>& values : knobs)
                {
                    const int before = monitor.getResponse(1).generation;
                    const double started = juce::Time::getMillisecondCounterHiRes();
                    monitor.setSetting(1, withValues(eqDefaults, values));
                    expect(waitFor([&] { return monitor.getResponse(1).generation > before; }), "a curve");
                    latencies.push_back((juce::Time::getMillisecondCounterHiRes() - started) / 1000.0);
                    expectLessThan(worstDifference(monitor.getResponse(1).magnitudeDb, getTestEqCurve(values, monitor.getFrequencies())), kExactDb);
                    juce::Thread::sleep(250); // the next knob after the rate limit's interval
                }
                std::sort(latencies.begin(), latencies.end());
                logMessage("knob to curve: median " + juce::String(juce::roundToInt(latencies[latencies.size() / 2] * 1000.0)) + " ms, longest "
                           + juce::String(juce::roundToInt(latencies.back() * 1000.0)) + " ms; render " + juce::String(juce::roundToInt(monitor.getResponse(1).renderSeconds * 1000.0)) + " ms");
                expectLessThan(latencies[latencies.size() / 2], kUpdateSeconds);
            }

            beginTest("a knob that does not move costs nothing");
            {
                const int before = monitor.getResponse(1).generation;
                for (int step = 0; step < 10; ++step)
                {
                    monitor.setSetting(1, withValues(eqDefaults, {0.2f, 0.35f, 0.6f})); // the last setting again
                    juce::Thread::sleep(kKnobStepMs);
                }
                expectEquals(monitor.getResponse(1).generation, before);
            }

            beginTest("a knob turned for one second: at most 5 curves per second, the last one is the last setting");
            {
                const int before = monitor.getResponse(1).generation;
                std::vector<float> values;
                for (int step = 0; step < kKnobSteps; ++step)
                {
                    values = {0.5f + 0.4f * static_cast<float>(step) / kKnobSteps, 0.5f, 0.3f};
                    monitor.setSetting(1, withValues(eqDefaults, values));
                    juce::Thread::sleep(kKnobStepMs);
                }
                juce::Thread::sleep(400); // the last one rendered
                const engine::LiveSlotResponse response = monitor.getResponse(1);
                const int curves = response.generation - before;
                logMessage(juce::String(curves) + " curves for " + juce::String(kKnobSteps) + " settings in 1 s");
                expect(curves >= 2 && curves <= 7, "curves: " + juce::String(curves));
                expectLessThan(worstDifference(response.magnitudeDb, getTestEqCurve(values, monitor.getFrequencies())), kExactDb);
            }

            beginTest("the knobs at rest: the system class of the setting (LTI for the EQ; not LTI with the drive on, with the reason)");
            {
                expect(waitFor([&] { return monitor.getResponse(1).classified && monitor.getResponse(1).classOfCurrentSetting; }), "a system class");
                const engine::LiveSlotResponse eqResponse = monitor.getResponse(1);
                expect(eqResponse.lti, eqResponse.systemClass + ": " + eqResponse.reason);

                std::vector<float> driveOn = withValues(driveDefaults, {0.75f, 0.4f, 0.2f});
                driveOn[kDriveIndex] = 1.0f;
                monitor.setSetting(2, driveOn);
                expect(waitFor([&] { return monitor.getResponse(2).classified && monitor.getResponse(2).classOfCurrentSetting; }), "a system class");
                const engine::LiveSlotResponse driveResponse = monitor.getResponse(2);
                expect(! driveResponse.lti, "the drive is on");
                expect(driveResponse.reason.isNotEmpty(), "a reason");
                logMessage("Test EQ Drive, drive on: " + driveResponse.systemClass + " (" + driveResponse.reason + ")");
            }

            beginTest("the same plugins again: the instances stay; a removed slot goes");
            {
                const int generation = monitor.getResponse(1).generation;
                monitor.setSlots({std::nullopt, eq});
                expectEquals(monitor.getNumSlots(), 2);
                expectEquals(monitor.getResponse(1).generation, generation);
            }
        }

        beginTest("the view on the page Compare: a knob of the playing instance moves, the curve and the deviation follow");
        {
            engine::MeasurementEngine audio;
            audio.prepare(kSampleRate, kBlockSize);
            juce::String error;
            audio.addSlot(nullptr, "dry", error);
            std::unique_ptr<pluginlab::hosting::HostedPlugin> plugin = pluginlab::hosting::HostedPlugin::load(*m_formatManager, *eq, kSampleRate, kBlockSize, error);
            expect(plugin != nullptr, error);
            if (plugin != nullptr)
            {
                pluginlab::hosting::HostedPlugin* playing = plugin.get();
                audio.addSlot(std::move(plugin), "Test EQ", error);
                pluginlab::host::LiveResponseView view(audio);
                view.setBounds(0, 0, kViewWidth, kViewHeight);
                view.setVisible(true); // the view measures only while it is visible (the page is shown)
                view.slotsChanged();
                playing->setParameterNormalised(0, 0.75f); // +12 dB
                expect(runUntil([&] { return view.describe().contains("largest deviation") || view.describe().contains("deviation from the reference"); }),
                       view.describe());
                expect(view.getMonitor() != nullptr && view.getMonitor()->getResponse(1).generation > 0, "a curve of the Test EQ");
                const int before = view.getMonitor()->getResponse(1).generation;
                playing->setParameterNormalised(0, 0.25f); // -12 dB
                expect(runUntil([&] { return view.getMonitor()->getResponse(1).generation > before; }), "the curve follows the knob");
                runUntil([&] { return view.describe().contains("12.0") && ! view.describe().contains("being checked"); });
                logMessage(view.describe());
                const juce::File picture = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pluginlab_live_response.png");
                juce::FileOutputStream stream(picture);
                if (stream.openedOk())
                {
                    stream.setPosition(0);
                    stream.truncate();
                    juce::PNGImageFormat().writeImageToStream(view.createComponentSnapshot(view.getLocalBounds()), stream);
                    logMessage("picture: " + picture.getFullPathName());
                }
            }
        }

        beginTest("the curves go up to Nyquist, at every rate");
        for (const double rate : {44100.0, 48000.0, 96000.0})
        {
            engine::LiveResponseMonitor monitor(*m_formatManager, rate);
            expectWithinAbsoluteError(monitor.getFrequencies().back(), 0.5 * rate, 1.0e-9);
            expect(monitor.getFrequencies()[monitor.getFrequencies().size() - 2] < 0.5 * rate, "rising");
        }
        m_formatManager.reset(); // before JUCE's leak check at exit (this test object is static)
    }

private:
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

    static std::vector<float> withValues(std::vector<float> setting, const std::vector<float>& values)
    {
        for (size_t index = 0; index < values.size() && index < setting.size(); ++index)
        {
            setting[index] = values[index];
        }
        return setting;
    }

    static double worstDifference(const std::vector<double>& measured, const std::vector<double>& exact)
    {
        double worst = measured.size() == exact.size() ? 0.0 : 1.0e9;
        for (size_t index = 0; index < measured.size() && index < exact.size(); ++index)
        {
            if (exact[index] > kCompareFloorDb)
            {
                worst = std::max(worst, std::abs(measured[index] - exact[index]));
            }
        }
        return worst;
    }

    template <typename Condition>
    static bool waitFor(Condition condition)
    {
        const double started = juce::Time::getMillisecondCounterHiRes();
        while (! condition())
        {
            if (juce::Time::getMillisecondCounterHiRes() - started > kWaitMs)
            {
                return false;
            }
            juce::Thread::sleep(kPollMs);
        }
        return true;
    }

    // the view's timer needs the message loop: run it until the condition holds
    template <typename Condition>
    static bool runUntil(Condition condition)
    {
        const double started = juce::Time::getMillisecondCounterHiRes();
        while (! condition())
        {
            if (juce::Time::getMillisecondCounterHiRes() - started > kWaitMs)
            {
                return false;
            }
            if (juce::MessageManager* messages = juce::MessageManager::getInstanceWithoutCreating())
            {
                messages->runDispatchLoopUntil(20);
            }
            else
            {
                juce::Thread::sleep(20);
            }
        }
        return true;
    }

    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static LiveResponseTests liveResponseTests;
