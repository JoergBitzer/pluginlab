#include <iostream>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_events/juce_events.h>

#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginProbe.h"
#include "pluginlab/hosting/PluginScanXml.h"
#include "pluginlab/hosting/PluginScanner.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace
{
constexpr int kExpectedArgumentCount = 3;      // program, plugin file, result file
constexpr int kExpectedProbeArgumentCount = 5; // program, --probe, plugin file, plugin identifier, result file
constexpr int kIndexOfFirstArgument = 1;
constexpr int kIndexOfProbeFile = 2;
constexpr int kIndexOfProbeIdentifier = 3;
constexpr int kIndexOfProbeResult = 4;
constexpr int kExitProbeFailed = 4;
constexpr int kExitOk = 0;
constexpr int kExitWrongArguments = 2;
constexpr int kExitCannotWriteResult = 3;

// A crashing plugin must end this process quietly: no Windows error dialog that would wait for a click.
void silenceCrashDialogs()
{
#if JUCE_WINDOWS
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif
}
}

// Usage: PluginLabScanner <plugin file> <result file (XML)>
//        PluginLabScanner --probe <plugin file> <plugin identifier> <result file (text)>   (quick check of one plugin)
int main(int argc, char* argv[])
{
    const bool isProbe = argc == kExpectedProbeArgumentCount && pluginlab::hosting::getProbeArgument() == argv[kIndexOfFirstArgument];
    if (isProbe)
    {
        silenceCrashDialogs();
        const juce::ScopedJuceInitialiser_GUI juceInitialiser;
        juce::AudioPluginFormatManager probeFormats;
        pluginlab::hosting::addHeadlessFormats(probeFormats);
        const juce::File probeFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[kIndexOfProbeFile]);
        const juce::File probeResult = juce::File::getCurrentWorkingDirectory().getChildFile(argv[kIndexOfProbeResult]);
        const bool passed = pluginlab::hosting::probePluginInProcess(probeFormats, probeFile, argv[kIndexOfProbeIdentifier], probeResult);
        if (passed)
        {
            return kExitOk;
        }
        return kExitProbeFailed;
    }

    if (argc != kExpectedArgumentCount)
    {
        std::cerr << "Usage: PluginLabScanner <plugin file> <result file>\n       PluginLabScanner --probe <plugin file> <plugin identifier> <result file>" << std::endl;
        return kExitWrongArguments;
    }

    silenceCrashDialogs();
    const juce::ScopedJuceInitialiser_GUI juceInitialiser; // plugins expect a message manager

    const juce::File pluginFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    const juce::File resultFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);

    juce::AudioPluginFormatManager formatManager;
    pluginlab::hosting::addHeadlessFormats(formatManager);
    const pluginlab::hosting::PluginScanResult result = pluginlab::hosting::PluginScanner::scanFileInProcess(formatManager, pluginFile);

    if (! pluginlab::hosting::writeScanResult(result, resultFile))
    {
        return kExitCannotWriteResult;
    }
    return kExitOk;
}
