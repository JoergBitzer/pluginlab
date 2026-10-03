#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "pluginlab/hosting/PluginScanResult.h"
#include "pluginlab/hosting/PluginValidator.h"

namespace pluginlab::ui
{
class TextTableModel;
class PluginScanThread;
class PluginValidationThread;

// Lists the plugins of the standard folders (or of a folder the user chooses) and loads one on request:
//  - every file is scanned by its own scanner process (a crashing plugin shows up as "Crashed" in the list)
//  - before a plugin is loaded it is validated with pluginval (in a child process, result remembered); only a plugin that passed
//    can be loaded. If pluginval is not available, the user can allow loading without validation (a checkbox).
// The owner gets the plugin description in onPluginChosen and loads the plugin itself.
class PluginBrowserComponent : public juce::Component
{
public:
    PluginBrowserComponent();
    ~PluginBrowserComponent() override;

    std::function<void(const juce::PluginDescription&)> onPluginChosen;
    std::function<void()> onScanFinished;

    void scanStandardFolders();
    void scanFolder(const juce::File& folder);

    // Selects the entry with this display name and loads it as if the user pressed Load; false if there is none.
    bool chooseByName(const juce::String& displayName);

    void setStatus(const juce::String& text);

    void resized() override;

    // called by the background threads (on the message thread)
    void addScanResult(const hosting::PluginScanResult& result);
    void scanFinished();
    void validationFinished(const juce::File& pluginFile, const hosting::ValidationResult& result);

private:
    struct Row
    {
        juce::File file;
        hosting::ScanStatus scanStatus = hosting::ScanStatus::ScannerFailed;
        juce::String message;
        juce::PluginDescription description;
        bool hasDescription = false;
    };

    juce::String getCellText(int row, int columnId) const;
    juce::String getValidationText(const Row& row) const;
    void startScan(const juce::FileSearchPath& folders);
    void chooseFolder();
    void loadSelected();
    void loadRow(const Row& row);
    void showValidationOutcome(const Row& row, const hosting::ValidationResult& result);

    juce::File m_pluginval;
    std::vector<Row> m_rows;
    std::map<juce::String, hosting::ValidationResult> m_validation; // by plugin file path
    juce::String m_validatingPath;                                  // the file that is being validated now, else empty
    juce::String m_pendingLoadPath;                                 // load this file when its validation has passed

    juce::TextButton m_scanButton{"Scan standard folders"};
    juce::TextButton m_addFolderButton{"Add folder..."};
    juce::TextButton m_loadButton{"Load"};
    juce::ToggleButton m_allowUnvalidatedButton{"Load without validation"};
    juce::Label m_statusLabel;

    std::unique_ptr<TextTableModel> m_model;
    juce::TableListBox m_table;

    std::unique_ptr<PluginScanThread> m_scanThread;
    std::unique_ptr<PluginValidationThread> m_validationThread;
    std::unique_ptr<juce::FileChooser> m_folderChooser;
};
}
