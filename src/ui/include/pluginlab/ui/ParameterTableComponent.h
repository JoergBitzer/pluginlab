#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "pluginlab/hosting/HostedPlugin.h"

namespace pluginlab::ui
{
class TextTableModel;

// The parameters of one hosted plugin: index, name, value as the plugin displays it, and a slider (normalised value 0 ... 1).
// Refreshes itself a few times per second, so changes made in the plugin's own editor show up. Only the rows that are visible are
// asked from the plugin (a plugin can have thousands of parameters; asking all of them several times per second blocked a plugin
// that shares locks between its parameter code and its audio code).
class ParameterTableComponent : public juce::Component, private juce::Timer
{
public:
    ParameterTableComponent();
    ~ParameterTableComponent() override;

    // nullptr: show nothing. The plugin must stay alive until it is replaced here or this component is deleted.
    void setPlugin(hosting::HostedPlugin* plugin);

    void resized() override;

private:
    class SliderModel;

    void timerCallback() override;
    hosting::HostedPlugin* m_plugin = nullptr;
    std::unique_ptr<SliderModel> m_model;
    juce::TableListBox m_table;
};
}
