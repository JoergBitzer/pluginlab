#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HostMainComponent.h"
#include "HostReport.h"
#include "pluginlab/PluginLabVersion.h"

namespace
{

// Command line modes without a window (files, because a GUI program on Windows has no console; CI checks the files):
//   --write-version <file>              writes the version into the file and quits
//   --report <plugin folder> <file>     scans the folder, loads the plugins, writes a report (see HostReport.h) and quits
// Options of the window (manual tests): --scan <folder> scans the folder at startup, --load <plugin name> loads that plugin
// after the scan
const juce::String kWriteVersionOption = "--write-version";
const juce::String kReportOption = "--report";
const juce::String kScanOption = "--scan";
const juce::String kLoadOption = "--load";
constexpr int kArgumentsOfOneValue = 1;
constexpr int kArgumentsOfWriteVersion = 1;
constexpr int kArgumentsOfReport = 2;
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
        setContentOwned(new pluginlab::host::HostMainComponent(options), true);
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
            const juce::File versionFile(arguments[versionIndex + 1].unquoted());
            versionFile.replaceWithText(juce::String(pluginlab::getVersionString()));
            quit();
            return;
        }

        const int reportIndex = arguments.indexOf(kReportOption);
        if (reportIndex >= 0 && reportIndex + kArgumentsOfReport < arguments.size())
        {
            const juce::File pluginFolder(arguments[reportIndex + 1].unquoted());
            const juce::File reportFile(arguments[reportIndex + 2].unquoted());
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

        pluginlab::host::StartupOptions options;
        const int scanIndex = arguments.indexOf(kScanOption);
        if (scanIndex >= 0 && scanIndex + kArgumentsOfOneValue < arguments.size())
        {
            options.scanFolder = juce::File(arguments[scanIndex + 1].unquoted());
        }
        const int loadIndex = arguments.indexOf(kLoadOption);
        if (loadIndex >= 0 && loadIndex + kArgumentsOfOneValue < arguments.size())
        {
            options.pluginNameToLoad = arguments[loadIndex + 1].unquoted();
        }

        m_mainWindow = std::make_unique<MainWindow>(getApplicationName(), options);
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
