#include "HostMainComponent.h"

#include "pluginlab/ui/PluginEditorWindow.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/ui/GuiFormats.h"
#include "pluginlab/ui/TextTableModel.h"

namespace pluginlab::host
{
namespace
{
constexpr int kWindowWidth = 1100;
constexpr int kWindowHeight = 760;
constexpr int kMargin = 8;
constexpr int kButtonHeight = 28;
constexpr int kRowHeight = 24;
constexpr int kHeaderHeight = 24;
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kUnloadButtonWidth = 100;
constexpr int kEditorButtonWidth = 120;
constexpr int kLoadedColumnName = 1;
constexpr int kLoadedColumnFormat = 2;
constexpr int kLoadedColumnParameters = 3;
constexpr int kLoadedWidthName = 200;
constexpr int kLoadedWidthFormat = 60;
constexpr int kLoadedWidthParameters = 90;
constexpr int kLoadedListWidth = 380;
}

HostMainComponent::HostMainComponent(const StartupOptions& options)
    : m_pluginNameToLoad(options.pluginNameToLoad), m_showEditorAfterLoad(options.pluginNameToLoad.isNotEmpty())
{
    ui::addGuiFormats(m_formatManager);

    m_loadedModel = std::make_unique<ui::TextTableModel>(
        [this] { return static_cast<int>(m_loadedPlugins.size()); },
        [this](int row, int column)
        {
            const hosting::HostedPlugin& plugin = *m_loadedPlugins[static_cast<size_t>(row)]->plugin;
            if (column == kLoadedColumnName)
            {
                return hosting::getDisplayName(plugin.getDescription());
            }
            if (column == kLoadedColumnFormat)
            {
                return plugin.getDescription().pluginFormatName;
            }
            if (column == kLoadedColumnParameters)
            {
                return juce::String(static_cast<int>(plugin.getParameters().size()));
            }
            return juce::String();
        },
        [this] { selectionChanged(); });
    m_loadedTable.setModel(m_loadedModel.get());
    m_loadedTable.setRowHeight(kRowHeight);
    m_loadedTable.setHeaderHeight(kHeaderHeight);
    m_loadedTable.setMultipleSelectionEnabled(false);
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
    m_unloadButton.onClick = [this] { unloadSelectedPlugin(); };
    m_editorButton.onClick = [this] { showEditorOfSelectedPlugin(); };

    for (juce::Component* component : std::initializer_list<juce::Component*>{
             &m_browser, &m_unloadButton, &m_editorButton, &m_loadedTable, &m_parameters})
    {
        addAndMakeVisible(component);
    }

    setSize(kWindowWidth, kWindowHeight);

    if (options.scanFolder != juce::File())
    {
        m_browser.scanFolder(options.scanFolder);
    }
}

HostMainComponent::~HostMainComponent()
{
    m_parameters.setPlugin(nullptr);
    m_loadedTable.setModel(nullptr);
    for (std::unique_ptr<LoadedPlugin>& loaded : m_loadedPlugins)
    {
        loaded->window.reset(); // the editor must go before the plugin
    }
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
    area.removeFromTop(kMargin);

    m_loadedTable.setBounds(area.removeFromLeft(kLoadedListWidth));
    area.removeFromLeft(kMargin);
    m_parameters.setBounds(area);
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

void HostMainComponent::selectionChanged()
{
    LoadedPlugin* loaded = getSelectedLoadedPlugin();
    if (loaded == nullptr)
    {
        m_parameters.setPlugin(nullptr);
        return;
    }
    m_parameters.setPlugin(loaded->plugin.get());
}

void HostMainComponent::loadPlugin(const juce::PluginDescription& description)
{
    juce::String error;
    std::unique_ptr<hosting::HostedPlugin> plugin =
        hosting::HostedPlugin::load(m_formatManager, description, kSampleRate, kBlockSize, error);
    if (plugin == nullptr)
    {
        m_browser.setStatus("Cannot load " + hosting::getDisplayName(description) + ": " + error);
        return;
    }

    auto loaded = std::make_unique<LoadedPlugin>();
    loaded->plugin = std::move(plugin);
    LoadedPlugin& loadedReference = *loaded;
    m_loadedPlugins.push_back(std::move(loaded));
    m_loadedTable.updateContent();
    m_loadedTable.selectRow(static_cast<int>(m_loadedPlugins.size()) - 1);
    if (m_showEditorAfterLoad)
    {
        m_showEditorAfterLoad = false; // only for --load (manual tests, screenshots)
        showEditor(loadedReference);
    }
    m_browser.setStatus("Loaded " + hosting::getDisplayName(description) + ".");
}

void HostMainComponent::unloadSelectedPlugin()
{
    const int row = m_loadedTable.getSelectedRow();
    if (getSelectedLoadedPlugin() == nullptr)
    {
        m_browser.setStatus("Select a loaded plugin first.");
        return;
    }
    m_parameters.setPlugin(nullptr);
    m_loadedPlugins[static_cast<size_t>(row)]->window.reset(); // the editor before the plugin
    m_loadedPlugins.erase(m_loadedPlugins.begin() + row);
    m_loadedTable.updateContent();
    m_loadedTable.deselectAllRows();
    m_browser.setStatus("Unloaded.");
}

void HostMainComponent::showEditorOfSelectedPlugin()
{
    LoadedPlugin* loaded = getSelectedLoadedPlugin();
    if (loaded == nullptr)
    {
        m_browser.setStatus("Select a loaded plugin first.");
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
    loaded.window = std::make_unique<ui::PluginEditorWindow>(
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
}
