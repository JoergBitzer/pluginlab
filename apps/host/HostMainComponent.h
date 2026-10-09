#pragma once

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "HostSettings.h"
#include "pluginlab/engine/MeasurementEngine.h"
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
// Optional actions at startup (command line), for manual tests and screenshots.
struct StartupOptions
{
    juce::File scanFolder;
    juce::String pluginNameToLoad; // loaded after the scan has finished
    juce::File compareAudioFile;   // --compare: opens the Compare page with this file and (compareSlotPlugin) a plugin slot
    juce::File compareSlotPlugin;
    juce::File developerPlugin;    // --developer <plugin file> [--view]: loads the plugin, opens the Developer page and makes the report
    bool developerView = false;    // ... and shows it when it is ready
    bool developerGui = false;     // --gui: the GUI review instead of the report (W5d.7)
    double playSeconds = 0.0;      // --play <seconds> <report file>: plays on the Compare page, writes the report and quits
    juce::File playReportFile;

    // true if the program was started for a manual test: it then does not ask to restore the last session
    bool isManualTest() const
    {
        return scanFolder != juce::File() || pluginNameToLoad.isNotEmpty() || compareAudioFile != juce::File() || developerPlugin != juce::File();
    }
};

// The page "Plugins": the plugin browser (scan, validation, load of all selected plugins), the loaded plugins and the parameters of
// the selected one. The loaded plugins are the slots of the engine: a plugin that is loaded here is in the comparison on the page
// "Compare", a plugin that is unloaded here is gone from it. Every loaded plugin can have an editor window of its own.
class HostMainComponent : public juce::Component
{
public:
    HostMainComponent(pluginlab::engine::MeasurementEngine& engine,
                      juce::AudioPluginFormatManager& formatManager,
                      HostSettings& settings,
                      const StartupOptions& options = StartupOptions());
    ~HostMainComponent() override;

    // Told after plugins were loaded or unloaded (the Compare page shows the slots, the session is kept)
    std::function<void()> onSlotsChanged;

    void resized() override;

    void loadPlugin(const juce::PluginDescription& description);

    // The list of the loaded plugins and the parameters follow the engine (after a plugin set was loaded, for example)
    void refresh();

    // The editor windows must go before their plugins: call this before the slots of the engine are changed from outside.
    void closeAllEditorWindows();

    // The editor window of the plugin in this slot of the engine (the page "Compare" asks for it)
    void showEditorOfSlot(int slotIndex);

private:
    std::vector<int> getPluginSlots() const;
    void unloadSelectedPlugins();
    void showEditorsOfSelectedPlugins();
    void selectionChanged();
    void savePluginSet();
    void loadPluginSet();
    void slotsChanged();

    pluginlab::engine::MeasurementEngine& m_engine;
    juce::AudioPluginFormatManager& m_formatManager;
    HostSettings& m_settings;
    std::map<hosting::HostedPlugin*, std::unique_ptr<ui::PluginEditorWindow>> m_windows;
    std::unique_ptr<juce::FileChooser> m_chooser;
    juce::String m_pluginNameToLoad;
    bool m_showEditorAfterLoad; // true if the plugin was named on the command line (--load): then its editor opens, for manual tests

    ui::PluginBrowserComponent m_browser;
    juce::TextButton m_unloadButton{"Unload"};
    juce::TextButton m_editorButton{"Show editor"};
    juce::TextButton m_saveSetButton{"Save plugin set..."};
    juce::TextButton m_loadSetButton{"Load plugin set..."};
    std::unique_ptr<ui::TextTableModel> m_loadedModel;
    juce::TableListBox m_loadedTable;
    ui::ParameterTableComponent m_parameters;
};
}
