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
constexpr int kScanButtonWidth = 170;
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
constexpr int kColumnFile = 6;
constexpr int kWidthName = 200;
constexpr int kWidthFormat = 60;
constexpr int kWidthManufacturer = 130;
constexpr int kWidthScan = 100;
constexpr int kWidthValidation = 130;
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
    PluginValidationThread(PluginBrowserComponent& owner, const juce::File& pluginval, const juce::File& pluginFile, int strictnessLevel)
        : juce::Thread("PluginValidation"),
          m_owner(owner),
          m_pluginval(pluginval),
          m_pluginFile(pluginFile),
          m_strictnessLevel(strictnessLevel)
    {
    }

    ~PluginValidationThread() override
    {
        stopThread(hosting::PluginValidator::kDefaultTimeoutMs * 2 + 5000);
    }

    void run() override
    {
        hosting::PluginValidator validator(m_pluginval, hosting::PluginValidator::getDefaultCacheFile(), m_strictnessLevel);
        const hosting::ValidationResult result = validator.validate(m_pluginFile);
        const juce::Component::SafePointer<PluginBrowserComponent> owner(&m_owner);
        const juce::File pluginFile = m_pluginFile;
        juce::MessageManager::callAsync([owner, pluginFile, result]
                                        {
                                            if (owner != nullptr)
                                            {
                                                owner->validationFinished(pluginFile, result);
                                            }
                                        });
    }

private:
    PluginBrowserComponent& m_owner;
    juce::File m_pluginval;
    juce::File m_pluginFile;
    int m_strictnessLevel;
};

PluginBrowserComponent::PluginBrowserComponent()
    : m_pluginval(hosting::PluginValidator::findPluginval())
{
    m_model = std::make_unique<TextTableModel>([this] { return static_cast<int>(m_rows.size()); },
                                               [this](int row, int column) { return getCellText(row, column); });
    m_model->setSortFunction([this](int columnId, bool forwards) { sortBy(columnId, forwards); });
    m_table.setModel(m_model.get());
    m_table.setRowHeight(kRowHeight);
    m_table.setHeaderHeight(kHeaderHeight);
    m_table.getHeader().addColumn("Plugin", kColumnName, kWidthName);
    m_table.getHeader().addColumn("Format", kColumnFormat, kWidthFormat);
    m_table.getHeader().addColumn("Manufacturer", kColumnManufacturer, kWidthManufacturer);
    m_table.getHeader().addColumn("Scan", kColumnScan, kWidthScan);
    m_table.getHeader().addColumn("Validation", kColumnValidation, kWidthValidation);
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

    if (m_pluginval == juce::File())
    {
        setStatus("pluginval was not found: plugins cannot be validated (set PLUGINLAB_PLUGINVAL or install pluginval).");
    }
    else
    {
        setStatus("Ready. Scan the standard plugin folders or add a folder.");
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
    const juce::String path = row.file.getFullPathName();
    if (path == m_validatingPath)
    {
        return "validating ...";
    }
    const auto found = m_validation.find(path);
    if (found == m_validation.end())
    {
        return "not validated";
    }
    if (found->second.status == hosting::ValidationStatus::NotAvailable)
    {
        return "pluginval missing";
    }
    return hosting::toString(found->second.status);
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
    if (columnId == kColumnManufacturer)
    {
        return entry.description.manufacturerName;
    }
    if (columnId == kColumnScan)
    {
        return hosting::toString(entry.scanStatus);
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

int PluginBrowserComponent::getNumEntries() const
{
    return static_cast<int>(m_rows.size());
}

juce::String PluginBrowserComponent::getEntryName(int index) const
{
    return getCellText(index, kColumnName);
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
    Row base;
    base.file = result.file;
    base.scanStatus = result.status;
    base.message = result.message;

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
    const juce::String path = result.file.getFullPathName();
    if (m_pluginval != juce::File() && ! result.descriptions.isEmpty() && m_validation.find(path) == m_validation.end())
    {
        const hosting::PluginValidator validator(m_pluginval, hosting::PluginValidator::getDefaultCacheFile(), getStrictnessLevel());
        hosting::ValidationResult cached;
        if (validator.getCachedResult(result.file, cached))
        {
            m_validation[path] = cached;
        }
    }
    m_table.updateContent();
    m_table.repaint();
    applySort();
}

void PluginBrowserComponent::scanFinished()
{
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
    const int selected = m_table.getSelectedRow();
    if (selected < 0 || selected >= static_cast<int>(m_rows.size()))
    {
        setStatus("Select a plugin in the list first.");
        return;
    }
    loadRow(m_rows[static_cast<size_t>(selected)]);
}

void PluginBrowserComponent::loadRow(const Row& row)
{
    if (! row.hasDescription)
    {
        setStatus("Cannot load " + row.file.getFileName() + ": " + hosting::toString(row.scanStatus) + ". " + row.message);
        return;
    }

    const juce::String path = row.file.getFullPathName();
    const auto known = m_validation.find(path);
    if (m_allowUnvalidatedButton.getToggleState())
    {
        if (onPluginChosen)
        {
            onPluginChosen(row.description);
        }
        return;
    }
    if (known != m_validation.end() && known->second.status == hosting::ValidationStatus::Passed)
    {
        if (onPluginChosen)
        {
            onPluginChosen(row.description);
        }
        return;
    }
    if (known != m_validation.end() && (known->second.status == hosting::ValidationStatus::Failed || known->second.status == hosting::ValidationStatus::TimedOut))
    {
        setStatus(hosting::getDisplayName(row.description) + " failed the validation and is not loaded: " + known->second.message
                      + " (Tick 'Load without validation' to load it anyway, or choose a lower strictness level.)");
        return;
    }

    if (m_pluginval == juce::File())
    {
        setStatus("pluginval was not found. Tick 'Load without validation' to load the plugin anyway.");
        return;
    }

    if (m_validationThread != nullptr && m_validationThread->isThreadRunning())
    {
        setStatus("Another plugin is being validated, please wait.");
        return;
    }
    m_validatingPath = path;
    m_pendingLoadPath = path;
    setStatus("Validating " + hosting::getDisplayName(row.description) + " with pluginval ...");
    m_table.repaint();
    m_validationThread = std::make_unique<PluginValidationThread>(*this, m_pluginval, row.file, getStrictnessLevel());
    m_validationThread->startThread();
}

void PluginBrowserComponent::validationFinished(const juce::File& pluginFile, const hosting::ValidationResult& result)
{
    const juce::String path = pluginFile.getFullPathName();
    m_validatingPath.clear();
    m_validation[path] = result;
    m_table.repaint();
    applySort(); // the validation column may be the sort column

    const bool loadWanted = (path == m_pendingLoadPath);
    m_pendingLoadPath.clear();
    for (const Row& row : m_rows)
    {
        if (row.file != pluginFile || ! row.hasDescription)
        {
            continue;
        }
        showValidationOutcome(row, result);
        if (loadWanted && result.status == hosting::ValidationStatus::Passed && onPluginChosen)
        {
            onPluginChosen(row.description);
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
    if (m_pluginval != juce::File())
    {
        const hosting::PluginValidator validator(m_pluginval, hosting::PluginValidator::getDefaultCacheFile(), getStrictnessLevel());
        for (const Row& row : m_rows)
        {
            hosting::ValidationResult cached;
            if (row.hasDescription && validator.getCachedResult(row.file, cached))
            {
                m_validation[row.file.getFullPathName()] = cached;
            }
        }
    }
    m_table.repaint();
}
}
