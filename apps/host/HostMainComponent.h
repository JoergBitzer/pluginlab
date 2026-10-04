#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/ui/ParameterTableComponent.h"
#include "pluginlab/ui/PluginBrowserComponent.h"

namespace pluginlab::ui
{
class TextTableModel;
class PluginEditorWindow;
}

namespace pluginlab::host
{
// Optional actions at startup (command line --scan <folder> and --load <plugin name>), for manual tests and screenshots.
struct StartupOptions
{
    juce::File scanFolder;
    juce::String pluginNameToLoad; // loaded after the scan has finished
};

// The main window of the host: the plugin browser (scan, validation, load), the loaded plugins and the parameters of the
// selected one. Every loaded plugin has its own editor window.
class HostMainComponent : public juce::Component
{
public:
    explicit HostMainComponent(const StartupOptions& options = StartupOptions());
    ~HostMainComponent() override;

    void resized() override;

private:
    struct LoadedPlugin
    {
        std::unique_ptr<hosting::HostedPlugin> plugin;
        std::unique_ptr<ui::PluginEditorWindow> window;
    };

    void loadPlugin(const juce::PluginDescription& description);
    void unloadSelectedPlugin();
    void showEditorOfSelectedPlugin();
    void showEditor(LoadedPlugin& loaded);
    void selectionChanged();
    LoadedPlugin* getSelectedLoadedPlugin();

    juce::AudioPluginFormatManager m_formatManager;
    std::vector<std::unique_ptr<LoadedPlugin>> m_loadedPlugins;
    juce::String m_pluginNameToLoad;
    bool m_showEditorAfterLoad; // true if the plugin was named on the command line (--load): then its editor opens, for manual tests

    ui::PluginBrowserComponent m_browser;
    juce::TextButton m_unloadButton{"Unload"};
    juce::TextButton m_editorButton{"Show editor"};
    std::unique_ptr<ui::TextTableModel> m_loadedModel;
    juce::TableListBox m_loadedTable;
    ui::ParameterTableComponent m_parameters;
};
}
