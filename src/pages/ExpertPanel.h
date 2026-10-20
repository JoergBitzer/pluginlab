#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HostSettings.h"
#include "MatchJob.h"
#include "MeasurementPresets.h"
#include "pluginlab/engine/MeasurementEngine.h"
#include "pluginlab/engine/PluginMeasurement.h"

namespace pluginlab::host
{
// The page "Expert" (W10.5, docs/design/W10-plan.md 3.4): the measurement units of W7 on the chosen slots at their current settings (the system
// class first), the null test of each slot against a master, in a process of its own (PluginLabHost --expert) with a progress bar; the results
// side by side (a column per slot) and as a report (the report window of the application, the system's viewer in the plugin). The matcher with all
// its options is the page Matcher, the live transfer functions are on the page EQ Compare: the page links to both.
class ExpertPanel : public juce::Component, public juce::TableListBoxModel, private juce::Timer
{
public:
    ExpertPanel(engine::MeasurementEngine& engine, HostSettings& settings);
    ~ExpertPanel() override;

    // Told by the shell
    std::function<void(const juce::String& title, const juce::File& report)> onShowReport;
    std::function<void()> onShowMatcher;
    std::function<void()> onShowCompare;
    std::function<void()> onMeasurementFinished;

    void slotsChanged();

    // for the shells and the tests: choose, run, read
    void setSlotChosen(int slot, bool chosen);
    void setUnitChosen(const juce::String& key, bool chosen);
    void setNullMaster(int slot);        // -1: no null test

    // measurement presets (W10.6): the choices of the page under a name; the slots by role (the plugins, the dry slot, the first plugin as master)
    MeasurementPreset makePreset(const juce::String& name, const juce::String& note) const;
    void applyPreset(const MeasurementPreset& preset);
    bool selectPreset(const juce::String& name);   // from the folder; false if there is none of that name
    juce::StringArray getChosenUnits() const;
    int getNullMaster() const;                      // -1: none
    std::vector<int> getChosenSlots() const;
    void startMeasurement();
    bool isMeasuring() const;
    const juce::File& getLastReport() const { return m_lastReport; }
    const std::vector<engine::MeasurementSummary>& getResults() const { return m_results; }
    juce::String getCellText(int row, int column) const;   // column 0: the quantity
    int getNumResultRows() const;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;

private:
    struct NullRow
    {
        juce::String name;
        juce::String text;
    };

    void timerCallback() override;
    void rebuildSlotToggles();
    void measurementFinished(bool success, const juce::String& message);
    void rebuildTable();
    void setStatus(const juce::String& text);
    void refreshPresets(const juce::String& select = {});
    void askToSavePreset();

    engine::MeasurementEngine& m_engine;
    HostSettings& m_settings;

    juce::Label m_title;
    juce::Label m_presetLabel{{}, "Preset:"};
    juce::ComboBox m_presetBox;
    juce::TextButton m_savePresetButton{"Save as preset ..."};
    juce::Label m_presetNote;
    std::vector<MeasurementPreset> m_presets;
    std::unique_ptr<juce::AlertWindow> m_saveDialog;
    juce::Label m_slotsLabel{{}, "Slots:"};
    std::vector<std::unique_ptr<juce::ToggleButton>> m_slotToggles;
    std::vector<bool> m_slotChosen;
    juce::Label m_unitsLabel{{}, "Units:"};
    juce::Label m_systemClassNote;
    std::vector<std::unique_ptr<juce::ToggleButton>> m_unitToggles;
    juce::Label m_nullLabel{{}, "Null test against:"};
    juce::ComboBox m_nullBox;
    juce::TextButton m_measureButton{"Measure"};
    juce::TextButton m_reportButton{"View report"};
    juce::TextButton m_matcherButton{"Matcher (all options)"};
    juce::TextButton m_compareButton{"Live curves (EQ Compare)"};
    juce::Rectangle<int> m_progressArea;
    juce::TableListBox m_table;
    juce::Label m_status;

    std::unique_ptr<MatchJob> m_job;
    juce::File m_lastReport;
    double m_fraction = -1.0;
    juce::String m_step;
    juce::uint32 m_startedMs = 0;
    std::vector<engine::MeasurementSummary> m_results;
    juce::StringArray m_quantities;      // the rows of the table (the union, in the order of the first appearance)
    std::vector<NullRow> m_nulls;
    juce::String m_nullMasterName;
};
}
