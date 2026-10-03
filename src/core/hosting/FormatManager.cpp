#include "pluginlab/hosting/FormatManager.h"

namespace pluginlab::hosting
{
void addHeadlessFormats(juce::AudioPluginFormatManager& formatManager)
{
    formatManager.addFormat(std::make_unique<juce::VST3PluginFormatHeadless>());
#if PLUGINLAB_WITH_VST2
    formatManager.addFormat(std::make_unique<juce::VSTPluginFormatHeadless>());
#endif
}

bool isVst2Supported()
{
#if PLUGINLAB_WITH_VST2
    return true;
#else
    return false;
#endif
}
}
