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
class FingerprintJob;
class ReportWindow;

// The third page of the host: the technical tests of the plugins. Lists the plugins that are loaded on the Plugins page; for each one a
// report (the fingerprint of docs/design/W5-fingerprint.md) can be generated and, when it is ready, viewed. The measurement runs in a
// process of its own (PluginLabHost --fingerprint), so a plugin that crashes during the tests does not take the host down: the row then
// says that no report could be made. The tests are technical only and do not depend on the kind of algorithm.
class DeveloperPanel : public juce::Component, public juce::TableListBoxModel
{
public:
    DeveloperPanel(pluginlab::engine::MeasurementEngine& engine, HostSettings& settings);
    ~DeveloperPanel() override;

    // Told when a report is ready (the row)
    std::function<void(int row)> onReportReady;

    // The list follows the plugins of the engine (call after plugins were loaded or unloaded)
    void refresh();

    void resized() override;

    // juce::TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    juce::String getCellTooltip(int rowNumber, int columnId) override;

    // called by the job (message thread)
    void jobFinished(const juce::String& key, bool success);

    // what the buttons of a row do
    void generateReport(int row);
    void viewReport(int row);
    juce::String getStatusText(int row) const;
    bool isReportReady(int row) const;

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
    };

    const pluginlab::engine::SummaryItem* findSummaryItem(const Row& row, int columnId) const;

    void updateRowFromDisk(Row& row) const;
    Row* findRow(const juce::String& key);
    void startNextJob();
    juce::File makeReportFile(const juce::PluginDescription& description) const;

    pluginlab::engine::MeasurementEngine& m_engine;
    HostSettings& m_settings;
    std::vector<Row> m_rows;
    std::deque<juce::String> m_queue; // keys of the rows that wait for their turn
    std::unique_ptr<FingerprintJob> m_job;
    std::map<juce::String, std::unique_ptr<ReportWindow>> m_windows;

    juce::TableListBox m_table;
    juce::Label m_infoLabel;

    friend class ReportWindow;
    void closeWindow(const juce::String& key);
};
}
