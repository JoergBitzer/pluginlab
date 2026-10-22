#include "MatchJob.h"

#include <algorithm>

#include "PageServices.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/OfflineRenderer.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginDisplayName.h"

namespace pluginlab::host
{
namespace
{
constexpr int kPollMs = 200;
constexpr juce::uint32 kProgressIntervalMs = 1000;
constexpr int kStopTimeoutMs = 10000;
constexpr int kProcessTimeoutMs = 10 * 60 * 1000;
constexpr int kContinuousSteps = 0x7fffffff; // JUCE's getNumSteps() of a continuous parameter

juce::Array<juce::var> toArray(const std::vector<float>& values)
{
    juce::Array<juce::var> array;
    for (const float value : values)
    {
        array.add(value);
    }
    return array;
}
}

juce::String writeMatchJob(const MatchJobSpec& spec, const juce::File& jobFile)
{
    if (spec.targets.empty())
    {
        return "no target chosen";
    }
    auto root = std::make_unique<juce::DynamicObject>();
    auto master = std::make_unique<juce::DynamicObject>();
    if (spec.masterPlugin.isNotEmpty())
    {
        master->setProperty("plugin", spec.masterPlugin);
        master->setProperty("identifier", spec.masterIdentifier);
        master->setProperty("values", toArray(spec.masterValues));
    }
    else
    {
        master->setProperty("curve", spec.masterCurve);
    }
    root->setProperty("master", juce::var(master.release()));
    juce::Array<juce::var> targets;
    for (const MatchJobTarget& entry : spec.targets)
    {
        auto target = std::make_unique<juce::DynamicObject>();
        target->setProperty("plugin", entry.plugin);
        target->setProperty("identifier", entry.identifier);
        target->setProperty("values", toArray(entry.values));
        juce::Array<juce::var> released;
        for (const int index : entry.released)
        {
            released.add(index);
        }
        target->setProperty("releaseIndices", released);
        target->setProperty("modelStart", entry.modelStart);
        targets.add(juce::var(target.release()));
    }
    root->setProperty("targets", targets);
    root->setProperty("tolerance", spec.tolerance);
    root->setProperty("seconds", spec.seconds);
    root->setProperty("lowestHz", spec.lowestHz);
    root->setProperty("highestHz", spec.highestHz);
    jobFile.getParentDirectory().createDirectory();
    if (! jobFile.replaceWithText(juce::JSON::toString(juce::var(root.release()))))
    {
        return "cannot write " + jobFile.getFullPathName();
    }
    return {};
}

std::vector<MatchOutcome> readMatchResult(const juce::File& reportFile)
{
    std::vector<MatchOutcome> outcomes;
    const juce::var result = juce::JSON::parse(juce::File(reportFile.getFullPathName() + ".json").loadFileAsString());
    const juce::Array<juce::var>* targets = result.getProperty("targets", {}).getArray();
    if (targets == nullptr)
    {
        return outcomes;
    }
    for (const juce::var& entry : *targets)
    {
        MatchOutcome outcome;
        outcome.plugin = entry.getProperty("plugin", "").toString();
        outcome.name = entry.getProperty("name", "").toString();
        outcome.refused = static_cast<bool>(entry.getProperty("refused", false));
        outcome.reason = entry.getProperty("reason", "").toString();
        outcome.matched = static_cast<bool>(entry.getProperty("matched", false));
        outcome.largestDb = entry.getProperty("largestDb", 0.0);
        outcome.worstHz = entry.getProperty("worstHz", 0.0);
        outcome.cause = entry.getProperty("cause", "").toString();
        outcome.evidence = entry.getProperty("evidence", "").toString();
        outcome.levelDb = entry.getProperty("levelDb", 0.0);
        if (const juce::Array<juce::var>* setting = entry.getProperty("setting", {}).getArray())
        {
            for (const juce::var& value : *setting)
            {
                outcome.setting.push_back(static_cast<float>(static_cast<double>(value)));
            }
        }
        if (const juce::Array<juce::var>* released = entry.getProperty("released", {}).getArray())
        {
            for (const juce::var& index : *released)
            {
                outcome.released.push_back(static_cast<int>(index));
            }
        }
        outcomes.push_back(outcome);
    }
    return outcomes;
}

int applyMatchOutcomes(engine::MeasurementEngine& engine, const std::vector<MatchOutcome>& outcomes, const std::vector<int>& slots)
{
    int applied = 0;
    for (const MatchOutcome& outcome : outcomes)
    {
        if (outcome.refused)
        {
            continue;
        }
        for (const int slot : slots)
        {
            hosting::HostedPlugin* plugin = engine.getPlugin(slot);
            if (plugin == nullptr || plugin->getDescription().createIdentifierString() != outcome.plugin)
            {
                continue;
            }
            for (const int parameter : outcome.released)
            {
                if (parameter >= 0 && parameter < static_cast<int>(outcome.setting.size()))
                {
                    plugin->setParameterNormalised(parameter, outcome.setting[static_cast<size_t>(parameter)]);
                }
            }
            ++applied;
            break;
        }
    }
    return applied;
}

std::vector<int> getMappedParameters(const HostSettings& settings, const juce::PluginDescription& description)
{
    std::vector<int> released;
    juce::String error;
    const juce::File file = settings.getMappingFolder().getChildFile(engine::getMappingFileName(description.name));
    if (const std::optional<engine::Mapping> mapping = engine::Mapping::read(file, error))
    {
        for (const engine::MappedParameter& parameter : mapping->parameters)
        {
            if (parameter.quantity == engine::MappedQuantity::Frequency || parameter.quantity == engine::MappedQuantity::Gain
                || parameter.quantity == engine::MappedQuantity::Q)
            {
                released.push_back(parameter.index);
            }
        }
    }
    return released;
}

std::vector<int> getContinuousParameters(engine::MeasurementEngine& engine, int slot)
{
    std::vector<int> released;
    hosting::HostedPlugin* plugin = engine.getPlugin(slot);
    if (plugin == nullptr)
    {
        return released;
    }
    const juce::Array<juce::AudioProcessorParameter*>& parameters = plugin->getInstance().getParameters();
    for (int index = 0; index < parameters.size(); ++index)
    {
        const juce::AudioProcessorParameter& parameter = *parameters[index];
        const bool continuous = parameter.getNumSteps() == kContinuousSteps || parameter.getNumSteps() > 100;
        if (continuous && parameter.isAutomatable() && ! parameter.getName(64).containsIgnoreCase("bypass"))
        {
            released.push_back(index);
        }
    }
    return released;
}

int saveMatchedPresets(engine::MeasurementEngine& engine, const HostSettings& settings, const std::vector<int>& slots, const juce::String& masterName)
{
    int saved = 0;
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M");
    for (const int slot : slots)
    {
        hosting::HostedPlugin* plugin = engine.getPlugin(slot);
        if (plugin == nullptr)
        {
            continue;
        }
        juce::MemoryBlock state;
        plugin->getInstance().getStateInformation(state);
        const juce::String name = engine::makeFileNamePart(hosting::getDisplayName(plugin->getDescription()));
        const juce::File folder = settings.getPresetFolder().getChildFile(name);
        folder.createDirectory();
        const juce::String stem = "matched to " + engine::makeFileNamePart(masterName) + " " + stamp;
        if (folder.getChildFile(stem + ".state").replaceWithData(state.getData(), state.getSize()))
        {
            // what the state is, for people and for a later "load preset"
            auto info = std::make_unique<juce::DynamicObject>();
            info->setProperty("plugin", plugin->getDescription().createIdentifierString());
            info->setProperty("master", masterName);
            info->setProperty("date", stamp);
            info->setProperty("values", toArray(engine::getSetting(plugin->getInstance())));
            folder.getChildFile(stem + ".json").replaceWithText(juce::JSON::toString(juce::var(info.release())));
            ++saved;
        }
    }
    return saved;
}

class MatchJob::Runner : public juce::Thread
{
public:
    Runner(juce::File job, juce::File report, Progress progress, Finished finished, std::shared_ptr<std::atomic<bool>> alive, juce::String option)
        : juce::Thread("MatchJob"), m_job(std::move(job)), m_report(std::move(report)), m_progress(std::move(progress)), m_finished(std::move(finished)),
          m_alive(std::move(alive)), m_option(std::move(option))
    {
    }

    ~Runner() override
    {
        signalThreadShouldExit();
        stopThread(kStopTimeoutMs);
    }

    void run() override
    {
        juce::ChildProcess process;
        const juce::File host = getHelperExecutable(); // the application, or the PluginLabHost in the plugin's bundle
        bool success = false;
        juce::String message;
        if (process.start(juce::StringArray{host.getFullPathName(), m_option, m_job.getFullPathName(), m_report.getFullPathName()}, 0))
        {
            const juce::uint32 started = juce::Time::getMillisecondCounter();
            juce::uint32 lastProgress = 0;
            while (process.isRunning() && ! threadShouldExit() && juce::Time::getMillisecondCounter() - started < static_cast<juce::uint32>(kProcessTimeoutMs))
            {
                if (juce::Time::getMillisecondCounter() - lastProgress >= kProgressIntervalMs)
                {
                    lastProgress = juce::Time::getMillisecondCounter();
                    const JobProgress progress = readProgressFile(getProgressFile(m_report));
                    post([callback = m_progress, progress] {
                        if (callback)
                        {
                            callback(progress.fraction, progress.step);
                        }
                    });
                }
                wait(kPollMs);
            }
            if (process.isRunning())
            {
                process.kill();
                if (threadShouldExit())
                {
                    message = "stopped";
                }
                else
                {
                    message = "stopped (time limit)";
                }
            }
            success = m_report.existsAsFile() && m_report.getSize() > 0;
            if (! success && message.isEmpty())
            {
                message = "the process ended without a report (a crash?)";
            }
        }
        else
        {
            message = "could not start the process (" + host.getFullPathName() + ")";
        }
        // a copy of the callback: it may destroy the job (and this thread object) while it runs
        post([callback = m_finished, success, message] {
            if (callback)
            {
                callback(success, message);
            }
        });
    }

private:
    // on the message thread, and only while the job exists (its owner may be gone)
    void post(std::function<void()> call)
    {
        const std::shared_ptr<std::atomic<bool>> alive = m_alive;
        juce::MessageManager::callAsync([alive, call] {
            if (alive->load())
            {
                call();
            }
        });
    }

    juce::File m_job;
    juce::File m_report;
    Progress m_progress;
    Finished m_finished;
    std::shared_ptr<std::atomic<bool>> m_alive;
    juce::String m_option;
};

MatchJob::MatchJob(juce::File job, juce::File report, Progress progress, Finished finished, juce::String option)
    : m_report(report), m_alive(std::make_shared<std::atomic<bool>>(true))
{
    m_runner = std::make_unique<Runner>(std::move(job), std::move(report), std::move(progress), std::move(finished), m_alive, std::move(option));
    m_runner->startThread();
}

MatchJob::~MatchJob()
{
    m_alive->store(false); // calls already posted do nothing
    m_runner.reset();
}
}
