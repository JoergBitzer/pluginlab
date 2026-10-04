#include "HostMainComponent.h"

#include "pluginlab/engine/SessionFiles.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/ui/PluginEditorWindow.h"
#include "pluginlab/ui/TextTableModel.h"

namespace pluginlab::host
{
namespace
{
constexpr int kMargin = 8;
constexpr int kButtonHeight = 28;
constexpr int kRowHeight = 24;
constexpr int kHeaderHeight = 24;
constexpr int kUnloadButtonWidth = 100;
constexpr int kEditorButtonWidth = 120;
constexpr int kSetButtonWidth = 170;
constexpr int kLoadedColumnName = 1;
constexpr int kLoadedColumnFormat = 2;
constexpr int kLoadedColumnParameters = 3;
constexpr int kLoadedWidthName = 200;
constexpr int kLoadedWidthFormat = 60;
constexpr int kLoadedWidthParameters = 90;
constexpr int kLoadedListWidth = 380;
const juce::String kPluginSetWildcard = "*" + pluginlab::engine::kPluginSetExtension;
}

HostMainComponent::HostMainComponent(pluginlab::engine::MeasurementEngine& engine,
                                     juce::AudioPluginFormatManager& formatManager,
                                     HostSettings& settings,
                                     const StartupOptions& options)
    : m_engine(engine),
      m_formatManager(formatManager),
      m_settings(settings),
      m_pluginNameToLoad(options.pluginNameToLoad),
      m_showEditorAfterLoad(options.pluginNameToLoad.isNotEmpty())
{
    m_loadedModel = std::make_unique<ui::TextTableModel>(
        [this] { return static_cast<int>(getPluginSlots().size()); },
        [this](int row, int column)
        {
            const std::vector<int> slots = getPluginSlots();
            if (row < 0 || row >= static_cast<int>(slots.size()))
            {
                return juce::String();
            }
            const hosting::HostedPlugin* plugin = m_engine.getPlugin(slots[static_cast<size_t>(row)]);
            if (plugin == nullptr)
            {
                return juce::String();
            }
            if (column == kLoadedColumnName)
            {
                return hosting::getDisplayName(plugin->getDescription());
            }
            if (column == kLoadedColumnFormat)
            {
                return plugin->getDescription().pluginFormatName;
            }
            if (column == kLoadedColumnParameters)
            {
                return juce::String(plugin->getNumParameters());
            }
            return juce::String();
        },
        [this] { selectionChanged(); });
    m_loadedTable.setModel(m_loadedModel.get());
    m_loadedTable.setRowHeight(kRowHeight);
    m_loadedTable.setHeaderHeight(kHeaderHeight);
    m_loadedTable.setMultipleSelectionEnabled(true);
    m_loadedTable.getHeader().addColumn("Loaded plugin", kLoadedColumnName, kLoadedWidthName);
    m_loadedTable.getHeader().addColumn("Format", kLoadedColumnFormat, kLoadedWidthFormat);
    m_loadedTable.getHeader().addColumn("Parameters", kLoadedColumnParameters, kLoadedWidthParameters);

    m_browser.onPluginChosen = [this](const juce::PluginDescription& description) { loadPlugin(description); };
    m_browser.onScanFinished = [this]
    {
        if (m_pluginNameToLoad.isNotEmpty())
        {
            const juce::String name = m_pluginNameToLoad;
            m_pluginNameToLoad.clear();
            m_browser.chooseByName(name);
        }
    };
    m_unloadButton.onClick = [this] { unloadSelectedPlugins(); };
    m_editorButton.onClick = [this] { showEditorsOfSelectedPlugins(); };
    m_saveSetButton.onClick = [this] { savePluginSet(); };
    m_loadSetButton.onClick = [this] { loadPluginSet(); };
    m_saveSetButton.setTooltip("Saves the loaded plugins with their settings (parameters) in a file, to come back to this set later");
    m_loadSetButton.setTooltip("Replaces the loaded plugins by a saved set. A set with a plugin that crashes can crash the program.");

    for (juce::Component* component : std::initializer_list<juce::Component*>{
             &m_browser, &m_unloadButton, &m_editorButton, &m_saveSetButton, &m_loadSetButton, &m_loadedTable, &m_parameters})
    {
        addAndMakeVisible(component);
    }

    if (options.scanFolder != juce::File())
    {
        m_browser.scanFolder(options.scanFolder);
    }
}

HostMainComponent::~HostMainComponent()
{
    m_parameters.setPlugin(nullptr);
    m_loadedTable.setModel(nullptr);
    closeAllEditorWindows();
}

void HostMainComponent::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);

    m_browser.setBounds(area.removeFromTop(area.getHeight() / 2));
    area.removeFromTop(kMargin);

    juce::Rectangle<int> buttons = area.removeFromTop(kButtonHeight);
    m_unloadButton.setBounds(buttons.removeFromLeft(kUnloadButtonWidth));
    buttons.removeFromLeft(kMargin);
    m_editorButton.setBounds(buttons.removeFromLeft(kEditorButtonWidth));
    buttons.removeFromLeft(kMargin * 3);
    m_saveSetButton.setBounds(buttons.removeFromLeft(kSetButtonWidth));
    buttons.removeFromLeft(kMargin);
    m_loadSetButton.setBounds(buttons.removeFromLeft(kSetButtonWidth));
    area.removeFromTop(kMargin);

    m_loadedTable.setBounds(area.removeFromLeft(kLoadedListWidth));
    area.removeFromLeft(kMargin);
    m_parameters.setBounds(area);
}

// the slots of the engine that hold a plugin (the dry slot is not listed)
std::vector<int> HostMainComponent::getPluginSlots() const
{
    std::vector<int> slots;
    for (int index = 0; index < m_engine.getNumSlots(); ++index)
    {
        if (m_engine.getSlotInfo(index).hasPlugin)
        {
            slots.push_back(index);
        }
    }
    return slots;
}

void HostMainComponent::refresh()
{
    m_parameters.setPlugin(nullptr);
    m_loadedTable.updateContent();
    m_loadedTable.deselectAllRows();
    m_loadedTable.repaint();
}

void HostMainComponent::closeAllEditorWindows()
{
    m_windows.clear();
}

void HostMainComponent::slotsChanged()
{
    m_loadedTable.updateContent();
    if (onSlotsChanged)
    {
        onSlotsChanged();
    }
}

void HostMainComponent::selectionChanged()
{
    const std::vector<int> slots = getPluginSlots();
    const int row = m_loadedTable.getLastRowSelected();
    if (row < 0 || row >= static_cast<int>(slots.size()))
    {
        m_parameters.setPlugin(nullptr);
        return;
    }
    m_parameters.setPlugin(m_engine.getPlugin(slots[static_cast<size_t>(row)]));
}

void HostMainComponent::loadPlugin(const juce::PluginDescription& description)
{
    juce::String error;
    std::unique_ptr<hosting::HostedPlugin> plugin = hosting::HostedPlugin::load(
        m_formatManager, description, m_engine.getSampleRate(), m_engine.getMaxBlockSize(), error);
    if (plugin == nullptr)
    {
        m_browser.setStatus("Cannot load " + hosting::getDisplayName(description) + ": " + error);
        return;
    }
    const int slot = m_engine.addSlot(std::move(plugin), hosting::getDisplayName(description), error);
    if (slot < 0)
    {
        m_browser.setStatus("Cannot use " + hosting::getDisplayName(description) + ": " + error);
        return;
    }
    slotsChanged();
    const std::vector<int> slots = getPluginSlots();
    m_loadedTable.selectRow(static_cast<int>(slots.size()) - 1);
    if (m_showEditorAfterLoad)
    {
        m_showEditorAfterLoad = false; // only for --load (manual tests, screenshots)
        showEditorOfSlot(slot);
    }
    m_browser.setStatus("Loaded " + hosting::getDisplayName(description) + " (latency " + juce::String(m_engine.getSlotInfo(slot).measuredLatency)
                        + " samples).");
}

void HostMainComponent::unloadSelectedPlugins()
{
    const std::vector<int> slots = getPluginSlots();
    const juce::SparseSet<int> selected = m_loadedTable.getSelectedRows();
    if (selected.size() == 0)
    {
        m_browser.setStatus("Select a loaded plugin first.");
        return;
    }
    m_parameters.setPlugin(nullptr);
    for (int index = selected.size() - 1; index >= 0; --index) // from the back: the slot numbers of the others stay
    {
        const int row = selected[index];
        if (row >= 0 && row < static_cast<int>(slots.size()))
        {
            m_windows.erase(m_engine.getPlugin(slots[static_cast<size_t>(row)])); // the editor before its plugin
            m_engine.removeSlot(slots[static_cast<size_t>(row)]);
        }
    }
    m_loadedTable.deselectAllRows();
    slotsChanged();
    m_browser.setStatus("Unloaded.");
}

void HostMainComponent::showEditorsOfSelectedPlugins()
{
    const std::vector<int> slots = getPluginSlots();
    const juce::SparseSet<int> selected = m_loadedTable.getSelectedRows();
    if (selected.size() == 0)
    {
        m_browser.setStatus("Select a loaded plugin first.");
        return;
    }
    for (int index = 0; index < selected.size(); ++index)
    {
        const int row = selected[index];
        if (row >= 0 && row < static_cast<int>(slots.size()))
        {
            showEditorOfSlot(slots[static_cast<size_t>(row)]);
        }
    }
}

void HostMainComponent::showEditorOfSlot(int slotIndex)
{
    hosting::HostedPlugin* plugin = m_engine.getPlugin(slotIndex);
    if (plugin == nullptr)
    {
        return;
    }
    const auto existing = m_windows.find(plugin);
    if (existing != m_windows.end())
    {
        existing->second->toFront(true);
        return;
    }
    const juce::Component::SafePointer<HostMainComponent> self(this);
    m_windows[plugin] = std::make_unique<ui::PluginEditorWindow>(
        plugin->getInstance(), hosting::getDisplayName(plugin->getDescription()),
        [self, plugin]
        {
            // closing the window does not unload the plugin; delete the window after the close handler has returned
            juce::MessageManager::callAsync([self, plugin]
                                            {
                                                if (self != nullptr)
                                                {
                                                    self->m_windows.erase(plugin);
                                                }
                                            });
        });
}

void HostMainComponent::savePluginSet()
{
    m_chooser = std::make_unique<juce::FileChooser>("Save the plugin set", m_settings.getListFolder().getChildFile("plugins" + pluginlab::engine::kPluginSetExtension),
                                                    kPluginSetWildcard);
    m_chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                           [this](const juce::FileChooser& chooser)
                           {
                               juce::File file = chooser.getResult();
                               if (file == juce::File())
                               {
                                   return;
                               }
                               file = file.withFileExtension(pluginlab::engine::kPluginSetExtension);
                               m_settings.setListFolder(file.getParentDirectory());
                               if (pluginlab::engine::savePluginSet(m_engine, file))
                               {
                                   m_browser.setStatus("Saved the plugin set " + file.getFullPathName());
                                   return;
                               }
                               m_browser.setStatus("Cannot write " + file.getFullPathName());
                           });
}

void HostMainComponent::loadPluginSet()
{
    m_chooser = std::make_unique<juce::FileChooser>("Load a plugin set", m_settings.getListFolder(), kPluginSetWildcard);
    m_chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                           [this](const juce::FileChooser& chooser)
                           {
                               const juce::File file = chooser.getResult();
                               if (file == juce::File())
                               {
                                   return;
                               }
                               m_settings.setListFolder(file.getParentDirectory());
                               m_parameters.setPlugin(nullptr);
                               closeAllEditorWindows();
                               juce::String report;
                               const bool loaded = pluginlab::engine::loadPluginSet(m_engine, m_formatManager, file, report);
                               refresh();
                               slotsChanged();
                               if (! loaded)
                               {
                                   m_browser.setStatus(report);
                                   return;
                               }
                               juce::String text = "Loaded the plugin set " + file.getFileName() + ".";
                               if (report.isNotEmpty())
                               {
                                   text += " Not loaded: " + report.replace("\n", "; ");
                               }
                               m_browser.setStatus(text);
                           });
}
}
