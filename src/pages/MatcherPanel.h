#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HostSettings.h"
#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::host
{
class MatchJob;
class LiveDeviationThread;

// The page "Matcher" (W9.6, docs/matching/matcher-page.md): the master (a loaded plugin as it is set, or a band curve), the targets (the other
// loaded plugins) with the parameters the user releases, the tolerance, the frequency range, the time per target and the model start; the live
// deviation of the selected target while its knobs are turned (copies of the plugins, no audio); "Match" runs PluginLabHost --match in a process
// of its own (a crashing plugin does not take the host down) with a progress bar; the report; "Apply" sets the found values on the loaded
// targets, "Save as presets" writes their state; "Listen" goes to the page Compare (gapless switching).
class MatcherPanel : public juce::Component, public juce::TableListBoxModel, private juce::Timer
{
public:
    MatcherPanel(pluginlab::engine::MeasurementEngine& engine, HostSettings& settings);
    ~MatcherPanel() override;

    // Told by the shell
    std::function<void()> onListen;                                                      // go to the page Compare
    std::function<void(const juce::String& title, const juce::File& report)> onShowReport; // a report window
    std::function<void()> onMatchFinished;

    // The plugins were loaded or unloaded: the master choice, the targets and the live view follow
    void slotsChanged();

    // The first loaded plugin as master, all others as targets (the command line of the tests, --matcher)
    void chooseFirstAsMaster();
    void startMatch();
    void showLastReport();
    void applyResults();

    void resized() override;
    void paint(juce::Graphics& g) override;

    // juce::TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;

    // called by the job (message thread)
    void jobProgress(double fraction, const juce::String& step);
    void jobFinished(bool success, const juce::String& message);
    // called by the live view (message thread)
    void liveUpdate(const std::vector<double>& frequencies, const std::vector<double>& master, const std::vector<double>& target);

private:
    struct TargetRow
    {
        int slot = 0;
        juce::String key;
        bool match = true;
        std::vector<int> released;   // parameter indices
        juce::String source;         // where the released parameters came from ("mapping", "chosen")
    };

    class Plot : public juce::Component
    {
    public:
        void setCurves(std::vector<double> frequencies, std::vector<double> master, std::vector<double> target, juce::String caption);
        void paint(juce::Graphics& g) override;

    private:
        std::vector<double> m_frequencies;
        std::vector<double> m_master;
        std::vector<double> m_target;
        juce::String m_caption;
    };

    void timerCallback() override;
    void rebuildTargets();
    void rebuildLive();
    void chooseParameters(int row);
    int getMasterSlot() const;       // -1: the band curve
    juce::String writeJob(const juce::File& jobFile) const;
    void savePresets();
    void updateButtons();

    pluginlab::engine::MeasurementEngine& m_engine;
    HostSettings& m_settings;
    juce::AudioPluginFormatManager m_formatManager;
    std::vector<TargetRow> m_targets;
    std::vector<int> m_masterSlots;  // the combo box items after the band curve

    juce::Label m_masterLabel{{}, "Master:"};
    juce::ComboBox m_masterBox;
    juce::ComboBox m_curveType;
    juce::Slider m_curveFrequency;
    juce::Slider m_curveGain;
    juce::Slider m_curveQ;
    juce::Label m_toleranceLabel{{}, "Tolerance:"};
    juce::ComboBox m_toleranceBox;
    juce::Label m_rangeLabel{{}, "Range (Hz):"};
    juce::Slider m_lowest;
    juce::Slider m_highest;
    juce::Label m_secondsLabel{{}, "Seconds per target:"};
    juce::Slider m_seconds;
    juce::ToggleButton m_modelStart{"Start from the model (needs a mapping)"};
    juce::TableListBox m_table;
    Plot m_plot;
    juce::TextButton m_matchButton{"Match"};
    juce::TextButton m_reportButton{"View report"};
    juce::TextButton m_applyButton{"Apply to targets"};
    juce::TextButton m_saveButton{"Save as presets"};
    juce::TextButton m_listenButton{"Listen (Compare)"};
    juce::Label m_status;
    juce::Rectangle<int> m_progressArea;

    std::unique_ptr<MatchJob> m_job;
    std::unique_ptr<LiveDeviationThread> m_live;
    juce::File m_lastReport;
    double m_fraction = -1.0;
    juce::String m_step;
    juce::uint32 m_startedMs = 0;
};
}
