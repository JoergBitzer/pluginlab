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

inline juce::File getMonoPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestMono.vst3");
}

// delays the audio by 64 samples and reports 64
inline juce::File getLatencyPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestLatency.vst3");
}

// delays the audio by 100 samples and reports 0
inline juce::File getLatencyLiarPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestLatencyLie.vst3");
}

// RBJ peaking EQs: correct, designed for 44.1 kHz whatever the host rate, prepareToPlay resets to hard-coded values
inline juce::File getEqPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEq.vst3");
}

inline juce::File getEqFsFaultPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqFs.vst3");
}

inline juce::File getEqPrepareFaultPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqPrepare.vst3");
}

inline juce::File getLinearPhasePlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestLinearPhase.vst3");
}

inline juce::File getBlockSmoothingPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestBlockSmoothing.vst3");
}

inline juce::File getBlockFaultPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestBlockFault.vst3");
}

inline juce::File getCrossFeedPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestCrossFeed.vst3");
}

inline juce::File getWidthPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestWidth.vst3");
}

inline juce::File getSideChainPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestSideChain.vst3");
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
