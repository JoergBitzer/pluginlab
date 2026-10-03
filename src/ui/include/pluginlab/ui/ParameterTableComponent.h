#pragma once

#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "pluginlab/hosting/HostedPlugin.h"

namespace pluginlab::ui
{
class TextTableModel;

// The parameters of one hosted plugin: index, name, value as the plugin displays it, and a slider (normalised value 0 ... 1).
// Refreshes itself a few times per second, so changes made in the plugin's own editor show up.
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
    void refresh();

    hosting::HostedPlugin* m_plugin = nullptr;
    std::vector<hosting::ParameterInfo> m_rows;
    std::unique_ptr<SliderModel> m_model;
    juce::TableListBox m_table;
};
}
