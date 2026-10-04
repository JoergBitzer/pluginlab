#include "pluginlab/ui/PluginEditorWindow.h"

#include "pluginlab/ui/PluginEditorFactory.h"

namespace pluginlab::ui
{
PluginEditorWindow::PluginEditorWindow(juce::AudioPluginInstance& instance,
                                       const juce::String& title,
                                       std::function<void()> onCloseRequested)
    : juce::DocumentWindow(title,
                           juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                           juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
      m_onCloseRequested(std::move(onCloseRequested))
{
    setUsingNativeTitleBar(true);
    juce::AudioProcessorEditor* editor = createEditorFor(instance);
    const bool editorCanResize = editor->isResizable();
    setContentOwned(editor, true);
    setResizable(editorCanResize, false);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

void PluginEditorWindow::closeButtonPressed()
{
    m_onCloseRequested();
}
}
