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

// knobs that show something else than they do (calibration, W8.6): frequency 5 % high, "Q" a bandwidth in octaves, gain half the dB shown
inline juce::File getEqKnobFaultPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqKnobs.vst3");
}

// two RBJ peaks (interactions, W8.7): in series (they add in dB) and in parallel (they do not add where they overlap)
inline juce::File getEqTwoBandsPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqTwoBands.vst3");
}

inline juce::File getEqParallelPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqParallel.vst3");
}

// the frequency knob ends at 5 kHz (attribution of the matcher, W9.5)
inline juce::File getEqLimitedPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqLimited.vst3");
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

inline juce::File getGainRampPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestGainRamp.vst3");
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

inline juce::File getTremoloPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestTremolo.vst3");
}

// real-time behaviour (W5c): the gain reaches the audio through a 100 ms timer; +6 dB offline; a tremolo on the system clock
inline juce::File getTempoDelayPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestTempoDelay.vst3");
}

inline juce::File getTimerGainPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestTimerGain.vst3");
}

inline juce::File getOfflineSwitchPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestOfflineSwitch.vst3");
}

inline juce::File getWallClockPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestWallClock.vst3");
}

inline juce::File getEqSwitchPlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqSwitch.vst3");
}

// an "analogue" EQ: a drive that makes it non-linear, an output level, a display switch that does nothing (W8.4)
inline juce::File getEqDrivePlugin()
{
    return getTestPluginFolder().getChildFile("PluginLabTestEqDrive.vst3");
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

// The reference plugins of W6.4 (plugins/reference), built into their own folder
namespace testpaths
{
inline juce::File getReferencePlugin(const juce::String& name)
{
    return juce::File(PLUGINLAB_REFERENCE_PLUGIN_DIR).getChildFile("PluginLabReference" + name + ".vst3");
}
}
