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

juce::String makeTitleText()
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
    m_processor.onHostedPluginChanged = [this](bool openEditor) { showHostedPlugin(openEditor); };

    // a component of its own (not painted in paint()): visible whatever the host does with the background
    m_titleLabel.setText(makeTitleText(), juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(kTitleFontHeight, juce::Font::bold));
    m_titleLabel.setColour(juce::Label::textColourId,
                           getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).contrasting());
    addAndMakeVisible(m_titleLabel);
    addAndMakeVisible(m_browser);
    addAndMakeVisible(m_parameters);
    addAndMakeVisible(m_showEditorButton);
    addAndMakeVisible(m_unloadButton);
    m_showEditorButton.onClick = [this] { openHostedEditorWindow(); };
    m_unloadButton.onClick = [this] { m_processor.unloadPlugin(); };

    setSize(kEditorWidth, kEditorHeight);
    showHostedPlugin(false); // a plugin may already be loaded (state of the session): its window opens only on request

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
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

bool LoaderEditor::isShowingHostedEditor() const
{
    return m_hostedWindow != nullptr;
}

void LoaderEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);
    m_titleLabel.setBounds(area.removeFromTop(kTitleHeight));
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

void LoaderEditor::openHostedEditorWindow()
{
    showHostedPlugin(true);
}

void LoaderEditor::showHostedPlugin(bool openWindow)
{
    pluginlab::hosting::HostedPlugin* hosted = m_processor.getHostedPlugin();
    if (hosted == nullptr)
    {
        return;
    }
    m_parameters.setPlugin(hosted);
    if (! openWindow)
    {
        return;
    }
    if (m_hostedWindow != nullptr)
    {
        m_hostedWindow->toFront(true);
        return;
    }
    const juce::Component::SafePointer<LoaderEditor> self(this);
    m_hostedWindow = std::make_unique<pluginlab::ui::PluginEditorWindow>(
        hosted->getInstance(), makeTitleText() + ": " + pluginlab::hosting::getDisplayName(hosted->getDescription()),
        [self]
        {
            // the close handler runs inside the window: delete it afterwards
            juce::MessageManager::callAsync([self]
                                            {
                                                if (self != nullptr)
                                                {
                                                    self->m_hostedWindow.reset();
                                                }
                                            });
        });
}
