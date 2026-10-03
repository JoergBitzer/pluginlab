#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/ui/ParameterTableComponent.h"
#include "pluginlab/ui/PluginBrowserComponent.h"

class LoaderProcessor;

// The editor of the loader plugin: the plugin browser on top (scan, validate, load), below the parameters of the loaded plugin and
// the editor of the loaded plugin itself.
class LoaderEditor : public juce::AudioProcessorEditor
{
public:
    explicit LoaderEditor(LoaderProcessor& loaderProcessor);
    ~LoaderEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // true while the editor of a loaded plugin is shown (for the tests)
    bool isShowingHostedEditor() const;

private:
    void dropHostedEditor();
    void showHostedEditor();

    LoaderProcessor& m_processor;
    pluginlab::ui::PluginBrowserComponent m_browser;
    pluginlab::ui::ParameterTableComponent m_parameters;
    juce::Viewport m_editorViewport;
    std::unique_ptr<juce::AudioProcessorEditor> m_hostedEditor;
};
