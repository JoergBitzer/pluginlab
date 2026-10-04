#include "pluginlab/ui/PluginBrowserComponent.h"

#include <algorithm>

#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/ui/TextTableModel.h"

namespace pluginlab::ui
{
namespace
{
constexpr int kMargin = 8;
constexpr int kButtonHeight = 28;
constexpr int kStatusHeight = 24;
constexpr int kRowHeight = 24;
constexpr int kHeaderHeight = 24;
constexpr int kScanButtonWidth = 210;
constexpr int kAddFolderButtonWidth = 110;
constexpr int kLoadButtonWidth = 80;
constexpr int kAllowUnvalidatedWidth = 190;
constexpr int kStrictnessLabelWidth = 110;
constexpr int kStrictnessBoxWidth = 60;
constexpr int kLowestStrictness = 1;
constexpr int kHighestStrictness = 10;
constexpr int kThreadStopMs = hosting::PluginScanner::kDefaultTimeoutMs + 5000;

constexpr int kColumnName = 1;
constexpr int kColumnFormat = 2;
constexpr int kColumnManufacturer = 3;
constexpr int kColumnScan = 4;
constexpr int kColumnValidation = 5;
constexpr int kColumnModified = 6;
constexpr int kColumnFile = 7;
constexpr int kColumnType = 8; // (the id only; the column sits after Format)
constexpr int kWidthName = 200;
constexpr int kWidthFormat = 60;
constexpr int kWidthManufacturer = 130;
constexpr int kWidthScan = 100;
constexpr int kWidthValidation = 230;
constexpr int kWidthModified = 120;
constexpr int kWidthType = 150;
constexpr int kWidthFile = 500;
}

// Scans plugin files one by one with scanner processes, in a background thread; results are handed to the browser on the
// message thread.
class PluginScanThread : public juce::Thread
{
public:
    // folders empty: the standard folders of every format
    PluginScanThread(PluginBrowserComponent& owner, const juce::FileSearchPath& folders)
        : juce::Thread("PluginScan"), m_owner(owner), m_folders(folders)
    {
    }

    ~PluginScanThread() override
    {
        stopThread(kThreadStopMs);
    }

    void run() override
    {
        juce::AudioPluginFormatManager formatManager;
        hosting::addHeadlessFormats(formatManager);
        juce::StringArray files;
        if (m_folders.getNumPaths() == 0)
        {
            files = hosting::PluginScanner::findPluginFilesInStandardFolders(formatManager);
        }
        else
        {
            files = hosting::PluginScanner::findPluginFiles(formatManager, m_folders);
        }

        const hosting::PluginScanner scanner(hosting::PluginScanner::getDefaultScannerExecutable());
        const juce::Component::SafePointer<PluginBrowserComponent> owner(&m_owner);
        for (const juce::String& path : files)
        {
            if (threadShouldExit())
            {
                return;
            }
            const hosting::PluginScanResult result = scanner.scanFile(juce::File(path));
            juce::MessageManager::callAsync([owner, result]
                                            {
                                                if (owner != nullptr)
                                                {
                                                    owner->addScanResult(result);
                                                }
                                            });
        }
        juce::MessageManager::callAsync([owner]
                                        {
                                            if (owner != nullptr)
                                            {
                                                owner->scanFinished();
                                            }
                                        });
    }

private:
    PluginBrowserComponent& m_owner;
    juce::FileSearchPath m_folders;
};

// Validates one plugin file with pluginval in a background thread. A validator object of its own, so that nothing is shared with
// the message thread (the result file is the only common place).
class PluginValidationThread : public juce::Thread
{
public:
    // quickCheck: check only this plugin of the file (the file has many plugins) instead of validating the whole file with pluginval
    PluginValidationThread(PluginBrowserComponent& owner, const juce::File& pluginval, const juce::File& pluginFile, int strictnessLevel,
                           const juce::File& catalogFile, const juce::String& validationKey, const juce::PluginDescription& description,
                           bool quickCheck)
        : juce::Thread("PluginValidation"),
          m_owner(owner),
          m_pluginval(pluginval),
          m_pluginFile(pluginFile),
          m_strictnessLevel(strictnessLevel),
          m_catalogFile(catalogFile),
          m_validationKey(validationKey),
          m_description(description),
          m_quickCheck(quickCheck)
    {
    }

    ~PluginValidationThread() override
    {
        stopThread(hosting::PluginValidator::kDefaultTimeoutMs * 2 + 5000);
    }

    void run() override
    {
        hosting::PluginValidator validator(m_pluginval, m_catalogFile, m_strictnessLevel);
        hosting::ValidationResult result;
        if (m_quickCheck)
        {
            result = validator.validateQuick(m_pluginFile, m_description);
        }
        else
        {
            result = validator.validate(m_pluginFile);
        }
        const juce::Component::SafePointer<PluginBrowserComponent> owner(&m_owner);
        const juce::String key = m_validationKey;
        juce::MessageManager::callAsync([owner, key, result]
                                        {
                                            if (owner != nullptr)
                                            {
                                                owner->validationFinished(key, result);
                                            }
                                        });
    }

private:
    PluginBrowserComponent& m_owner;
    juce::File m_pluginval;
    juce::File m_pluginFile;
    int m_strictnessLevel;
    juce::File m_catalogFile;
    juce::String m_validationKey;
    juce::PluginDescription m_description;
    bool m_quickCheck;
};

PluginBrowserComponent::PluginBrowserComponent(const juce::File& catalogFile)
    : m_catalogFile(catalogFile), m_pluginval(hosting::PluginValidator::findPluginval())
{
    m_model = std::make_unique<TextTableModel>([this] { return static_cast<int>(m_rows.size()); },
                                               [this](int row, int column) { return getCellText(row, column); });
    m_model->setSortFunction([this](int columnId, bool forwards) { sortBy(columnId, forwards); });
    m_table.setModel(m_model.get());
    m_table.setRowHeight(kRowHeight);
    m_table.setHeaderHeight(kHeaderHeight);
    m_table.setMultipleSelectionEnabled(true);
    m_table.getHeader().addColumn("Plugin", kColumnName, kWidthName);
    m_table.getHeader().addColumn("Format", kColumnFormat, kWidthFormat);
    m_table.getHeader().addColumn("Type", kColumnType, kWidthType);
    m_table.getHeader().addColumn("Manufacturer", kColumnManufacturer, kWidthManufacturer);
    m_table.getHeader().addColumn("Scan", kColumnScan, kWidthScan);
    m_table.getHeader().addColumn("Validation", kColumnValidation, kWidthValidation);
    m_table.getHeader().addColumn("Modified", kColumnModified, kWidthModified);
    m_table.getHeader().addColumn("File", kColumnFile, kWidthFile);

    m_scanButton.onClick = [this] { scanStandardFolders(); };
    m_addFolderButton.onClick = [this] { chooseFolder(); };
    m_loadButton.onClick = [this] { loadSelected(); };
    m_allowUnvalidatedButton.setTooltip("Load the selected plugin without asking pluginval, also if it has failed the validation. Use this for plugins that you trust.");

    m_strictnessLabel.setText("pluginval level", juce::dontSendNotification);
    for (int level = kLowestStrictness; level <= kHighestStrictness; ++level)
    {
        m_strictnessBox.addItem(juce::String(level), level);
    }
    m_strictnessBox.setSelectedId(hosting::PluginValidator::kDefaultStrictnessLevel, juce::dontSendNotification);
    m_strictnessBox.setTooltip("How strict pluginval is. Level 5 is recommended; a lower level lets more plugins pass.");
    m_strictnessBox.onChange = [this] { strictnessChanged(); };

    for (juce::Component* component : std::initializer_list<juce::Component*>{
             &m_scanButton, &m_addFolderButton, &m_loadButton, &m_allowUnvalidatedButton, &m_strictnessLabel, &m_strictnessBox, &m_statusLabel, &m_table})
    {
        addAndMakeVisible(component);
    }

    showCatalog();
    if (m_pluginval == juce::File())
    {
        setStatus("pluginval was not found: plugins cannot be validated (set PLUGINLAB_PLUGINVAL or install pluginval).");
    }
}

PluginBrowserComponent::~PluginBrowserComponent()
{
    m_scanThread.reset();
    m_validationThread.reset();
    m_table.setModel(nullptr);
}

void PluginBrowserComponent::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    juce::Rectangle<int> buttons = area.removeFromTop(kButtonHeight);
    m_scanButton.setBounds(buttons.removeFromLeft(kScanButtonWidth));
    buttons.removeFromLeft(kMargin);
    m_addFolderButton.setBounds(buttons.removeFromLeft(kAddFolderButtonWidth));
    buttons.removeFromLeft(kMargin * 3);
    m_loadButton.setBounds(buttons.removeFromLeft(kLoadButtonWidth));
    buttons.removeFromLeft(kMargin);
    m_allowUnvalidatedButton.setBounds(buttons.removeFromLeft(kAllowUnvalidatedWidth));
    buttons.removeFromLeft(kMargin);
    m_strictnessLabel.setBounds(buttons.removeFromLeft(kStrictnessLabelWidth));
    m_strictnessBox.setBounds(buttons.removeFromLeft(kStrictnessBoxWidth));
    area.removeFromTop(kMargin);

    m_statusLabel.setBounds(area.removeFromBottom(kStatusHeight));
    m_table.setBounds(area);
}

void PluginBrowserComponent::setStatus(const juce::String& text)
{
    m_statusLabel.setText(text, juce::dontSendNotification);
}

juce::String PluginBrowserComponent::getValidationText(const Row& row) const
{
    if (! row.hasDescription)
    {
        return {};
    }
    const juce::String key = getValidationKey(row);
    if (key == m_validatingKey)
    {
        return "validating ...";
    }
    const auto found = m_validation.find(key);
    if (found == m_validation.end())
    {
        return "not validated";
    }
    const hosting::ValidationResult& result = found->second;
    if (result.status == hosting::ValidationStatus::NotAvailable)
    {
        return "pluginval missing";
    }
    if (result.outdated)
    {
        return "not validated (plugin changed since " + result.validatedAt + ")";
    }
    if (result.isQuickCheck)
    {
        return "Quick check " + hosting::toString(result.status) + " " + result.validatedAt;
    }
    return hosting::toString(result.status) + " " + result.validatedAt + ", level " + juce::String(getStrictnessLevel());
}

// A file with one plugin is validated as a whole by pluginval; a file with many plugins gets a quick check of each plugin on its own,
// so the result belongs to the plugin.
juce::String PluginBrowserComponent::getValidationKey(const Row& row)
{
    if (row.numberOfPluginsInFile > 1)
    {
        return row.file.getFullPathName() + "|" + row.description.createIdentifierString();
    }
    return row.file.getFullPathName();
}

void PluginBrowserComponent::rememberStoredValidation(const Row& row, const hosting::PluginValidator& validator)
{
    if (! row.hasDescription)
    {
        return;
    }
    hosting::ValidationResult stored;
    bool found = false;
    if (row.numberOfPluginsInFile > 1)
    {
        found = validator.getStoredQuickResult(row.file, row.description, stored);
    }
    else if (m_pluginval != juce::File())
    {
        found = validator.getStoredResult(row.file, stored);
    }
    if (found)
    {
        m_validation[getValidationKey(row)] = stored;
    }
}

juce::String PluginBrowserComponent::getCellText(int row, int columnId) const
{
    return getCellText(m_rows[static_cast<size_t>(row)], columnId);
}

juce::String PluginBrowserComponent::getCellText(const Row& entry, int columnId) const
{
    if (columnId == kColumnName)
    {
        if (entry.hasDescription)
        {
            return hosting::getDisplayName(entry.description);
        }
        return entry.file.getFileNameWithoutExtension();
    }
    if (columnId == kColumnFormat)
    {
        return entry.description.pluginFormatName;
    }
    if (columnId == kColumnType)
    {
        if (entry.hasDescription)
        {
            return hosting::getTypeText(entry.description);
        }
        return {};
    }
    if (columnId == kColumnManufacturer)
    {
        return entry.description.manufacturerName;
    }
    if (columnId == kColumnScan)
    {
        if (entry.changedSinceScan)
        {
            return hosting::toString(entry.scanStatus) + " (plugin changed, scan again)";
        }
        return hosting::toString(entry.scanStatus);
    }
    if (columnId == kColumnModified)
    {
        return entry.modified;
    }
    if (columnId == kColumnValidation)
    {
        return getValidationText(entry);
    }
    if (columnId == kColumnFile)
    {
        return entry.file.getFullPathName();
    }
    return {};
}

// Sorts the rows by the text of a column (case does not matter, numbers inside the text count as numbers); equal entries keep
// their order, so a second sort by another column keeps the first as the secondary order. The selected plugin stays selected.
void PluginBrowserComponent::sortBy(int columnId, bool forwards)
{
    m_sortColumnId = columnId;
    m_sortForwards = forwards;
    applySort();
}

void PluginBrowserComponent::applySort()
{
    if (m_sortColumnId == 0)
    {
        return;
    }
    juce::String selectedFile;
    juce::String selectedName;
    const int selected = m_table.getSelectedRow();
    if (selected >= 0 && selected < static_cast<int>(m_rows.size()))
    {
        selectedFile = m_rows[static_cast<size_t>(selected)].file.getFullPathName();
        selectedName = getCellText(m_rows[static_cast<size_t>(selected)], kColumnName);
    }

    std::stable_sort(m_rows.begin(), m_rows.end(),
                     [this](const Row& first, const Row& second)
                     {
                         const int order = getCellText(first, m_sortColumnId).compareNatural(getCellText(second, m_sortColumnId));
                         if (m_sortForwards)
                         {
                             return order < 0;
                         }
                         return order > 0;
                     });

    m_table.updateContent();
    m_table.deselectAllRows();
    for (size_t row = 0; row < m_rows.size(); ++row)
    {
        const bool isSelected = m_rows[row].file.getFullPathName() == selectedFile && getCellText(m_rows[row], kColumnName) == selectedName;
        if (isSelected && selectedFile.isNotEmpty())
        {
            m_table.selectRow(static_cast<int>(row));
        }
    }
    m_table.repaint();
}

void PluginBrowserComponent::setMultipleSelection(bool allowed)
{
    m_table.setMultipleSelectionEnabled(allowed);
}

int PluginBrowserComponent::getNumEntries() const
{
    return static_cast<int>(m_rows.size());
}

juce::String PluginBrowserComponent::getEntryName(int index) const
{
    return getCellText(index, kColumnName);
}

void PluginBrowserComponent::removeRowsOf(const juce::File& file)
{
    m_rows.erase(std::remove_if(m_rows.begin(), m_rows.end(), [&file](const Row& row) { return row.file == file; }), m_rows.end());
}

void PluginBrowserComponent::removeRowsOfMissingFiles()
{
    m_rows.erase(std::remove_if(m_rows.begin(), m_rows.end(), [](const Row& row) { return ! row.file.exists(); }), m_rows.end());
    m_table.updateContent();
    m_table.repaint();
}

// The list of the last scans, from the catalog file: shown at once when the browser opens. Plugins that are gone are left out, plugins that
// changed since the scan are marked.
void PluginBrowserComponent::showCatalog()
{
    const hosting::PluginValidator validator(m_pluginval, m_catalogFile, getStrictnessLevel());
    juce::String newestScan;
    for (const hosting::CatalogEntry& entry : hosting::PluginCatalog(m_catalogFile).load())
    {
        if (! entry.file.exists() || entry.scannedAt.isEmpty())
        {
            continue;
        }
        Row base;
        base.file = entry.file;
        base.scanStatus = entry.scanStatus;
        base.message = entry.message;
        base.modified = hosting::getModifiedText(entry.file);
        base.changedSinceScan = hosting::describePluginFile(entry.file) != entry.stamp;
        base.numberOfPluginsInFile = entry.descriptions.size();
        if (entry.descriptions.isEmpty())
        {
            m_rows.push_back(base);
        }
        for (const juce::PluginDescription& description : entry.descriptions)
        {
            Row row = base;
            row.description = description;
            row.hasDescription = true;
            m_rows.push_back(row);
            rememberStoredValidation(row, validator);
        }
        newestScan = juce::jmax(newestScan, entry.scannedAt);
    }
    if (m_rows.empty())
    {
        setStatus("Ready. Press 'Scan / rescan plugin folders' or add a folder.");
        return;
    }
    setStatus("Showing " + juce::String(static_cast<int>(m_rows.size())) + " entries of the scan of " + newestScan
              + ". Press 'Scan / rescan plugin folders' to update the list.");
    m_table.updateContent();
}

void PluginBrowserComponent::scanStandardFolders()
{
    startScan(juce::FileSearchPath());
}

void PluginBrowserComponent::scanFolder(const juce::File& folder)
{
    startScan(juce::FileSearchPath(folder.getFullPathName()));
}

void PluginBrowserComponent::startScan(const juce::FileSearchPath& folders)
{
    if (m_scanThread != nullptr && m_scanThread->isThreadRunning())
    {
        setStatus("A scan is already running.");
        return;
    }
    setStatus("Scanning ...");
    m_scanResults.clear();
    m_scanIsOfStandardFolders = folders.getNumPaths() == 0;
    m_scanThread = std::make_unique<PluginScanThread>(*this, folders);
    m_scanThread->startThread();
}

void PluginBrowserComponent::chooseFolder()
{
    m_folderChooser = std::make_unique<juce::FileChooser>("Folder with plugins");
    const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    m_folderChooser->launchAsync(chooserFlags,
                                 [this](const juce::FileChooser& chooser)
                                 {
                                     const juce::File folder = chooser.getResult();
                                     if (folder != juce::File())
                                     {
                                         scanFolder(folder);
                                     }
                                 });
}

void PluginBrowserComponent::addScanResult(const hosting::PluginScanResult& result)
{
    m_scanResults.push_back(result);
    removeRowsOf(result.file);
    const juce::String filePath = result.file.getFullPathName();
    for (auto known = m_validation.begin(); known != m_validation.end();)
    {
        // read again below: the plugin may have changed
        if (known->first == filePath || known->first.startsWith(filePath + "|"))
        {
            known = m_validation.erase(known);
            continue;
        }
        ++known;
    }

    Row base;
    base.file = result.file;
    base.modified = hosting::getModifiedText(result.file);
    base.scanStatus = result.status;
    base.message = result.message;
    base.numberOfPluginsInFile = result.descriptions.size();

    if (result.descriptions.isEmpty())
    {
        m_rows.push_back(base);
    }
    for (const juce::PluginDescription& description : result.descriptions)
    {
        Row row = base;
        row.description = description;
        row.hasDescription = true;
        m_rows.push_back(row);
    }

    // a result that is already remembered is shown at once
    const hosting::PluginValidator validator(m_pluginval, m_catalogFile, getStrictnessLevel());
    for (const Row& row : m_rows)
    {
        if (row.file == result.file)
        {
            rememberStoredValidation(row, validator);
        }
    }
    m_table.updateContent();
    m_table.repaint();
    applySort();
}

void PluginBrowserComponent::scanFinished()
{
    hosting::PluginCatalog(m_catalogFile).storeScanResults(m_scanResults, m_scanIsOfStandardFolders);
    m_scanResults.clear();
    if (m_scanIsOfStandardFolders)
    {
        removeRowsOfMissingFiles();
    }
    int numberOfProblems = 0;
    for (const Row& row : m_rows)
    {
        if (row.scanStatus != hosting::ScanStatus::Ok)
        {
            ++numberOfProblems;
        }
    }
    setStatus("Scan finished: " + juce::String(static_cast<int>(m_rows.size())) + " entries, " + juce::String(numberOfProblems)
              + " with problems. Select a plugin and press Load (it is validated first).");
    if (onScanFinished)
    {
        onScanFinished();
    }
}

bool PluginBrowserComponent::chooseByName(const juce::String& displayName)
{
    for (size_t row = 0; row < m_rows.size(); ++row)
    {
        if (m_rows[row].hasDescription && hosting::getDisplayName(m_rows[row].description) == displayName)
        {
            m_table.selectRow(static_cast<int>(row));
            loadSelected();
            return true;
        }
    }
    return false;
}

void PluginBrowserComponent::loadSelected()
{
    if (! m_loadQueue.empty())
    {
        setStatus("Plugins are still being loaded, please wait.");
        return;
    }
    const juce::SparseSet<int> selected = m_table.getSelectedRows();
    for (int index = 0; index < selected.size(); ++index)
    {
        const int row = selected[index];
        if (row >= 0 && row < static_cast<int>(m_rows.size()))
        {
            m_loadQueue.push_back(m_rows[static_cast<size_t>(row)]);
        }
    }
    if (m_loadQueue.empty())
    {
        setStatus("Select one or more plugins in the list first.");
        return;
    }
    m_queueTotal = static_cast<int>(m_loadQueue.size());
    m_queueLoaded = 0;
    m_queueRefused.clear();
    processLoadQueue();
}

// Loads the waiting plugins one after the other; stops while a check runs in the background (validationFinished goes on).
void PluginBrowserComponent::processLoadQueue()
{
    while (! m_loadQueue.empty())
    {
        const Row row = m_loadQueue.front();
        const LoadOutcome outcome = loadRow(row);
        if (outcome == LoadOutcome::Validating)
        {
            return;
        }
        m_loadQueue.pop_front();
        if (outcome == LoadOutcome::Loaded)
        {
            ++m_queueLoaded;
        }
        else
        {
            m_queueRefused.add(hosting::getDisplayName(row.description));
        }
    }
    if (m_queueTotal > 1)
    {
        juce::String text = "Loaded " + juce::String(m_queueLoaded) + " of " + juce::String(m_queueTotal) + " plugins.";
        if (! m_queueRefused.isEmpty())
        {
            text += " Not loaded: " + m_queueRefused.joinIntoString(", ") + ".";
        }
        setStatus(text);
    }
}

PluginBrowserComponent::LoadOutcome PluginBrowserComponent::loadRow(const Row& row)
{
    if (! row.hasDescription)
    {
        setStatus("Cannot load " + row.file.getFileName() + ": " + hosting::toString(row.scanStatus) + ". " + row.message);
        return LoadOutcome::Refused;
    }

    const juce::String key = getValidationKey(row);
    const auto known = m_validation.find(key);
    if (m_allowUnvalidatedButton.getToggleState())
    {
        if (onPluginChosen)
        {
            onPluginChosen(row.description);
        }
        return LoadOutcome::Loaded;
    }
    const bool knownIsCurrent = known != m_validation.end() && ! known->second.outdated;
    if (knownIsCurrent && known->second.status == hosting::ValidationStatus::Passed)
    {
        if (onPluginChosen)
        {
            onPluginChosen(row.description);
        }
        return LoadOutcome::Loaded;
    }
    if (knownIsCurrent && (known->second.status == hosting::ValidationStatus::Failed || known->second.status == hosting::ValidationStatus::TimedOut))
    {
        setStatus(hosting::getDisplayName(row.description) + " failed the validation and is not loaded: " + known->second.message
                      + " (Tick 'Load without validation' to load it anyway, or choose a lower strictness level.)");
        return LoadOutcome::Refused;
    }

    const bool quickCheck = row.numberOfPluginsInFile > 1;
    if (m_pluginval == juce::File() && ! quickCheck)
    {
        setStatus("pluginval was not found. Tick 'Load without validation' to load the plugin anyway.");
        return LoadOutcome::Refused;
    }

    if (m_validationThread != nullptr && m_validationThread->isThreadRunning())
    {
        setStatus("Another plugin is being validated, please wait.");
        return LoadOutcome::Refused;
    }
    m_validatingKey = key;
    m_pendingLoadKey = key;
    if (quickCheck)
    {
        setStatus("Quick check of " + hosting::getDisplayName(row.description) + " (the file has " + juce::String(row.numberOfPluginsInFile)
                  + " plugins, pluginval would test all of them) ...");
    }
    else
    {
        setStatus("Validating " + hosting::getDisplayName(row.description) + " with pluginval ...");
    }
    m_table.repaint();
    m_validationThread = std::make_unique<PluginValidationThread>(*this, m_pluginval, row.file, getStrictnessLevel(), m_catalogFile, key,
                                                                  row.description, quickCheck);
    m_validationThread->startThread();
    return LoadOutcome::Validating;
}

void PluginBrowserComponent::validationFinished(const juce::String& validationKey, const hosting::ValidationResult& result)
{
    m_validatingKey.clear();
    m_validation[validationKey] = result;
    m_table.repaint();
    applySort(); // the validation column may be the sort column

    const bool loadWanted = (validationKey == m_pendingLoadKey);
    m_pendingLoadKey.clear();
    for (const Row& row : m_rows)
    {
        if (! row.hasDescription || getValidationKey(row) != validationKey)
        {
            continue;
        }
        showValidationOutcome(row, result);
        const bool passed = result.status == hosting::ValidationStatus::Passed;
        if (loadWanted && passed && onPluginChosen)
        {
            onPluginChosen(row.description);
        }
        if (loadWanted && ! m_loadQueue.empty())
        {
            m_loadQueue.pop_front();
            if (loadWanted && passed)
            {
                ++m_queueLoaded;
            }
            else
            {
                m_queueRefused.add(hosting::getDisplayName(row.description));
            }
            processLoadQueue();
        }
        return;
    }
}

void PluginBrowserComponent::showValidationOutcome(const Row& row, const hosting::ValidationResult& result)
{
    const juce::String name = hosting::getDisplayName(row.description);
    if (result.status == hosting::ValidationStatus::Passed)
    {
        setStatus(name + ": " + result.message);
        return;
    }
    setStatus(name + " is not loaded: " + result.message);
}
}

namespace pluginlab::ui
{
int PluginBrowserComponent::getStrictnessLevel() const
{
    return m_strictnessBox.getSelectedId();
}

// results are valid for one level only: forget them and take the ones that are cached for the new level
void PluginBrowserComponent::strictnessChanged()
{
    m_validation.clear();
    const hosting::PluginValidator validator(m_pluginval, m_catalogFile, getStrictnessLevel());
    for (const Row& row : m_rows)
    {
        rememberStoredValidation(row, validator);
    }
    m_table.repaint();
}
}
