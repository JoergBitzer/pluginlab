#include "LiveWorker.h"

#include "PageServices.h"
#include "pluginlab/hosting/FormatManager.h"

namespace pluginlab::host
{
namespace
{
const juce::String kWorkerId = "pluginlabliveworker";
constexpr int kMaximumLaunches = 2;           // the first start and one more after a crash
constexpr int kSendIntervalMs = 50;           // the worker looks for new curves 20 times per second
const juce::String kCrashText = "the measuring process ended (a plugin crashed?)";

juce::MemoryBlock toBlock(const juce::var& message)
{
    const juce::String text = juce::JSON::toString(message, true);
    return juce::MemoryBlock(text.toRawUTF8(), text.getNumBytesAsUTF8());
}

juce::var fromBlock(const juce::MemoryBlock& block)
{
    return juce::JSON::parse(block.toString());
}

juce::var makeMessage(const juce::String& command)
{
    auto object = std::make_unique<juce::DynamicObject>();
    object->setProperty("cmd", command);
    return juce::var(object.release());
}

// what the coordinator and the page share: the latest response per slot (written by the connection's thread)
struct Responses
{
    juce::CriticalSection lock;
    std::vector<engine::LiveSlotResponse> slots;
};
}

class RemoteLiveResponse::Coordinator : public juce::ChildProcessCoordinator
{
public:
    Coordinator(std::shared_ptr<Responses> responses, std::shared_ptr<std::atomic<bool>> alive, RemoteLiveResponse& owner)
        : m_responses(std::move(responses)), m_alive(std::move(alive)), m_owner(owner)
    {
    }

    ~Coordinator() override
    {
        killWorkerProcess();
    }

    void handleMessageFromWorker(const juce::MemoryBlock& block) override
    {
        const juce::var message = fromBlock(block);
        const int slot = message.getProperty("slot", -1);
        const juce::ScopedLock lock(m_responses->lock);
        if (slot >= 0 && slot < static_cast<int>(m_responses->slots.size()))
        {
            m_responses->slots[static_cast<size_t>(slot)] = engine::liveSlotResponseFromJson(message.getProperty("response", {}));
        }
    }

    void handleConnectionLost() override
    {
        const std::shared_ptr<std::atomic<bool>> alive = m_alive;
        RemoteLiveResponse* owner = &m_owner;
        juce::MessageManager::callAsync([alive, owner] {
            if (alive->load())
            {
                owner->connectionLost();
            }
        });
    }

    const std::shared_ptr<Responses>& getResponses() const { return m_responses; }

private:
    std::shared_ptr<Responses> m_responses;
    std::shared_ptr<std::atomic<bool>> m_alive;
    RemoteLiveResponse& m_owner;
};

RemoteLiveResponse::RemoteLiveResponse(double sampleRate, const engine::LiveResponseOptions& options)
    : m_sampleRate(sampleRate), m_options(options), m_frequencies(engine::getLiveResponseFrequencies(sampleRate, options)),
      m_alive(std::make_shared<std::atomic<bool>>(true))
{
    launch();
}

RemoteLiveResponse::~RemoteLiveResponse()
{
    m_alive->store(false);
    m_coordinator.reset();
}

void RemoteLiveResponse::launch()
{
    auto responses = std::make_shared<Responses>();
    responses->slots.resize(m_slots.size());
    m_coordinator = std::make_unique<Coordinator>(responses, m_alive, *this);
    ++m_launches;
    m_running = m_coordinator->launchWorkerProcess(getHelperExecutable(), kWorkerId, 0, 0);
    if (! m_running)
    {
        m_problem = "the measuring process could not be started (" + getHelperExecutable().getFullPathName() + ")";
        return;
    }
    // the state of the page: the options, the slots, the last settings
    juce::var setup = makeMessage("setup");
    setup.getDynamicObject()->setProperty("sampleRate", m_sampleRate);
    setup.getDynamicObject()->setProperty("lowestHz", m_options.lowestHz);
    setup.getDynamicObject()->setProperty("perOctave", m_options.perOctave);
    setup.getDynamicObject()->setProperty("updatesPerSecond", m_options.updatesPerSecond);
    setup.getDynamicObject()->setProperty("classifyAfterSeconds", m_options.classifyAfterSeconds);
    m_coordinator->sendMessageToWorker(toBlock(setup));
    const std::vector<std::optional<juce::PluginDescription>> slots = m_slots;
    const std::vector<std::vector<float>> sent = m_sent;
    m_sent.clear();
    setSlots(slots);
    for (size_t slot = 0; slot < sent.size(); ++slot)
    {
        if (! sent[slot].empty())
        {
            setSetting(static_cast<int>(slot), sent[slot]);
        }
    }
}

void RemoteLiveResponse::connectionLost()
{
    {
        const juce::ScopedLock lock(m_coordinator->getResponses()->lock);
        for (engine::LiveSlotResponse& response : m_coordinator->getResponses()->slots)
        {
            response.error = kCrashText;
        }
    }
    if (m_launches < kMaximumLaunches)
    {
        launch(); // once more: the crash may have been a passing one
        return;
    }
    m_running = false;
    m_problem = kCrashText + juce::String(", twice: the live curves are switched off");
}

const std::vector<double>& RemoteLiveResponse::getFrequencies() const
{
    return m_frequencies;
}

double RemoteLiveResponse::getSampleRate() const
{
    return m_sampleRate;
}

void RemoteLiveResponse::setSlots(const std::vector<std::optional<juce::PluginDescription>>& slots)
{
    m_slots = slots;
    m_sent.assign(slots.size(), {});
    if (m_coordinator == nullptr)
    {
        return;
    }
    {
        const juce::ScopedLock lock(m_coordinator->getResponses()->lock);
        m_coordinator->getResponses()->slots.assign(slots.size(), {});
        for (size_t index = 0; index < slots.size(); ++index)
        {
            if (! slots[index])
            {
                // the dry slot: flat and LTI at once (as the monitor's dry slot)
                engine::LiveSlotResponse& dry = m_coordinator->getResponses()->slots[index];
                dry.generation = 1;
                dry.magnitudeDb.assign(m_frequencies.size(), 0.0);
                dry.classified = true;
                dry.classOfCurrentSetting = true;
            }
            else if (! m_running)
            {
                m_coordinator->getResponses()->slots[index].error = m_problem;
            }
        }
    }
    juce::var message = makeMessage("slots");
    juce::Array<juce::var> list;
    for (const std::optional<juce::PluginDescription>& slot : slots)
    {
        list.add(slot ? juce::var(slot->createXml()->toString()) : juce::var(juce::String()));
    }
    message.getDynamicObject()->setProperty("slots", list);
    m_coordinator->sendMessageToWorker(toBlock(message));
}

int RemoteLiveResponse::getNumSlots() const
{
    return static_cast<int>(m_slots.size());
}

void RemoteLiveResponse::setSetting(int slot, const std::vector<float>& setting)
{
    if (slot < 0 || slot >= getNumSlots() || m_coordinator == nullptr || ! m_running || m_sent[static_cast<size_t>(slot)] == setting)
    {
        return; // a knob that does not move sends nothing
    }
    m_sent[static_cast<size_t>(slot)] = setting;
    juce::var message = makeMessage("setting");
    message.getDynamicObject()->setProperty("slot", slot);
    juce::Array<juce::var> values;
    for (const float value : setting)
    {
        values.add(value);
    }
    message.getDynamicObject()->setProperty("values", values);
    m_coordinator->sendMessageToWorker(toBlock(message));
}

engine::LiveSlotResponse RemoteLiveResponse::getResponse(int slot) const
{
    if (m_coordinator == nullptr)
    {
        return {};
    }
    const juce::ScopedLock lock(m_coordinator->getResponses()->lock);
    const std::vector<engine::LiveSlotResponse>& slots = m_coordinator->getResponses()->slots;
    return slot >= 0 && slot < static_cast<int>(slots.size()) ? slots[static_cast<size_t>(slot)] : engine::LiveSlotResponse();
}

bool RemoteLiveResponse::isRunning() const
{
    return m_running;
}

juce::String RemoteLiveResponse::getProblem() const
{
    return m_problem;
}

int RemoteLiveResponse::getLaunches() const
{
    return m_launches;
}

// ---- the worker ----

namespace
{
class LiveWorker : public juce::ChildProcessWorker, private juce::Timer
{
public:
    LiveWorker()
    {
        hosting::addHeadlessFormats(m_formatManager);
    }

    ~LiveWorker() override
    {
        stopTimer();
        m_monitor.reset();
    }

    void handleMessageFromCoordinator(const juce::MemoryBlock& block) override
    {
        // the monitor is used from the message thread only
        const juce::var message = fromBlock(block);
        juce::MessageManager::callAsync([this, message] { handle(message); });
    }

    void handleConnectionLost() override
    {
        juce::MessageManager::callAsync([] { juce::JUCEApplicationBase::quit(); }); // the page is gone
    }

private:
    void handle(const juce::var& message)
    {
        const juce::String command = message.getProperty("cmd", "").toString();
        if (command == "setup")
        {
            engine::LiveResponseOptions options;
            options.lowestHz = message.getProperty("lowestHz", options.lowestHz);
            options.perOctave = message.getProperty("perOctave", options.perOctave);
            options.updatesPerSecond = message.getProperty("updatesPerSecond", options.updatesPerSecond);
            options.classifyAfterSeconds = message.getProperty("classifyAfterSeconds", options.classifyAfterSeconds);
            m_monitor = std::make_unique<engine::LiveResponseMonitor>(m_formatManager, message.getProperty("sampleRate", 48000.0), options);
            startTimer(kSendIntervalMs);
        }
        else if (command == "slots" && m_monitor != nullptr)
        {
            std::vector<std::optional<juce::PluginDescription>> slots;
            if (const juce::Array<juce::var>* list = message.getProperty("slots", {}).getArray())
            {
                for (const juce::var& entry : *list)
                {
                    std::optional<juce::PluginDescription> description;
                    if (const std::unique_ptr<juce::XmlElement> xml = juce::parseXML(entry.toString()))
                    {
                        juce::PluginDescription parsed;
                        if (parsed.loadFromXml(*xml))
                        {
                            description = parsed;
                        }
                    }
                    slots.push_back(description);
                }
            }
            m_monitor->setSlots(slots);
            m_lastGeneration.assign(slots.size(), -1);
            m_lastClassifications.assign(slots.size(), -1);
            m_lastError.assign(slots.size(), {});
        }
        else if (command == "setting" && m_monitor != nullptr)
        {
            std::vector<float> values;
            if (const juce::Array<juce::var>* list = message.getProperty("values", {}).getArray())
            {
                for (const juce::var& value : *list)
                {
                    values.push_back(static_cast<float>(static_cast<double>(value)));
                }
            }
            m_monitor->setSetting(message.getProperty("slot", -1), values);
        }
    }

    void timerCallback() override
    {
        if (m_monitor == nullptr)
        {
            return;
        }
        for (int slot = 0; slot < m_monitor->getNumSlots() && slot < static_cast<int>(m_lastGeneration.size()); ++slot)
        {
            const engine::LiveSlotResponse response = m_monitor->getResponse(slot);
            const auto index = static_cast<size_t>(slot);
            if (response.generation == m_lastGeneration[index] && response.classifications == m_lastClassifications[index]
                && response.error == m_lastError[index])
            {
                continue;
            }
            m_lastGeneration[index] = response.generation;
            m_lastClassifications[index] = response.classifications;
            m_lastError[index] = response.error;
            juce::var message = makeMessage("response");
            message.getDynamicObject()->setProperty("slot", slot);
            message.getDynamicObject()->setProperty("response", engine::toJson(response));
            sendMessageToCoordinator(toBlock(message));
        }
    }

    juce::AudioPluginFormatManager m_formatManager;
    std::unique_ptr<engine::LiveResponseMonitor> m_monitor;
    std::vector<int> m_lastGeneration;
    std::vector<int> m_lastClassifications;
    std::vector<juce::String> m_lastError;
};

std::unique_ptr<LiveWorker> g_worker;
}

bool startLiveWorker(const juce::String& commandLine)
{
    if (! commandLine.contains(kWorkerId))
    {
        return false;
    }
    g_worker = std::make_unique<LiveWorker>();
    if (! g_worker->initialiseFromCommandLine(commandLine, kWorkerId))
    {
        g_worker.reset();
        return false;
    }
    return true;
}

void stopLiveWorker()
{
    g_worker.reset();
}
}
