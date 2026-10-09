#pragma once

#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HostSettings.h"
#include "pluginlab/engine/Fingerprint.h"
#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::host
{
class PanelJob;
class ReportWindow;

// The third page of the host: the technical tests of the plugins. Lists the plugins that are loaded on the Plugins page; for each one a
// report (the fingerprint of docs/design/W5-fingerprint.md) can be generated and, when it is ready, viewed. The measurement runs in a
// process of its own (PluginLabHost --fingerprint), so a plugin that crashes during the tests does not take the host down: the row then
// says that no report could be made. The tests are technical only and do not depend on the kind of algorithm. The GUI review (W5d,
// docs/design/W5d-gui-review.md: captures and vision variants of the editor, scale factor, robustness, load) runs the same way
// (PluginLabHost --gui-snapshot); a crash names the step it happened in.
class DeveloperPanel : public juce::Component, public juce::TableListBoxModel
{
public:
    DeveloperPanel(pluginlab::engine::MeasurementEngine& engine, HostSettings& settings);
    ~DeveloperPanel() override;

    // what a child process makes
    enum class JobKind
    {
        Report,
        GuiReview
    };

    // Told when a report / a GUI review is ready (the row)
    std::function<void(int row)> onReportReady;
    std::function<void(int row)> onGuiReviewReady;

    // The list follows the plugins of the engine (call after plugins were loaded or unloaded)
    void refresh();

    void resized() override;

    // juce::TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    juce::String getCellTooltip(int rowNumber, int columnId) override;

    // called by the job (message thread); message: why it failed (the step of a crash)
    void jobFinished(const juce::String& key, JobKind kind, bool success, const juce::String& message);

    // what the buttons of a row do
    void generateReport(int row);
    void viewReport(int row);
    void generateGuiReview(int row);
    void viewGuiReview(int row);
    juce::String getStatusText(int row) const;
    juce::String getGuiStatusText(int row) const;
    bool isReportReady(int row) const;
    bool isGuiReviewReady(int row) const;

private:
    enum class State
    {
        NoReport,
        Waiting,
        Running,
        Ready,
        Failed
    };

    struct Row
    {
        juce::File pluginFile;
        juce::PluginDescription description;
        juce::String key; // the identifier string: unique per plugin
        juce::File reportFile;
        State state = State::NoReport;
        juce::Time reportTime;
        bool outdated = false; // the plugin is newer than the report
        std::vector<pluginlab::engine::SummaryItem> summary; // the single results of the report (from <report>.json)
        juce::File guiFolder;                                // the GUI review: a folder with gui_review.md and the images
        State guiState = State::NoReport;
        juce::String guiMessage;                             // why the last GUI review failed
    };

    const pluginlab::engine::SummaryItem* findSummaryItem(const Row& row, int columnId) const;

    void updateRowFromDisk(Row& row) const;
    Row* findRow(const juce::String& key);
    void startNextJob();
    void queueJob(int row, JobKind kind);
    juce::File makeReportFile(const juce::PluginDescription& description) const;

    pluginlab::engine::MeasurementEngine& m_engine;
    HostSettings& m_settings;
    std::vector<Row> m_rows;
    std::deque<std::pair<juce::String, JobKind>> m_queue; // the rows (keys) that wait for their turn, and for what
    std::unique_ptr<PanelJob> m_job;
    std::map<juce::String, std::unique_ptr<juce::DocumentWindow>> m_windows; // report windows by key, GUI review windows by key + "#gui"

    juce::TableListBox m_table;
    juce::Label m_infoLabel;

    friend class ReportWindow;
    friend class GuiReviewWindow;
    void closeWindow(const juce::String& key);
};
}
