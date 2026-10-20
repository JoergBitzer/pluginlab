#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "LiveWorker.h"
#include "MatchJob.h"
#include "PageServices.h"
#include "TestPluginPaths.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginScanner.h"

// MatchPluginTests.cpp: the Test EQ's exact RBJ peak from its normalised gain, frequency and Q (at 48 kHz)
std::vector<double> getTestEqCurve(const std::vector<float>& values, const std::vector<double>& grid);

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kWaitMs = 60000;
constexpr int kJobWaitMs = 10 * 60 * 1000;
constexpr double kExactDb = 0.01;
constexpr double kCompareFloorDb = -60.0;
}

// W10.7: crash protection, option (c) of docs/design/W10-plan.md 3.7: everything that measures runs in a process of its own; a plugin that crashes
// there leaves the product running (this test program plays the application or the DAW). Listening stays in process (documented).
class CrashProtectionTests : public juce::UnitTest
{
public:
    CrashProtectionTests()
        : juce::UnitTest("Crash protection (W10.7)", "pluginlab")
    {
    }

    void runTest() override
    {
        const juce::File folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("pluginlab_crash_test");
        folder.deleteRecursively();
        folder.createDirectory();
        setenv("PLUGINLAB_SETTINGS_FOLDER", folder.getFullPathName().toRawUTF8(), 1);
        pluginlab::host::setHelperExecutable(juce::File(PLUGINLAB_HOST_EXECUTABLE));
        beginTest("the test plugins are scanned"); // (expect() before the first beginTest() crashes the test runner)
        m_formatManager = std::make_unique<juce::AudioPluginFormatManager>();
        pluginlab::hosting::addHeadlessFormats(*m_formatManager);
        const std::optional<juce::PluginDescription> eq = scan(testpaths::getEqPlugin());
        const std::optional<juce::PluginDescription> crash = scan(testpaths::getCrashInProcessPlugin());
        if (! eq || ! crash)
        {
            m_formatManager.reset();
            return;
        }

        beginTest("the live worker: the curve of the Test EQ, rendered in a process of its own");
        {
            pluginlab::host::RemoteLiveResponse live(kSampleRate);
            expect(live.isRunning(), live.getProblem());
            live.setSlots({std::nullopt, eq});
            std::vector<float> setting = pluginlab::engine::getDefaultSetting(*m_formatManager, *eq, kSampleRate);
            setting[0] = 0.75f;
            setting[1] = 0.4f;
            setting[2] = 0.2f;
            live.setSetting(1, setting);
            expect(runUntil([&] { return live.getResponse(1).generation > 0; }), "a curve from the worker");
            const pluginlab::engine::LiveSlotResponse response = live.getResponse(1);
            const std::vector<double> exact = getTestEqCurve({0.75f, 0.4f, 0.2f}, live.getFrequencies());
            double worst = response.magnitudeDb.size() == exact.size() ? 0.0 : 1.0e9;
            for (size_t index = 0; index < exact.size() && index < response.magnitudeDb.size(); ++index)
            {
                if (exact[index] > kCompareFloorDb)
                {
                    worst = std::max(worst, std::abs(exact[index] - response.magnitudeDb[index]));
                }
            }
            expectLessThan(worst, kExactDb);
            expectEquals(live.getResponse(0).generation, 1, "the dry slot is flat at once");
        }

        beginTest("a plugin that crashes in the live worker: the worker ends, is started once more, then the live curves stop with the reason");
        {
            pluginlab::host::RemoteLiveResponse live(kSampleRate);
            live.setSlots({crash});
            live.setSetting(0, pluginlab::engine::getDefaultSetting(*m_formatManager, *crash, kSampleRate));
            expect(runUntil([&] { return ! live.isRunning(); }), "the worker crashed twice");
            expectEquals(live.getLaunches(), 2, "started once more after the first crash");
            logMessage("live worker: " + live.getProblem());
            expect(live.getProblem().contains("crashed"), live.getProblem());
            expect(live.getResponse(0).error.isNotEmpty(), "the slot tells it");
        }

        // the jobs of the pages Expert, Easy and Matcher all run through MatchJob: a job that names the crashing plugin
        const auto runJob = [this, &folder](const juce::String& option, const juce::var& job) {
            const juce::File jobFile = folder.getChildFile(option.substring(2) + "_job.json");
            const juce::File report = folder.getChildFile(option.substring(2) + "_report.md");
            jobFile.replaceWithText(juce::JSON::toString(job));
            bool finished = false;
            bool success = true;
            juce::String message;
            pluginlab::host::MatchJob jobRunner(jobFile, report, {}, [&](bool ok, const juce::String& text) {
                finished = true;
                success = ok;
                message = text;
            }, option);
            expect(runUntil([&] { return finished; }, kJobWaitMs), option + " ended");
            expect(! success, option + ": no report from a crashed process");
            logMessage(option + ": " + message);
        };

        beginTest("the page Expert's job with a crashing plugin: it fails with a message, the program goes on");
        {
            auto slot = std::make_unique<juce::DynamicObject>();
            slot->setProperty("name", "crash");
            slot->setProperty("plugin", crash->fileOrIdentifier);
            auto job = std::make_unique<juce::DynamicObject>();
            job->setProperty("slots", juce::Array<juce::var>{juce::var(slot.release())});
            job->setProperty("units", juce::Array<juce::var>{"gain"});
            job->setProperty("nullMaster", -1);
            runJob("--expert", juce::var(job.release()));
        }

        beginTest("the pages Easy and Matcher: a match against a crashing plugin fails with a message, the program goes on");
        {
            pluginlab::host::MatchJobSpec spec;
            spec.masterPlugin = eq->fileOrIdentifier;
            spec.masterValues = pluginlab::engine::getDefaultSetting(*m_formatManager, *eq, kSampleRate);
            spec.targets.push_back({crash->fileOrIdentifier, pluginlab::engine::getDefaultSetting(*m_formatManager, *crash, kSampleRate), {0}, false});
            const juce::File jobFile = folder.getChildFile("match_job.json");
            expect(pluginlab::host::writeMatchJob(spec, jobFile).isEmpty());
            runJob("--match", juce::JSON::parse(jobFile));
        }
        expect(true, "this process is still running");
        folder.deleteRecursively();
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

    template <typename Condition>
    static bool runUntil(Condition condition, int timeoutMs = kWaitMs)
    {
        const juce::uint32 started = juce::Time::getMillisecondCounter();
        while (! condition())
        {
            if (juce::Time::getMillisecondCounter() - started > static_cast<juce::uint32>(timeoutMs))
            {
                return false;
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        }
        return true;
    }

    std::unique_ptr<juce::AudioPluginFormatManager> m_formatManager;
};

static CrashProtectionTests crashProtectionTests;
