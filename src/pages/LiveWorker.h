#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/engine/LiveResponse.h"

namespace pluginlab::host
{
// Crash protection of the live transfer functions (W10.7, docs/design/W10-plan.md 3.7, option c): the measuring copies of the plugins run in a
// process of their own, PluginLabHost in the worker mode (the helper of PageServices: the application itself, or the PluginLabHost in the plugin's
// bundle). A plugin that crashes while it is measured ends that process, not the application or the DAW; the page sees the connection end and
// tells it, and starts the worker once more.

// The page's side: the same calls as engine::LiveResponseMonitor (which runs in the worker). Threads: everything from the message thread.
class RemoteLiveResponse
{
public:
    explicit RemoteLiveResponse(double sampleRate, const engine::LiveResponseOptions& options = {});
    ~RemoteLiveResponse();

    const std::vector<double>& getFrequencies() const;
    double getSampleRate() const;

    void setSlots(const std::vector<std::optional<juce::PluginDescription>>& slots);
    int getNumSlots() const;
    void setSetting(int slot, const std::vector<float>& setting);
    engine::LiveSlotResponse getResponse(int slot) const;

    // false after the worker ended twice (a plugin that crashes every time); the problem in words
    bool isRunning() const;
    juce::String getProblem() const;
    int getLaunches() const;

private:
    class Coordinator;
    void launch();
    void connectionLost();

    double m_sampleRate;
    engine::LiveResponseOptions m_options;
    std::vector<double> m_frequencies;
    std::vector<std::optional<juce::PluginDescription>> m_slots;
    std::vector<std::vector<float>> m_sent;     // the last setting sent per slot
    std::unique_ptr<Coordinator> m_coordinator;
    std::shared_ptr<std::atomic<bool>> m_alive;
    int m_launches = 0;
    bool m_running = false;
    juce::String m_problem;
};

// The worker's side, in PluginLabHost's start: true if the command line asks for the worker (the process then serves until the page is gone)
bool startLiveWorker(const juce::String& commandLine);
// At the end of the application: the worker's plugins go before JUCE shuts down
void stopLiveWorker();
}
