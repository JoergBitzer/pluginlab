#include "HostShell.h"

namespace pluginlab::host
{
namespace
{
constexpr int kTabBarHeight = 30;
const juce::String kPluginsTabName = "Plugins";
const juce::String kCompareTabName = "Compare";
constexpr double kMsPerSecond = 1000.0;
constexpr int kWindowWidth = 1100;
constexpr int kWindowHeight = 760;
}

HostShell::HostShell(const StartupOptions& options)
    : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop), m_options(options)
{
    m_pluginsPage = new HostMainComponent(options);
    m_comparePage = new ComparePanel();
    m_comparePage->getSelectedPluginDescription = [this](juce::PluginDescription& description)
    {
        return m_pluginsPage->getSelectedDescription(description);
    };

    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    setTabBarDepth(kTabBarHeight);
    addTab(kPluginsTabName, background, m_pluginsPage, true);
    addTab(kCompareTabName, background, m_comparePage, true);

    setSize(kWindowWidth, kWindowHeight);

    if (options.compareAudioFile != juce::File())
    {
        m_comparePage->addFile(options.compareAudioFile);
        m_comparePage->addDrySlot();
        if (options.compareSlotPlugin != juce::File())
        {
            m_comparePage->addSlotFromFile(options.compareSlotPlugin);
        }
        setCurrentTabIndex(1);
        if (options.playSeconds > 0.0)
        {
            m_comparePage->startPlaying();
            startTimer(static_cast<int>(options.playSeconds * kMsPerSecond));
        }
    }
}

void HostShell::timerCallback()
{
    stopTimer();
    finishPlayTest();
}

// the command line test of the playback: the state after the time, as a file, then quit
void HostShell::finishPlayTest()
{
    const juce::String text = "playing: " + juce::String(static_cast<int>(m_comparePage->isPlaying())) + "\n" + m_comparePage->describeEngine() + "\n";
    m_options.playReportFile.replaceWithText(text);
    m_comparePage->stopPlaying();
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

HostShell::~HostShell()
{
    clearTabs(); // the pages are deleted here, before the base class goes
}
}
