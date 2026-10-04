#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ComparePanel.h"
#include "HostMainComponent.h"

namespace pluginlab::host
{
// The content of the main window: the page "Plugins" (browser, loaded plugins, parameters) and the page "Compare" (files, slots, playback).
class HostShell : public juce::TabbedComponent, private juce::Timer
{
public:
    explicit HostShell(const StartupOptions& options);
    ~HostShell() override;

private:
    void timerCallback() override;
    void finishPlayTest();

    StartupOptions m_options;
    HostMainComponent* m_pluginsPage = nullptr; // owned by the tab
    ComparePanel* m_comparePage = nullptr;      // owned by the tab
};
}
