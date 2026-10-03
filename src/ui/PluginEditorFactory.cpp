#include "pluginlab/ui/PluginEditorFactory.h"

namespace pluginlab::ui
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
