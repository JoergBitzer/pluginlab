#include "pluginlab/ui/GuiFormats.h"

namespace pluginlab::ui
{
void addGuiFormats(juce::AudioPluginFormatManager& formatManager)
{
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormat>());
#if PLUGINLAB_WITH_VST2
    formatManager.addFormat(std::make_unique<juce::VSTPluginFormat>());
#endif
}
}
