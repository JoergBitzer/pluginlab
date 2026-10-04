#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

namespace pluginlab::ui
{
// A window of its own with the editor of one loaded plugin (a generic editor if the plugin has none). The window must be deleted
// before the plugin instance.
class PluginEditorWindow : public juce::DocumentWindow
{
public:
    PluginEditorWindow(juce::AudioPluginInstance& instance, const juce::String& title, std::function<void()> onCloseRequested);

    void closeButtonPressed() override;

private:
    std::function<void()> m_onCloseRequested;
};
}
