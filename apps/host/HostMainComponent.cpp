#include "HostMainComponent.h"

#include <functional>

#include "PluginWindow.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/hosting/PluginScanner.h"

namespace pluginlab::host
{
namespace
{
constexpr int kWindowWidth = 1000;
constexpr int kWindowHeight = 700;
constexpr int kMargin = 8;
constexpr int kButtonHeight = 28;
constexpr int kStatusHeight = 24;
constexpr int kRowHeight = 24;
constexpr int kHeaderHeight = 24;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kParameterRefreshHz = 5;
constexpr int kNoSelection = -1;

// the columns of the tables (ids start at 1)
constexpr int kScanColumnName = 1;
constexpr int kScanColumnManufacturer = 2;
constexpr int kScanColumnStatus = 3;
constexpr int kScanColumnFile = 4;
constexpr int kLoadedColumnName = 1;
constexpr int kLoadedColumnParameters = 2;
constexpr int kParameterColumnIndex = 1;
constexpr int kParameterColumnName = 2;
constexpr int kParameterColumnValue = 3;
constexpr int kParameterColumnSlider = 4;
}

// A table that shows text, taken from functions of its owner.
class TextTableModel : public juce::TableListBoxModel
{
public:
    using CountFunction = std::function<int()>;
    using TextFunction = std::function<juce::String(int row, int columnId)>;

    TextTableModel(CountFunction count, TextFunction text)
        : m_count(std::move(count)), m_text(std::move(text))
    {
    }

    int getNumRows() override
    {
        return m_count();
    }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override
    {
        juce::ignoreUnused(rowNumber, width, height);
        if (rowIsSelected)
        {
            g.fillAll(juce::Colour(0xff4060a0));
        }
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override
    {
        g.setColour(juce::Colours::white);
        if (! rowIsSelected)
        {
            g.setColour(juce::Colour(0xffd0d0d0));
        }
        g.drawText(m_text(rowNumber, columnId), kMargin / 2, 0, width - kMargin, height, juce::Justification::centredLeft, true);
    }

private:
    CountFunction m_count;
    TextFunction m_text;
};

// The parameters of the selected plugin: index, name, value as text and a slider (normalised value 0 ... 1).
class ParameterTableModel : public TextTableModel
{
public:
    using ChangeFunction = std::function<void(int row, float normalisedValue)>;
    using ValueFunction = std::function<float(int row)>;

    ParameterTableModel(CountFunction count, TextFunction text, ValueFunction value, ChangeFunction change)
        : TextTableModel(std::move(count), std::move(text)), m_value(std::move(value)), m_change(std::move(change))
    {
    }

    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existing) override
    {
        juce::ignoreUnused(isRowSelected);
        if (columnId != kParameterColumnSlider || rowNumber >= getNumRows())
        {
            delete existing;
            return nullptr;
        }

        auto* slider = dynamic_cast<juce::Slider*>(existing);
        if (slider == nullptr)
        {
            slider = new juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
            slider->setRange(0.0, 1.0);
        }
        slider->onValueChange = [this, slider, rowNumber]
        {
            m_change(rowNumber, static_cast<float>(slider->getValue()));
        };
        if (! slider->isMouseButtonDown())
        {
            slider->setValue(static_cast<double>(m_value(rowNumber)), juce::dontSendNotification);
        }
        return slider;
    }

private:
    ValueFunction m_value;
    ChangeFunction m_change;
};

// Scans plugin files one by one with scanner processes, in a background thread. The results are handed to the main window
// on the message thread.
class PluginScanThread : public juce::Thread
{
public:
    // folders empty: the standard folders of every format
    PluginScanThread(HostMainComponent& owner, const juce::FileSearchPath& folders)
        : juce::Thread("PluginScan"), m_owner(owner), m_folders(folders)
    {
    }

    ~PluginScanThread() override
    {
        stopThread(hosting::PluginScanner::kDefaultTimeoutMs + 5000);
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
        const juce::Component::SafePointer<HostMainComponent> owner(&m_owner);

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
    HostMainComponent& m_owner;
    juce::FileSearchPath m_folders;
};

HostMainComponent::HostMainComponent(const StartupOptions& options)
    : m_pluginNameToLoad(options.pluginNameToLoad)
{
    m_formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
#if PLUGINLAB_WITH_VST2
    m_formatManager.addFormat(std::make_unique<juce::VSTPluginFormat>());
#endif

    m_scanModel = std::make_unique<TextTableModel>([this] { return static_cast<int>(m_scanRows.size()); },
                                                   [this](int row, int column) { return getScanCellText(row, column); });
    m_loadedModel = std::make_unique<TextTableModel>([this] { return static_cast<int>(m_loadedPlugins.size()); },
                                                     [this](int row, int column) { return getLoadedCellText(row, column); });
    m_parameterModel = std::make_unique<ParameterTableModel>(
        [this] { return static_cast<int>(m_parameterRows.size()); },
        [this](int row, int column)
        {
            const hosting::ParameterInfo& parameter = m_parameterRows[static_cast<size_t>(row)];
            if (column == kParameterColumnIndex)
            {
                return juce::String(parameter.index);
            }
            if (column == kParameterColumnName)
            {
                return parameter.name;
            }
            if (column == kParameterColumnValue)
            {
                return parameter.valueText + " " + parameter.label;
            }
            return juce::String();
        },
        [this](int row) { return m_parameterRows[static_cast<size_t>(row)].normalisedValue; },
        [this](int row, float value)
        {
            if (LoadedPlugin* loaded = getSelectedLoadedPlugin())
            {
                loaded->plugin->setParameterNormalised(row, value);
            }
        });

    m_scanTable.setModel(m_scanModel.get());
    m_scanTable.setRowHeight(kRowHeight);
    m_scanTable.setHeaderHeight(kHeaderHeight);
    m_scanTable.getHeader().addColumn("Plugin", kScanColumnName, 220);
    m_scanTable.getHeader().addColumn("Manufacturer", kScanColumnManufacturer, 140);
    m_scanTable.getHeader().addColumn("Status", kScanColumnStatus, 110);
    m_scanTable.getHeader().addColumn("File", kScanColumnFile, 500);

    m_loadedTable.setModel(m_loadedModel.get());
    m_loadedTable.setRowHeight(kRowHeight);
    m_loadedTable.setHeaderHeight(kHeaderHeight);
    m_loadedTable.getHeader().addColumn("Loaded plugin", kLoadedColumnName, 260);
    m_loadedTable.getHeader().addColumn("Parameters", kLoadedColumnParameters, 90);

    m_parameterTable.setModel(m_parameterModel.get());
    m_parameterTable.setRowHeight(kRowHeight);
    m_parameterTable.setHeaderHeight(kHeaderHeight);
    m_parameterTable.getHeader().addColumn("#", kParameterColumnIndex, 40);
    m_parameterTable.getHeader().addColumn("Parameter", kParameterColumnName, 200);
    m_parameterTable.getHeader().addColumn("Value", kParameterColumnValue, 140);
    m_parameterTable.getHeader().addColumn("", kParameterColumnSlider, 300);

    m_scanButton.onClick = [this] { startScan(juce::FileSearchPath()); };
    m_addFolderButton.onClick = [this] { chooseFolderToScan(); };
    m_loadButton.onClick = [this] { loadSelectedPlugin(); };
    m_unloadButton.onClick = [this] { unloadSelectedPlugin(); };
    m_editorButton.onClick = [this] { showEditorOfSelectedPlugin(); };
    m_loadedTable.setMultipleSelectionEnabled(false);

    for (juce::Component* component : std::initializer_list<juce::Component*>{
             &m_scanButton, &m_addFolderButton, &m_loadButton, &m_unloadButton, &m_editorButton, &m_statusLabel,
             &m_scanTable, &m_loadedTable, &m_parameterTable})
    {
        addAndMakeVisible(component);
    }
    setStatus("Ready. Scan the standard plugin folders or add a folder.");

    setSize(kWindowWidth, kWindowHeight);
    startTimerHz(kParameterRefreshHz);

    if (options.scanFolder != juce::File())
    {
        startScan(juce::FileSearchPath(options.scanFolder.getFullPathName()));
    }
}

HostMainComponent::~HostMainComponent()
{
    stopTimer();
    m_scanThread.reset();
    m_parameterTable.setModel(nullptr);
    for (std::unique_ptr<LoadedPlugin>& loaded : m_loadedPlugins)
    {
        loaded->window.reset(); // the editor must go before the plugin
    }
}

void HostMainComponent::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);

    m_statusLabel.setBounds(area.removeFromBottom(kStatusHeight));
    area.removeFromBottom(kMargin);

    juce::Rectangle<int> scanButtons = area.removeFromTop(kButtonHeight);
    m_scanButton.setBounds(scanButtons.removeFromLeft(180));
    scanButtons.removeFromLeft(kMargin);
    m_addFolderButton.setBounds(scanButtons.removeFromLeft(120));
    area.removeFromTop(kMargin);

    m_scanTable.setBounds(area.removeFromTop(area.getHeight() / 3));
    area.removeFromTop(kMargin);

    juce::Rectangle<int> loadButtons = area.removeFromTop(kButtonHeight);
    m_loadButton.setBounds(loadButtons.removeFromLeft(100));
    loadButtons.removeFromLeft(kMargin);
    m_unloadButton.setBounds(loadButtons.removeFromLeft(100));
    loadButtons.removeFromLeft(kMargin);
    m_editorButton.setBounds(loadButtons.removeFromLeft(120));
    area.removeFromTop(kMargin);

    m_loadedTable.setBounds(area.removeFromLeft(area.getWidth() / 3));
    area.removeFromLeft(kMargin);
    m_parameterTable.setBounds(area);
}

void HostMainComponent::setStatus(const juce::String& text)
{
    m_statusLabel.setText(text, juce::dontSendNotification);
}

juce::String HostMainComponent::getScanCellText(int row, int columnId) const
{
    const ScanRow& scanRow = m_scanRows[static_cast<size_t>(row)];
    if (columnId == kScanColumnName)
    {
        if (scanRow.hasDescription)
        {
            return hosting::getDisplayName(scanRow.description);
        }
        return scanRow.file.getFileNameWithoutExtension();
    }
    if (columnId == kScanColumnManufacturer)
    {
        return scanRow.description.manufacturerName;
    }
    if (columnId == kScanColumnStatus)
    {
        return hosting::toString(scanRow.status);
    }
    if (columnId == kScanColumnFile)
    {
        return scanRow.file.getFullPathName();
    }
    return {};
}

juce::String HostMainComponent::getLoadedCellText(int row, int columnId) const
{
    const hosting::HostedPlugin& plugin = *m_loadedPlugins[static_cast<size_t>(row)]->plugin;
    if (columnId == kLoadedColumnName)
    {
        return hosting::getDisplayName(plugin.getDescription());
    }
    if (columnId == kLoadedColumnParameters)
    {
        return juce::String(static_cast<int>(plugin.getParameters().size()));
    }
    return {};
}

HostMainComponent::LoadedPlugin* HostMainComponent::getSelectedLoadedPlugin()
{
    const int row = m_loadedTable.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(m_loadedPlugins.size()))
    {
        return nullptr;
    }
    return m_loadedPlugins[static_cast<size_t>(row)].get();
}

void HostMainComponent::startScan(const juce::FileSearchPath& folders)
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

void HostMainComponent::chooseFolderToScan()
{
    m_folderChooser = std::make_unique<juce::FileChooser>("Folder with VST3 plugins");
    const int chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    m_folderChooser->launchAsync(chooserFlags,
                                 [this](const juce::FileChooser& chooser)
                                 {
                                     const juce::File folder = chooser.getResult();
                                     if (folder != juce::File())
                                     {
                                         startScan(juce::FileSearchPath(folder.getFullPathName()));
                                     }
                                 });
}

void HostMainComponent::addScanResult(const hosting::PluginScanResult& result)
{
    if (result.descriptions.isEmpty())
    {
        ScanRow row;
        row.file = result.file;
        row.status = result.status;
        row.message = result.message;
        m_scanRows.push_back(row);
    }
    for (const juce::PluginDescription& description : result.descriptions)
    {
        ScanRow row;
        row.file = result.file;
        row.status = result.status;
        row.message = result.message;
        row.description = description;
        row.hasDescription = true;
        m_scanRows.push_back(row);
    }
    m_scanTable.updateContent();
    m_scanTable.repaint();
}

void HostMainComponent::scanFinished()
{
    int numberOfProblems = 0;
    for (const ScanRow& row : m_scanRows)
    {
        if (row.status != hosting::ScanStatus::Ok)
        {
            ++numberOfProblems;
        }
    }
    setStatus("Scan finished: " + juce::String(static_cast<int>(m_scanRows.size())) + " entries, "
              + juce::String(numberOfProblems) + " with problems (see the Status column).");

    if (m_pluginNameToLoad.isNotEmpty())
    {
        for (size_t row = 0; row < m_scanRows.size(); ++row)
        {
            if (m_scanRows[row].hasDescription && m_scanRows[row].description.name == m_pluginNameToLoad)
            {
                m_scanTable.selectRow(static_cast<int>(row));
                loadSelectedPlugin();
                break;
            }
        }
        m_pluginNameToLoad.clear();
    }
}

void HostMainComponent::loadSelectedPlugin()
{
    const int row = m_scanTable.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(m_scanRows.size()))
    {
        setStatus("Select a plugin in the list first.");
        return;
    }

    const ScanRow& scanRow = m_scanRows[static_cast<size_t>(row)];
    if (! scanRow.hasDescription)
    {
        setStatus("Cannot load " + scanRow.file.getFileName() + ": " + hosting::toString(scanRow.status) + ". " + scanRow.message);
        return;
    }

    juce::String error;
    std::unique_ptr<hosting::HostedPlugin> plugin =
        hosting::HostedPlugin::load(m_formatManager, scanRow.description, kSampleRate, kBlockSize, error);
    if (plugin == nullptr)
    {
        setStatus("Cannot load " + hosting::getDisplayName(scanRow.description) + ": " + error);
        return;
    }

    auto loaded = std::make_unique<LoadedPlugin>();
    loaded->plugin = std::move(plugin);
    LoadedPlugin& loadedReference = *loaded;
    m_loadedPlugins.push_back(std::move(loaded));
    m_loadedTable.updateContent();
    m_loadedTable.selectRow(static_cast<int>(m_loadedPlugins.size()) - 1);
    showEditor(loadedReference);
    setStatus("Loaded " + hosting::getDisplayName(scanRow.description) + ".");
}

void HostMainComponent::unloadSelectedPlugin()
{
    const int row = m_loadedTable.getSelectedRow();
    if (getSelectedLoadedPlugin() == nullptr)
    {
        setStatus("Select a loaded plugin first.");
        return;
    }
    m_loadedPlugins[static_cast<size_t>(row)]->window.reset(); // the editor before the plugin
    m_loadedPlugins.erase(m_loadedPlugins.begin() + row);
    m_parameterRows.clear();
    m_loadedTable.updateContent();
    m_loadedTable.deselectAllRows();
    m_parameterTable.updateContent();
    setStatus("Unloaded.");
}

void HostMainComponent::showEditorOfSelectedPlugin()
{
    LoadedPlugin* loaded = getSelectedLoadedPlugin();
    if (loaded == nullptr)
    {
        setStatus("Select a loaded plugin first.");
        return;
    }
    showEditor(*loaded);
}

void HostMainComponent::showEditor(LoadedPlugin& loaded)
{
    if (loaded.window != nullptr)
    {
        loaded.window->toFront(true);
        return;
    }

    LoadedPlugin* loadedPointer = &loaded;
    const juce::Component::SafePointer<HostMainComponent> self(this);
    loaded.window = std::make_unique<PluginWindow>(
        loaded.plugin->getInstance(), hosting::getDisplayName(loaded.plugin->getDescription()),
        [self, loadedPointer]
        {
            // closing the window does not unload the plugin; delete the window after the close handler has returned
            juce::MessageManager::callAsync([self, loadedPointer]
                                            {
                                                if (self == nullptr)
                                                {
                                                    return;
                                                }
                                                for (const std::unique_ptr<LoadedPlugin>& entry : self->m_loadedPlugins)
                                                {
                                                    if (entry.get() == loadedPointer)
                                                    {
                                                        entry->window.reset();
                                                    }
                                                }
                                            });
        });
}

void HostMainComponent::refreshParameters()
{
    LoadedPlugin* loaded = getSelectedLoadedPlugin();
    m_parameterRows.clear();
    if (loaded != nullptr)
    {
        m_parameterRows = loaded->plugin->getParameters();
    }
    m_parameterTable.updateContent();
    m_parameterTable.repaint();
}

void HostMainComponent::timerCallback()
{
    refreshParameters();
}
}
