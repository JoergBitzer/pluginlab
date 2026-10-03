#include <iostream>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>
#include <juce_events/juce_events.h>

#include "pluginlab/hosting/PluginScanXml.h"
#include "pluginlab/hosting/PluginScanner.h"

#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace
{
constexpr int kExpectedArgumentCount = 3; // program, plugin file, result file
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
int main(int argc, char* argv[])
{
    if (argc != kExpectedArgumentCount)
    {
        std::cerr << "Usage: PluginLabScanner <plugin file> <result file>" << std::endl;
        return kExitWrongArguments;
    }

    silenceCrashDialogs();
    const juce::ScopedJuceInitialiser_GUI juceInitialiser; // plugins expect a message manager

    const juce::File pluginFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    const juce::File resultFile = juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]);

    juce::VST3PluginFormatHeadless format;
    const pluginlab::hosting::PluginScanResult result = pluginlab::hosting::PluginScanner::scanFileInProcess(format, pluginFile);

    if (! pluginlab::hosting::writeScanResult(result, resultFile))
    {
        return kExitCannotWriteResult;
    }
    return kExitOk;
}
