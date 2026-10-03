#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginScanResult.h"

namespace pluginlab::host
{
class PluginWindow;
class PluginScanThread;
class TextTableModel;
class ParameterTableModel;

// Optional actions at startup (command line --scan <folder> and --load <plugin name>), for manual tests and screenshots.
struct StartupOptions
{
    juce::File scanFolder;
    juce::String pluginNameToLoad; // loaded after the scan has finished
};

// The main window of the host: the list of the scanned plugins, the loaded plugins and the parameters of the selected one.
class HostMainComponent : public juce::Component, private juce::Timer
{
public:
    explicit HostMainComponent(const StartupOptions& options = StartupOptions());
    ~HostMainComponent() override;

    void resized() override;

    // called by the scan thread (on the message thread)
    void addScanResult(const hosting::PluginScanResult& result);
    void scanFinished();

private:
    struct ScanRow
    {
        juce::File file;
        hosting::ScanStatus status = hosting::ScanStatus::ScannerFailed;
        juce::String message;
        juce::PluginDescription description;
        bool hasDescription = false;
    };

    struct LoadedPlugin
    {
        std::unique_ptr<hosting::HostedPlugin> plugin;
        std::unique_ptr<PluginWindow> window;
    };

    void timerCallback() override;

    void startScan(const juce::FileSearchPath& folders);
    void chooseFolderToScan();
    void loadSelectedPlugin();
    void unloadSelectedPlugin();
    void showEditorOfSelectedPlugin();
    void showEditor(LoadedPlugin& loaded);
    void refreshParameters();
    void setStatus(const juce::String& text);

    juce::String getScanCellText(int row, int columnId) const;
    juce::String getLoadedCellText(int row, int columnId) const;
    LoadedPlugin* getSelectedLoadedPlugin();

    juce::String m_pluginNameToLoad;

    juce::AudioPluginFormatManager m_formatManager;
    std::vector<ScanRow> m_scanRows;
    std::vector<std::unique_ptr<LoadedPlugin>> m_loadedPlugins;
    std::vector<hosting::ParameterInfo> m_parameterRows;

    juce::TextButton m_scanButton{"Scan standard folders"};
    juce::TextButton m_addFolderButton{"Add folder..."};
    juce::TextButton m_loadButton{"Load"};
    juce::TextButton m_unloadButton{"Unload"};
    juce::TextButton m_editorButton{"Show editor"};
    juce::Label m_statusLabel;

    std::unique_ptr<TextTableModel> m_scanModel;
    std::unique_ptr<TextTableModel> m_loadedModel;
    std::unique_ptr<ParameterTableModel> m_parameterModel;
    juce::TableListBox m_scanTable;
    juce::TableListBox m_loadedTable;
    juce::TableListBox m_parameterTable;

    std::unique_ptr<PluginScanThread> m_scanThread;
    std::unique_ptr<juce::FileChooser> m_folderChooser;
};
}
