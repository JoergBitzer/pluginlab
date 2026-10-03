#include "PluginWindow.h"

#include "pluginlab/ui/PluginEditorFactory.h"

namespace pluginlab::host
{
PluginWindow::PluginWindow(juce::AudioPluginInstance& instance, const juce::String& title, std::function<void()> onCloseRequested)
    : juce::DocumentWindow(title,
                           juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                           juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
      m_onCloseRequested(std::move(onCloseRequested))
{
    setUsingNativeTitleBar(true);
    setContentOwned(ui::createEditorFor(instance), true);
    setResizable(false, false);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

void PluginWindow::closeButtonPressed()
{
    m_onCloseRequested();
}
}
