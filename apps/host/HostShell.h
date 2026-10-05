#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ComparePanel.h"
#include "DeveloperPanel.h"
#include "HostMainComponent.h"
#include "HostSettings.h"
#include "pluginlab/engine/MeasurementEngine.h"

namespace pluginlab::host
{
// The content of the main window: the page "Plugins" (browser, loaded plugins, parameters) and the page "Compare" (files, slots,
// playback). Owns the engine that both pages work on: a plugin loaded on the first page is a slot on the second.
// Keeps the lists of the session (the audio files, the plugins with their settings) and offers to load them at the next start.
class HostShell : public juce::TabbedComponent, private juce::Timer
{
public:
    explicit HostShell(const StartupOptions& options);
    ~HostShell() override;

private:
    void timerCallback() override;
    void finishPlayTest();
    void changeEngineRate(double sampleRate);
    void slotsChanged();
    void filesChanged();
    void saveSession();
    void askToRestoreSession();
    void askForPluginSet(int numberOfPlugins);
    void restorePluginSet();
    void endRestore();

    StartupOptions m_options;
    HostSettings m_settings;
    juce::AudioPluginFormatManager m_formatManager;
    pluginlab::engine::MeasurementEngine m_engine;
    bool m_sessionIsSaved = false; // switched on after the questions of the start (an answer "no" must not overwrite the old session at once)
    HostMainComponent* m_pluginsPage = nullptr; // owned by the tab
    ComparePanel* m_comparePage = nullptr;      // owned by the tab
    DeveloperPanel* m_developerPage = nullptr;  // owned by the tab
};
}
