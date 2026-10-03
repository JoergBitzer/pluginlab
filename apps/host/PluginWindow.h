#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

namespace pluginlab::host
{
// A window with the editor of one loaded plugin (a generic editor if the plugin has none). The window must be deleted
// before the plugin instance.
class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow(juce::AudioPluginInstance& instance, const juce::String& title, std::function<void()> onCloseRequested);

    void closeButtonPressed() override;

private:
    std::function<void()> m_onCloseRequested;
};
}
