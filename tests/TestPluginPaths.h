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

// The VST2 version of the gain plugin: .so (Linux, may start with "lib"), .dll (Windows) or .vst bundle (macOS); an invalid File if
// there is none (a build without VST2).
inline juce::File getGainPluginVst2()
{
    const juce::Array<juce::File> candidates = getTestPluginFolder().findChildFiles(juce::File::findFilesAndDirectories, false, "*PluginLabTestGain*");
    for (const juce::File& candidate : candidates)
    {
        const bool isVst3 = candidate.hasFileExtension(".vst3");
        const bool isVst2 = candidate.hasFileExtension(".so") || candidate.hasFileExtension(".dll") || candidate.hasFileExtension(".vst");
        if (isVst2 && ! isVst3)
        {
            return candidate;
        }
    }
    return {};
}

inline juce::File getCrashPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestCrash.vst3");
}

inline juce::File getCrashInProcessPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestCrashProcess.vst3");
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
