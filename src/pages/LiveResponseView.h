#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "LiveWorker.h"
#include "pluginlab/engine/LiveResponse.h"
#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::host
{
// The real-time transfer function display on the page Compare (W10.3, docs/design/W10-plan.md 3.3): the magnitude of every slot in its
// colour on one log axis while the knobs of the playing plugins are turned (their editors or the parameter table), the reference slot
// thicker, the deviation of each slot from the reference below and in numbers. A slot that is not LTI at its setting is drawn grey with the
// reason. The measuring copies run in a process of their own (RemoteLiveResponse, W10.7); the audio is not touched.
class LiveResponseView : public juce::Component, private juce::Timer
{
public:
    explicit LiveResponseView(engine::MeasurementEngine& engine);
    ~LiveResponseView() override;

    // The list of the slots changed (or the engine's rate): the measuring instances follow
    void slotsChanged();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // for the tests: the monitor (null while switched off) and the text of the curves' legend
    const RemoteLiveResponse* getMonitor() const { return m_monitor.get(); }
    juce::String describe() const;

private:
    void timerCallback() override;
    void setLive(bool enabled);
    void rebuildPlot();
    int getReferenceSlot() const;
    bool isVisibleWithParents() const;

    engine::MeasurementEngine& m_engine;
    std::unique_ptr<RemoteLiveResponse> m_monitor; // the measuring copies in a process of their own (W10.7)
    std::vector<int> m_generations;
    std::vector<int> m_classifications;

    juce::ToggleButton m_enabled{"Live response"};
    juce::Label m_referenceLabel;
    juce::ComboBox m_reference;
    juce::Label m_info;
    juce::Rectangle<int> m_plotArea;
    juce::Image m_plot;
    juce::String m_description;
};
}
