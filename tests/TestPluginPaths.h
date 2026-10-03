#pragma once

#include <juce_core/juce_core.h>

// Where CMake put the test plugins and the scanner (compile definitions of the test target).
namespace testpaths
{
inline juce::File getTestPluginFolder()
{
    return juce::File(PLUGINLAB_TEST_PLUGIN_DIR);
}

inline juce::File getGainPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestGain.vst3");
}

inline juce::File getCrashPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestCrash.vst3");
}

inline juce::File getNotAPluginFile()
{
    return getTestPluginFolder().getChildFile("NotAPlugin.vst3");
}

inline juce::File getScannerExecutable()
{
    return juce::File(PLUGINLAB_SCANNER_EXECUTABLE);
}
}
