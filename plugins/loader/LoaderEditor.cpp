#include "LoaderEditor.h"

#include "LoaderProcessor.h"
#include "pluginlab/PluginLabVersion.h"
#include "pluginlab/hosting/PluginDisplayName.h"

namespace
{
constexpr int kEditorWidth = 900;
constexpr int kEditorHeight = 724;
constexpr int kMargin = 8;
constexpr int kBrowserHeight = 260;
constexpr int kButtonRowHeight = 28;
constexpr int kTitleHeight = 24;
constexpr float kTitleFontHeight = 16.0f;
const juce::String kProductName = "PluginLab Loader";

juce::String getTitle()
{
    return kProductName + " " + juce::String(pluginlab::getVersionString());
}
constexpr int kButtonWidth = 150;
// development: a folder that the browser scans as soon as the editor opens (for screenshots and for checking that the plugin finds its scanner)
const juce::String kScanOnOpenVariable = "PLUGINLAB_LOADER_SCAN";
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
    addAndMakeVisible(m_showEditorButton);
    addAndMakeVisible(m_unloadButton);
    m_showEditorButton.onClick = [this] { showHostedEditor(); };
    m_unloadButton.onClick = [this] { m_processor.unloadPlugin(); };

    setSize(kEditorWidth, kEditorHeight);
    showHostedEditor(); // a plugin may already be loaded (state of the session)

    const juce::String scanOnOpen = juce::SystemStats::getEnvironmentVariable(kScanOnOpenVariable, {});
    if (scanOnOpen.isNotEmpty())
    {
        m_browser.scanFolder(juce::File(scanOnOpen));
    }
}

LoaderEditor::~LoaderEditor()
{
    m_processor.onBeforeHostedPluginChanged = nullptr;
    m_processor.onHostedPluginChanged = nullptr;
    dropHostedEditor(); // the editor of the loaded plugin must go before this editor
}

void LoaderEditor::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    g.fillAll(background);
    g.setColour(background.contrasting()); // the text colour of the look and feel can be unreadable on this background in a host
    g.setFont(juce::FontOptions(kTitleFontHeight, juce::Font::bold));
    g.drawText(getTitle(), getLocalBounds().reduced(kMargin).removeFromTop(kTitleHeight), juce::Justification::centredLeft);
}

bool LoaderEditor::isShowingHostedEditor() const
{
    return m_hostedWindow != nullptr;
}

void LoaderEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);
    area.removeFromTop(kTitleHeight);
    m_browser.setBounds(area.removeFromTop(kBrowserHeight));
    area.removeFromTop(kMargin);
    juce::Rectangle<int> buttonRow = area.removeFromTop(kButtonRowHeight);
    m_showEditorButton.setBounds(buttonRow.removeFromLeft(kButtonWidth));
    buttonRow.removeFromLeft(kMargin);
    m_unloadButton.setBounds(buttonRow.removeFromLeft(kButtonWidth));
    area.removeFromTop(kMargin);
    m_parameters.setBounds(area);
}

void LoaderEditor::dropHostedEditor()
{
    m_parameters.setPlugin(nullptr);
    m_hostedWindow.reset(); // the window must go before the plugin instance
}

void LoaderEditor::showHostedEditor()
{
    pluginlab::hosting::HostedPlugin* hosted = m_processor.getHostedPlugin();
    if (hosted == nullptr)
    {
        return;
    }
    m_parameters.setPlugin(hosted);
    if (m_hostedWindow != nullptr)
    {
        m_hostedWindow->toFront(true);
        return;
    }
    m_hostedWindow = std::make_unique<pluginlab::ui::PluginEditorWindow>(
        hosted->getInstance(), getTitle() + ": " + pluginlab::hosting::getDisplayName(hosted->getDescription()),
        [this]
        {
            // the close handler runs inside the window: delete it afterwards
            juce::MessageManager::callAsync([safe = juce::Component::SafePointer<LoaderEditor>(this)]
                                            {
                                                if (safe != nullptr)
                                                {
                                                    safe->m_hostedWindow.reset();
                                                }
                                            });
        });
}
