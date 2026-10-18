#include "pluginlab/engine/LiveResponse.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "pluginlab/engine/LivePluginDevice.h"
#include "pluginlab/match/EqDomain.h"
#include "pluginlab/measure/SystemClass.h"

namespace pluginlab::engine
{
namespace
{
constexpr int kStopTimeoutMs = 4000;
constexpr int kClassifyPollMs = 100;
constexpr double kMsPerSecond = 1000.0;
constexpr double kHighestFractionOfRate = 0.45;

double nowMs()
{
    return juce::Time::getMillisecondCounterHiRes();
}

// a thread that runs a function until it is told to exit
class LoopThread : public juce::Thread
{
public:
    LoopThread(const juce::String& name, std::function<void(LoopThread&)> body)
        : juce::Thread(name), m_body(std::move(body))
    {
    }

    ~LoopThread() override
    {
        stop();
    }

    void stop()
    {
        signalThreadShouldExit();
        notify();
        stopThread(kStopTimeoutMs);
    }

    void run() override
    {
        m_body(*this);
    }

private:
    std::function<void(LoopThread&)> m_body;
};
}

struct LiveResponseMonitor::Shared
{
    std::unique_ptr<match::MagnitudeProbe> probe;
};

// The two measuring instances of a slot with their threads: one renders the curves, one checks the system class
class LiveResponseMonitor::SlotWorker
{
public:
    SlotWorker(juce::AudioPluginFormatManager& formatManager, const std::optional<juce::PluginDescription>& description, double sampleRate,
               const LiveResponseOptions& options, std::shared_ptr<Shared> shared)
        : m_options(options), m_sampleRate(sampleRate), m_shared(std::move(shared))
    {
        if (! description)
        {
            // the dry slot: flat, LTI, nothing to render
            m_response.generation = 1;
            m_response.magnitudeDb.assign(m_shared->probe->getFrequencies().size(), 0.0);
            m_response.classified = true;
            m_response.classOfCurrentSetting = true;
            m_response.systemClass = measure::getSystemClassName(measure::SystemClass::Lti);
            return;
        }
        m_renderTarget = std::make_unique<LivePluginTarget>(formatManager, *description, sampleRate);
        m_classifyTarget = std::make_unique<LivePluginTarget>(formatManager, *description, sampleRate);
        if (! m_renderTarget->isValid() || ! m_classifyTarget->isValid())
        {
            m_response.error = m_renderTarget->isValid() ? m_classifyTarget->getError() : m_renderTarget->getError();
            return;
        }
        m_renderThread = std::make_unique<LoopThread>("LiveResponse " + description->name, [this](LoopThread& thread) { renderLoop(thread); });
        m_classifyThread = std::make_unique<LoopThread>("LiveClass " + description->name, [this](LoopThread& thread) { classifyLoop(thread); });
        // below the audio thread of the engine (the DAW's or the device's): the display must give way when the audio is busy
        m_renderThread->startThread(juce::Thread::Priority::low);
        m_classifyThread->startThread(juce::Thread::Priority::background);
    }

    ~SlotWorker()
    {
        if (m_renderThread != nullptr)
        {
            m_renderThread->stop();
        }
        if (m_classifyThread != nullptr)
        {
            m_classifyThread->stop();
        }
    }

    void setSetting(const std::vector<float>& setting)
    {
        if (m_renderThread == nullptr)
        {
            return;
        }
        const juce::ScopedLock lock(m_lock);
        if (setting == m_setting)
        {
            return;
        }
        m_setting = setting;
        m_changed = true;
        m_lastChangeMs = nowMs();
        m_renderThread->notify();
    }

    LiveSlotResponse getResponse() const
    {
        const juce::ScopedLock lock(m_lock);
        return m_response;
    }

private:
    void renderLoop(LoopThread& thread)
    {
        double nextAllowedMs = 0.0;
        while (! thread.threadShouldExit())
        {
            bool changed = false;
            {
                const juce::ScopedLock lock(m_lock);
                changed = m_changed;
            }
            if (! changed)
            {
                thread.wait(-1);
                continue;
            }
            // at most updatesPerSecond renders: the settings that come in meanwhile are collected, the latest is rendered
            const double waitMs = nextAllowedMs - nowMs();
            if (waitMs > 0.0)
            {
                thread.wait(static_cast<int>(std::ceil(waitMs)));
                continue;
            }
            std::vector<float> setting;
            {
                const juce::ScopedLock lock(m_lock);
                setting = m_setting;
                m_changed = false;
            }
            const double startMs = nowMs();
            nextAllowedMs = startMs + kMsPerSecond / m_options.updatesPerSecond;
            std::vector<double> magnitude = m_shared->probe->measure(m_renderTarget->makeDevice(setting));
            const double seconds = (nowMs() - startMs) / kMsPerSecond;
            const juce::ScopedLock lock(m_lock);
            m_response.magnitudeDb = std::move(magnitude);
            m_response.renderSeconds = seconds;
            m_response.classOfCurrentSetting = m_classifiedSetting == setting;
            m_renderedSetting = setting;
            ++m_response.generation;
        }
    }

    void classifyLoop(LoopThread& thread)
    {
        measure::SystemClassSettings settings;
        settings.sampleRate = m_sampleRate;
        while (! thread.threadShouldExit())
        {
            thread.wait(kClassifyPollMs);
            std::vector<float> setting;
            {
                const juce::ScopedLock lock(m_lock);
                if (m_setting.empty() || m_setting == m_classifiedSetting
                    || nowMs() - m_lastChangeMs < m_options.classifyAfterSeconds * kMsPerSecond)
                {
                    continue;
                }
                setting = m_setting;
            }
            const measure::SystemClassResult result = measure::classifySystem(m_classifyTarget->makeDevice(setting), settings);
            const juce::ScopedLock lock(m_lock);
            m_classifiedSetting = setting;
            m_response.classified = true;
            m_response.lti = result.systemClass == measure::SystemClass::Lti;
            m_response.systemClass = measure::getSystemClassName(result.systemClass);
            m_response.reason = m_response.lti ? juce::String() : result.reason;
            m_response.classOfCurrentSetting = m_renderedSetting == setting;
            ++m_response.classifications;
        }
    }

    LiveResponseOptions m_options;
    double m_sampleRate;
    std::shared_ptr<Shared> m_shared;
    std::unique_ptr<LivePluginTarget> m_renderTarget;
    std::unique_ptr<LivePluginTarget> m_classifyTarget;
    juce::CriticalSection m_lock;
    std::vector<float> m_setting;            // the latest from the playing instance
    std::vector<float> m_renderedSetting;    // of the last curve
    std::vector<float> m_classifiedSetting;  // of the last system class
    bool m_changed = false;
    double m_lastChangeMs = 0.0;
    LiveSlotResponse m_response;
    std::unique_ptr<LoopThread> m_renderThread; // last: stopped before the members it uses go
    std::unique_ptr<LoopThread> m_classifyThread;
};

LiveResponseMonitor::LiveResponseMonitor(juce::AudioPluginFormatManager& formatManager, double sampleRate, const LiveResponseOptions& options)
    : m_formatManager(formatManager), m_sampleRate(sampleRate), m_options(options), m_shared(std::make_shared<Shared>())
{
    match::MagnitudeProbeSettings settings;
    settings.stimulus = match::ProbeStimulus::Impulse;
    settings.sampleRate = sampleRate;
    settings.lowestHz = options.lowestHz;
    settings.highestHz = std::min(options.highestHz, kHighestFractionOfRate * sampleRate);
    settings.perOctave = options.perOctave;
    m_shared->probe = std::make_unique<match::MagnitudeProbe>(settings);
}

LiveResponseMonitor::~LiveResponseMonitor()
{
    m_slots.clear(); // the threads end before the probe goes
}

const std::vector<double>& LiveResponseMonitor::getFrequencies() const
{
    return m_shared->probe->getFrequencies();
}

double LiveResponseMonitor::getSampleRate() const
{
    return m_sampleRate;
}

const LiveResponseOptions& LiveResponseMonitor::getOptions() const
{
    return m_options;
}

void LiveResponseMonitor::setSlots(const std::vector<std::optional<juce::PluginDescription>>& slots)
{
    std::vector<std::unique_ptr<SlotWorker>> workers(slots.size());
    std::vector<juce::String> identifiers(slots.size());
    for (size_t index = 0; index < slots.size(); ++index)
    {
        identifiers[index] = slots[index] ? slots[index]->createIdentifierString() : juce::String("dry");
        if (index < m_slots.size() && m_identifiers[index] == identifiers[index])
        {
            workers[index] = std::move(m_slots[index]); // the same plugin: its instances stay
        }
    }
    m_slots.clear(); // the instances of removed or replaced plugins go before new ones are loaded
    for (size_t index = 0; index < slots.size(); ++index)
    {
        if (workers[index] == nullptr)
        {
            workers[index] = std::make_unique<SlotWorker>(m_formatManager, slots[index], m_sampleRate, m_options, m_shared);
        }
    }
    m_slots = std::move(workers);
    m_identifiers = std::move(identifiers);
}

int LiveResponseMonitor::getNumSlots() const
{
    return static_cast<int>(m_slots.size());
}

void LiveResponseMonitor::setSetting(int slot, const std::vector<float>& setting)
{
    if (slot >= 0 && slot < getNumSlots())
    {
        m_slots[static_cast<size_t>(slot)]->setSetting(setting);
    }
}

LiveSlotResponse LiveResponseMonitor::getResponse(int slot) const
{
    if (slot < 0 || slot >= getNumSlots())
    {
        return {};
    }
    return m_slots[static_cast<size_t>(slot)]->getResponse();
}

LiveDeviation getLiveDeviation(const std::vector<double>& masterDb, const std::vector<double>& slotDb, double ignoreBelowDb)
{
    LiveDeviation deviation;
    double sum = 0.0;
    int count = 0;
    for (size_t index = 0; index < masterDb.size() && index < slotDb.size(); ++index)
    {
        if (masterDb[index] < ignoreBelowDb && slotDb[index] < ignoreBelowDb)
        {
            continue;
        }
        const double difference = slotDb[index] - masterDb[index];
        deviation.largestDb = std::max(deviation.largestDb, std::abs(difference));
        sum += difference * difference;
        ++count;
    }
    deviation.rmsDb = count > 0 ? std::sqrt(sum / count) : 0.0;
    return deviation;
}
}
