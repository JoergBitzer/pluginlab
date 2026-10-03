#include "LoaderEditor.h"

#include "LoaderProcessor.h"
#include "pluginlab/ui/PluginEditorFactory.h"

namespace
{
constexpr int kEditorWidth = 1000;
constexpr int kEditorHeight = 760;
constexpr int kMargin = 8;
constexpr int kBrowserHeight = 260;
constexpr int kParametersWidth = 380;
}

LoaderEditor::LoaderEditor(LoaderProcessor& loaderProcessor)
    : juce::AudioProcessorEditor(&loaderProcessor), m_processor(loaderProcessor)
{
    m_browser.onPluginChosen = [this](const juce::PluginDescription& description)
    {
        juce::String error;
        if (! m_processor.loadPlugin(description, error))
        {
            m_browser.setStatus("Cannot load " + description.name + ": " + error);
            return;
        }
        m_browser.setStatus("Loaded " + description.name + ".");
    };

    m_processor.onBeforeHostedPluginChanged = [this] { dropHostedEditor(); };
    m_processor.onHostedPluginChanged = [this] { showHostedEditor(); };

    addAndMakeVisible(m_browser);
    addAndMakeVisible(m_parameters);
    addAndMakeVisible(m_editorViewport);

    setSize(kEditorWidth, kEditorHeight);
    showHostedEditor(); // a plugin may already be loaded (state of the session)
}

LoaderEditor::~LoaderEditor()
{
    m_processor.onBeforeHostedPluginChanged = nullptr;
    m_processor.onHostedPluginChanged = nullptr;
    dropHostedEditor(); // the editor of the loaded plugin must go before this editor
}

void LoaderEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

bool LoaderEditor::isShowingHostedEditor() const
{
    return m_hostedEditor != nullptr;
}

void LoaderEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);
    m_browser.setBounds(area.removeFromTop(kBrowserHeight));
    area.removeFromTop(kMargin);
    m_parameters.setBounds(area.removeFromLeft(kParametersWidth));
    area.removeFromLeft(kMargin);
    m_editorViewport.setBounds(area);
}

void LoaderEditor::dropHostedEditor()
{
    m_parameters.setPlugin(nullptr);
    m_editorViewport.setViewedComponent(nullptr, false);
    m_hostedEditor.reset();
}

void LoaderEditor::showHostedEditor()
{
    pluginlab::hosting::HostedPlugin* hosted = m_processor.getHostedPlugin();
    if (hosted == nullptr)
    {
        return;
    }
    m_parameters.setPlugin(hosted);
    m_hostedEditor.reset(pluginlab::ui::createEditorFor(hosted->getInstance()));
    m_editorViewport.setViewedComponent(m_hostedEditor.get(), false);
}
