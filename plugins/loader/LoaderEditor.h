#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/ui/ParameterTableComponent.h"
#include "pluginlab/ui/PluginEditorWindow.h"
#include "pluginlab/ui/PluginBrowserComponent.h"

class LoaderProcessor;

// The editor of the loader plugin: the plugin browser on top (scan, validate, load) and below it the parameters of the loaded
// plugin. The editor of the loaded plugin opens in a window of its own (most plugin editors are too big for a corner of this one).
class LoaderEditor : public juce::AudioProcessorEditor
{
public:
    explicit LoaderEditor(LoaderProcessor& loaderProcessor);
    ~LoaderEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // true while the window with the editor of a loaded plugin is open (for the tests)
    bool isShowingHostedEditor() const;

private:
    void dropHostedEditor();
    void showHostedEditor();

    LoaderProcessor& m_processor;
    pluginlab::ui::PluginBrowserComponent m_browser;
    pluginlab::ui::ParameterTableComponent m_parameters;
    juce::TextButton m_showEditorButton{"Show plugin editor"};
    juce::TextButton m_unloadButton{"Unload plugin"};
    std::unique_ptr<pluginlab::ui::PluginEditorWindow> m_hostedWindow;
};
