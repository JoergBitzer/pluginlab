#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HostSettings.h"
#include "MatchJob.h"
#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::host
{
// The page "Easy" (W10.4, docs/design/W10-plan.md 3.4): the musician's walkthrough in five steps on one page.
// 1. My EQ: the owned plugin, set as it sounds good (its editor). 2. Compare with: up to three other loaded plugins. 3. Make them the same: one
// button; the matcher with the defaults (the band parameters of the mappings, or every continuous knob without one; tolerance medium) in a
// process of its own, the found values applied at once. 4. Listen: switch between the plugins (and without EQ); per plugin how close it came in
// words ("the same", "audibly close", "different", with the largest difference) and, if not the same, the main cause; a level slider per plugin,
// set by the match so that switching compares sound and not loudness, and adjustable. 5. Save the matched settings as presets of the plugins.
class EasyPanel : public juce::Component, private juce::Timer
{
public:
    EasyPanel(engine::MeasurementEngine& engine, HostSettings& settings);
    ~EasyPanel() override;

    // Told by the shell
    std::function<void()> onShowPlugins;                 // "Load plugins ...": the page Plugins
    std::function<void(int slot)> onShowEditorOfSlot;    // the editor window of a loaded plugin
    std::function<void()> onPlayOrStop;                  // the application's playback (null inside a DAW: the DAW plays)
    std::function<bool()> isPlaying;
    std::function<void()> onMatchFinished;

    // The plugins were loaded or unloaded
    void slotsChanged();

    // Step 3 and step 5 (also for the tests)
    void startMatch();
    void savePresets();
    bool isMatching() const;
    // the rows in words, one line per row (the tests read it)
    juce::String describe() const;
    int getMasterSlot() const;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // "the same" / "audibly close" / "different" from the largest difference
    static juce::String describeCloseness(double largestDb);

private:
    struct Row
    {
        int slot = 0;
        bool master = false;
        juce::String name;
        juce::String released;       // where the released parameters come from
        std::optional<MatchOutcome> outcome;
        std::unique_ptr<juce::TextButton> listen;
        std::unique_ptr<juce::Label> nameLabel;
        std::unique_ptr<juce::Label> closeLabel;
        std::unique_ptr<juce::Slider> trim;
    };

    void timerCallback() override;
    void rebuildRows();
    void updateRowTexts();
    void matchFinished(bool success, const juce::String& message);
    std::vector<int> getComparedSlots() const;
    void setStatus(const juce::String& text);

    engine::MeasurementEngine& m_engine;
    HostSettings& m_settings;
    std::vector<int> m_pluginSlots;  // the master box's items
    std::vector<Row> m_rows;

    juce::Label m_title;
    juce::Label m_stepMine;
    juce::ComboBox m_masterBox;
    juce::TextButton m_editorButton{"Show its editor"};
    juce::TextButton m_loadButton{"Load plugins ..."};
    juce::Label m_stepCompare;
    juce::Label m_compareList;
    juce::Label m_stepMatch;
    juce::TextButton m_matchButton{"Make them the same"};
    juce::Rectangle<int> m_progressArea;
    juce::Label m_stepListen;
    juce::TextButton m_playButton{"Play"};
    juce::Label m_columns;
    juce::Rectangle<int> m_rowsArea;
    juce::Label m_stepSave;
    juce::TextButton m_saveButton{"Save as presets"};
    juce::Label m_status;

    std::unique_ptr<MatchJob> m_job;
    juce::File m_lastReport;
    double m_fraction = -1.0;
    juce::String m_step;
    juce::uint32 m_startedMs = 0;
};
}
