#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HostShell.h"
#include "HostFingerprint.h"
#include "HostGuiSnapshot.h"
#include "HostReport.h"
#include "pluginlab/PluginLabVersion.h"

namespace
{
// a file named on the command line, relative to the working directory if it is not absolute
juce::File resolveFile(const juce::String& text)
{
    return juce::File::getCurrentWorkingDirectory().getChildFile(text.unquoted());
}


// Command line modes without a window (files, because a GUI program on Windows has no console; CI checks the files):
//   --write-version <file>              writes the version into the file and quits
//   --report <plugin folder> <file>     scans the folder, loads the plugins, writes a report (see HostReport.h) and quits
//   --fingerprint <plugin file> <file> [<plugin id>]  measures the fingerprint of the plugin (see HostFingerprint.h), writes it and quits
//   --measure <plugin file> <file> [<plugin id>]      runs the measurement units of W7 on the plugin (see HostFingerprint.h), writes them and quits
//   --expert <job file> <file>                       the measurement units and null tests of the page Expert (see HostFingerprint.h)
//   --map <plugin file> <file> [<plugin id>]          parameter survey and mapping proposal (see HostFingerprint.h); JSON into the user's mapping folder
//   --gui-snapshot <plugin file> <folder> [<plugin id>] captures the plugin's editor and writes the GUI review (see HostGuiSnapshot.h); in a virtual display
// Options of the window (manual tests): --scan <folder> scans the folder at startup, --load <plugin name> loads that plugin
// after the scan, --compare <audio file> [<plugin file>] opens the Compare page with the file, a dry slot and the plugin
const juce::String kWriteVersionOption = "--write-version";
const juce::String kReportOption = "--report";
const juce::String kFingerprintOption = "--fingerprint"; // --fingerprint <plugin file> <report file>
const juce::String kMeasureOption = "--measure";         // --measure <plugin file> <report file>
const juce::String kMapOption = "--map";                 // --map <plugin file> <report file>
const juce::String kCalibrateOption = "--calibrate";     // --calibrate <plugin file> <report file>
const juce::String kMatchOption = "--match";             // --match <job file> <report file>
const juce::String kExpertOption = "--expert";           // --expert <job file> <report file>
const juce::String kGuiSnapshotOption = "--gui-snapshot"; // --gui-snapshot <plugin file> <folder>
const juce::String kScanOption = "--scan";
const juce::String kLoadOption = "--load";
const juce::String kDeveloperOption = "--developer"; // --developer <plugin file> [--gui | --mapping | --calibration] [--view]: opens the Developer page and makes (and shows) the report, the GUI review, the mapping or the calibration
const juce::String kViewOption = "--view";
const juce::String kGuiOption = "--gui";         // with --developer: the GUI review instead of the report
const juce::String kMappingOption = "--mapping"; // with --developer: the survey and mapping proposal instead of the report
const juce::String kMatcherOption = "--matcher"; // --matcher <master plugin file> <target plugin file> [--run]: the Matcher page with both loaded
const juce::String kRunOption = "--run";
const juce::String kCalibrationOption = "--calibration"; // with --developer: the calibration (and the interactions) instead of the report
const juce::String kPlayOption = "--play"; // --play <seconds> <report file> (with --compare): plays, writes the report, quits
const juce::String kCompareOption = "--compare"; // --compare <audio file> [<plugin file>]: opens the Compare page
constexpr int kArgumentsOfOneValue = 1;
constexpr int kArgumentsOfWriteVersion = 1;
constexpr int kArgumentsOfReport = 2;
constexpr int kArgumentsOfPlay = 2;
constexpr int kExitOk = 0;
constexpr int kExitReportFailed = 1;
}

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow(const juce::String& name, const pluginlab::host::StartupOptions& options)
        : juce::DocumentWindow(name,
                               juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                               juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new pluginlab::host::HostShell(options), true);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class PluginLabHostApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override
    {
        return JUCE_APPLICATION_NAME_STRING;
    }

    const juce::String getApplicationVersion() override
    {
        return juce::String(pluginlab::getVersionString());
    }

    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }

    void initialise(const juce::String& commandLine) override
    {
        const juce::StringArray arguments = juce::StringArray::fromTokens(commandLine, true);
        const int versionIndex = arguments.indexOf(kWriteVersionOption);
        if (versionIndex >= 0 && versionIndex + kArgumentsOfWriteVersion < arguments.size())
        {
            const juce::File versionFile = resolveFile(arguments[versionIndex + 1]);
            versionFile.replaceWithText(juce::String(pluginlab::getVersionString()));
            quit();
            return;
        }

        const int reportIndex = arguments.indexOf(kReportOption);
        if (reportIndex >= 0 && reportIndex + kArgumentsOfReport < arguments.size())
        {
            const juce::File pluginFolder = resolveFile(arguments[reportIndex + 1]);
            const juce::File reportFile = resolveFile(arguments[reportIndex + 2]);
            const bool written = pluginlab::host::writeReport(pluginFolder, reportFile);
            int returnValue = kExitOk;
            if (! written)
            {
                returnValue = kExitReportFailed;
            }
            setApplicationReturnValue(returnValue);
            quit();
            return;
        }

        const int snapshotIndex = arguments.indexOf(kGuiSnapshotOption);
        if (snapshotIndex >= 0 && snapshotIndex + kArgumentsOfReport < arguments.size())
        {
            juce::String identifier;
            const int identifierIndex = snapshotIndex + kArgumentsOfReport + 1;
            if (identifierIndex < arguments.size() && ! arguments[identifierIndex].startsWith("--"))
            {
                identifier = arguments[identifierIndex].unquoted();
            }
            const bool written = pluginlab::host::writeGuiSnapshots(resolveFile(arguments[snapshotIndex + 1]), resolveFile(arguments[snapshotIndex + 2]), identifier);
            int returnValue = kExitOk;
            if (! written)
            {
                returnValue = kExitReportFailed;
            }
            setApplicationReturnValue(returnValue);
            quit();
            return;
        }

        const int mapIndex = arguments.indexOf(kMapOption);
        if (mapIndex >= 0 && mapIndex + kArgumentsOfReport < arguments.size())
        {
            juce::String identifier;
            const int identifierIndex = mapIndex + kArgumentsOfReport + 1;
            if (identifierIndex < arguments.size() && ! arguments[identifierIndex].startsWith("--"))
            {
                identifier = arguments[identifierIndex].unquoted();
            }
            const bool written = pluginlab::host::writeMappingProposal(resolveFile(arguments[mapIndex + 1]), resolveFile(arguments[mapIndex + 2]), identifier);
            setApplicationReturnValue(written ? kExitOk : kExitReportFailed);
            quit();
            return;
        }

        const int expertIndex = arguments.indexOf(kExpertOption);
        if (expertIndex >= 0 && expertIndex + kArgumentsOfReport < arguments.size())
        {
            const bool written = pluginlab::host::writeExpertReport(resolveFile(arguments[expertIndex + 1]), resolveFile(arguments[expertIndex + 2]));
            setApplicationReturnValue(written ? kExitOk : kExitReportFailed);
            quit();
            return;
        }

        const int matchIndex = arguments.indexOf(kMatchOption);
        if (matchIndex >= 0 && matchIndex + kArgumentsOfReport < arguments.size())
        {
            const bool written = pluginlab::host::writeMatchReport(resolveFile(arguments[matchIndex + 1]), resolveFile(arguments[matchIndex + 2]));
            setApplicationReturnValue(written ? kExitOk : kExitReportFailed);
            quit();
            return;
        }

        const int calibrateIndex = arguments.indexOf(kCalibrateOption);
        if (calibrateIndex >= 0 && calibrateIndex + kArgumentsOfReport < arguments.size())
        {
            juce::String identifier;
            const int identifierIndex = calibrateIndex + kArgumentsOfReport + 1;
            if (identifierIndex < arguments.size() && ! arguments[identifierIndex].startsWith("--"))
            {
                identifier = arguments[identifierIndex].unquoted();
            }
            const bool written =
                pluginlab::host::writeCalibrationReport(resolveFile(arguments[calibrateIndex + 1]), resolveFile(arguments[calibrateIndex + 2]), identifier);
            setApplicationReturnValue(written ? kExitOk : kExitReportFailed);
            quit();
            return;
        }

        const int measureIndex = arguments.indexOf(kMeasureOption);
        if (measureIndex >= 0 && measureIndex + kArgumentsOfReport < arguments.size())
        {
            juce::String identifier;
            const int identifierIndex = measureIndex + kArgumentsOfReport + 1;
            if (identifierIndex < arguments.size() && ! arguments[identifierIndex].startsWith("--"))
            {
                identifier = arguments[identifierIndex].unquoted();
            }
            const bool written = pluginlab::host::writeMeasurementReport(resolveFile(arguments[measureIndex + 1]), resolveFile(arguments[measureIndex + 2]),
                                                                         identifier);
            int returnValue = kExitOk;
            if (! written)
            {
                returnValue = kExitReportFailed;
            }
            setApplicationReturnValue(returnValue);
            quit();
            return;
        }

        const int fingerprintIndex = arguments.indexOf(kFingerprintOption);
        if (fingerprintIndex >= 0 && fingerprintIndex + kArgumentsOfReport < arguments.size())
        {
            juce::String identifier;
            const int identifierIndex = fingerprintIndex + kArgumentsOfReport + 1;
            if (identifierIndex < arguments.size() && ! arguments[identifierIndex].startsWith("--"))
            {
                identifier = arguments[identifierIndex].unquoted();
            }
            const bool written = pluginlab::host::writeFingerprintReport(resolveFile(arguments[fingerprintIndex + 1]),
                                                                         resolveFile(arguments[fingerprintIndex + 2]), identifier);
            int returnValue = kExitOk;
            if (! written)
            {
                returnValue = kExitReportFailed;
            }
            setApplicationReturnValue(returnValue);
            quit();
            return;
        }

        pluginlab::host::StartupOptions options;
        const int scanIndex = arguments.indexOf(kScanOption);
        if (scanIndex >= 0 && scanIndex + kArgumentsOfOneValue < arguments.size())
        {
            options.scanFolder = resolveFile(arguments[scanIndex + 1]);
        }
        const int loadIndex = arguments.indexOf(kLoadOption);
        if (loadIndex >= 0 && loadIndex + kArgumentsOfOneValue < arguments.size())
        {
            options.pluginNameToLoad = arguments[loadIndex + 1].unquoted();
        }

        const int compareIndex = arguments.indexOf(kCompareOption);
        if (compareIndex >= 0 && compareIndex + kArgumentsOfOneValue < arguments.size())
        {
            options.compareAudioFile = resolveFile(arguments[compareIndex + 1]);
            if (compareIndex + kArgumentsOfOneValue + 1 < arguments.size())
            {
                options.compareSlotPlugin = resolveFile(arguments[compareIndex + 2]);
            }
        }

        const int developerIndex = arguments.indexOf(kDeveloperOption);
        if (developerIndex >= 0 && developerIndex + kArgumentsOfOneValue < arguments.size())
        {
            options.developerPlugin = resolveFile(arguments[developerIndex + 1]);
            options.developerView = arguments.contains(kViewOption);
            options.developerGui = arguments.contains(kGuiOption);
            options.developerMapping = arguments.contains(kMappingOption);
            options.developerCalibration = arguments.contains(kCalibrationOption);
        }

        const int matcherIndex = arguments.indexOf(kMatcherOption);
        if (matcherIndex >= 0 && matcherIndex + 2 < arguments.size())
        {
            options.matcherMaster = resolveFile(arguments[matcherIndex + 1]);
            options.matcherTarget = resolveFile(arguments[matcherIndex + 2]);
            options.matcherRun = arguments.contains(kRunOption);
        }

        const int playIndex = arguments.indexOf(kPlayOption);
        if (playIndex >= 0 && playIndex + kArgumentsOfPlay < arguments.size())
        {
            options.playSeconds = arguments[playIndex + 1].getDoubleValue();
            options.playReportFile = resolveFile(arguments[playIndex + 2]);
        }

        m_mainWindow = std::make_unique<MainWindow>(getApplicationName() + " " + getApplicationVersion(), options);
    }

    void shutdown() override
    {
        m_mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<MainWindow> m_mainWindow;
};

START_JUCE_APPLICATION(PluginLabHostApplication)
