#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "pluginlab/hosting/PluginCatalog.h"
#include "pluginlab/hosting/PluginScanResult.h"
#include "pluginlab/hosting/PluginValidator.h"

namespace pluginlab::ui
{
class TextTableModel;
class PluginScanThread;
class PluginValidationThread;

// Lists the plugins of the standard folders (the list of the last scan is kept in the plugin catalog file and shown at the next start;
// the button scans again) (or of a folder the user chooses) and loads one on request:
//  - every file is scanned by its own scanner process (a crashing plugin shows up as "Crashed" in the list)
//  - before a plugin is loaded it is validated with pluginval (in a child process, result remembered); only a plugin that passed
//    can be loaded. If pluginval is not available, the user can allow loading without validation (a checkbox).
// The owner gets the plugin description in onPluginChosen and loads the plugin itself.
class PluginBrowserComponent : public juce::Component
{
public:
    // catalogFile: where the list of plugins and the validation results are kept (see PluginCatalog)
    explicit PluginBrowserComponent(const juce::File& catalogFile = hosting::PluginCatalog::getDefaultFile());
    ~PluginBrowserComponent() override;

    std::function<void(const juce::PluginDescription&)> onPluginChosen;
    std::function<void()> onScanFinished;

    void scanStandardFolders();
    void scanFolder(const juce::File& folder);

    // Selects the entry with this display name and loads it as if the user pressed Load; false if there is none.
    bool chooseByName(const juce::String& displayName);

    void setStatus(const juce::String& text);

    // The list as shown (after sorting): for the tests
    int getNumEntries() const;
    juce::String getEntryName(int index) const;
    // The same as a click on the header of this column (column ids are those of the list: 1 = Plugin, 2 = Format, ...)
    void sortBy(int columnId, bool forwards);

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
        juce::String modified;          // date of the plugin file (the newest file of a bundle)
        bool changedSinceScan = false;  // the file is not the one that was scanned: scan again
    };

    juce::String getCellText(int row, int columnId) const;
    juce::String getCellText(const Row& row, int columnId) const;
    void applySort();
    void showCatalog();
    void removeRowsOf(const juce::File& file);
    void removeRowsOfMissingFiles();
    juce::String getValidationText(const Row& row) const;
    void startScan(const juce::FileSearchPath& folders);
    void chooseFolder();
    void loadSelected();
    int getStrictnessLevel() const;
    void strictnessChanged();
    void loadRow(const Row& row);
    void showValidationOutcome(const Row& row, const hosting::ValidationResult& result);

    juce::File m_catalogFile;
    juce::File m_pluginval;
    std::vector<Row> m_rows;
    std::map<juce::String, hosting::ValidationResult> m_validation; // by plugin file path
    int m_sortColumnId = 0;                                         // 0: not sorted (order of the scan)
    bool m_sortForwards = true;
    std::vector<hosting::PluginScanResult> m_scanResults; // of the scan that is running, written to the catalog when it finishes
    bool m_scanIsOfStandardFolders = false;
    juce::String m_validatingPath;                                  // the file that is being validated now, else empty
    juce::String m_pendingLoadPath;                                 // load this file when its validation has passed

    juce::TextButton m_scanButton{"Scan / rescan plugin folders"};
    juce::TextButton m_addFolderButton{"Add folder..."};
    juce::TextButton m_loadButton{"Load"};
    juce::ToggleButton m_allowUnvalidatedButton{"Load without validation"};
    juce::Label m_strictnessLabel;
    juce::ComboBox m_strictnessBox;
    juce::Label m_statusLabel;

    std::unique_ptr<TextTableModel> m_model;
    juce::TableListBox m_table;

    std::unique_ptr<PluginScanThread> m_scanThread;
    std::unique_ptr<PluginValidationThread> m_validationThread;
    std::unique_ptr<juce::FileChooser> m_folderChooser;
};
}
