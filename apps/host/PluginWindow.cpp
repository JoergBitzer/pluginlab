#include "PluginWindow.h"

namespace pluginlab::host
{
namespace
{
juce::AudioProcessorEditor* createEditorFor(juce::AudioPluginInstance& instance)
{
    juce::AudioProcessorEditor* editor = nullptr;
    if (instance.hasEditor())
    {
        editor = instance.createEditorAndMakeActive();
    }
    if (editor == nullptr)
    {
        editor = new juce::GenericAudioProcessorEditor(instance);
    }
    return editor;
}
}

PluginWindow::PluginWindow(juce::AudioPluginInstance& instance, const juce::String& title, std::function<void()> onCloseRequested)
    : juce::DocumentWindow(title,
                           juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                           juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
      m_onCloseRequested(std::move(onCloseRequested))
{
    setUsingNativeTitleBar(true);
    setContentOwned(createEditorFor(instance), true);
    setResizable(false, false);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

void PluginWindow::closeButtonPressed()
{
    m_onCloseRequested();
}
}
