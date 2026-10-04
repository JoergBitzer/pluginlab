#include "HostShell.h"

#include "pluginlab/engine/SessionFiles.h"
#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/ui/GuiFormats.h"

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
constexpr double kDefaultRate = 48000.0;
constexpr int kEngineBlockSize = 512;
constexpr int kAnswerLoad = 1; // the first button of a message box
constexpr int kStartQuestionDelayMs = 400;
const juce::String kDryName = "dry (no plugin)";
}

HostShell::HostShell(const StartupOptions& options)
    : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop), m_options(options)
{
    ui::addGuiFormats(m_formatManager);
    m_engine.prepare(kDefaultRate, kEngineBlockSize);
    juce::String error;
    m_engine.addSlot(nullptr, kDryName, error); // always there: the reference without a plugin

    m_pluginsPage = new HostMainComponent(m_engine, m_formatManager, m_settings, options);
    m_comparePage = new ComparePanel(m_engine, m_settings);
    m_pluginsPage->onSlotsChanged = [this] { slotsChanged(); };
    m_comparePage->onFilesChanged = [this] { filesChanged(); };
    m_comparePage->onShowEditorOfSlot = [this](int slot) { m_pluginsPage->showEditorOfSlot(slot); };
    m_comparePage->onChangeEngineRate = [this](double rate) { changeEngineRate(rate); };

    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    setTabBarDepth(kTabBarHeight);
    addTab(kPluginsTabName, background, m_pluginsPage, true);
    addTab(kCompareTabName, background, m_comparePage, true);
    setSize(kWindowWidth, kWindowHeight);
    m_comparePage->slotsChanged();

    if (options.compareAudioFile != juce::File())
    {
        m_comparePage->addFile(options.compareAudioFile);
        if (options.compareSlotPlugin != juce::File())
        {
            juce::AudioPluginFormatManager manager;
            ui::addGuiFormats(manager);
            const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(manager, options.compareSlotPlugin);
            if (! scan.descriptions.isEmpty())
            {
                m_pluginsPage->loadPlugin(scan.descriptions[0]);
            }
        }
        setCurrentTabIndex(1);
        if (options.playSeconds > 0.0)
        {
            m_comparePage->startPlaying();
            startTimer(static_cast<int>(options.playSeconds * kMsPerSecond));
        }
    }

    if (options.isManualTest())
    {
        m_sessionIsSaved = false; // a manual test must not change the session of the user
        return;
    }
    const juce::Component::SafePointer<HostShell> self(this);
    juce::Timer::callAfterDelay(kStartQuestionDelayMs,
                                [self]
                                {
                                    if (self != nullptr)
                                    {
                                        self->askToRestoreSession();
                                    }
                                });
}

HostShell::~HostShell()
{
    stopTimer();
    saveSession();
    m_comparePage->stopPlaying();
    clearTabs(); // the pages (and their editor windows) are deleted here, before the engine and its plugins
    m_engine.clearSlots();
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

void HostShell::changeEngineRate(double sampleRate)
{
    m_pluginsPage->closeAllEditorWindows(); // before the plugins are removed
    m_engine.clearSlots();
    m_engine.clearFiles();
    m_engine.prepare(sampleRate, kEngineBlockSize);
    juce::String error;
    m_engine.addSlot(nullptr, kDryName, error);
    m_pluginsPage->refresh();
    m_comparePage->slotsChanged();
    saveSession();
}

void HostShell::slotsChanged()
{
    m_comparePage->slotsChanged();
    saveSession();
}

void HostShell::filesChanged()
{
    saveSession();
}

// The state of the session: the lists are written whenever they change, so that the next start can offer them.
void HostShell::saveSession()
{
    if (! m_sessionIsSaved)
    {
        return;
    }
    pluginlab::engine::saveAudioList(m_engine, m_settings.getLastSessionAudioList());
    pluginlab::engine::savePluginSet(m_engine, m_settings.getLastSessionPluginSet());
}

// ---- the start: ask before the last lists are loaded (a plugin can crash the program) ----

void HostShell::askToRestoreSession()
{
    const int numberOfFiles = pluginlab::engine::countFilesInAudioList(m_settings.getLastSessionAudioList());
    if (numberOfFiles <= 0)
    {
        askForPluginSet(pluginlab::engine::countPluginsInPluginSet(m_settings.getLastSessionPluginSet()));
        return;
    }
    juce::String fileText = juce::String(numberOfFiles) + " files";
    if (numberOfFiles == 1)
    {
        fileText = "1 file";
    }
    const juce::String message = "Load the audio list of the last session (" + fileText + ")?";
    const juce::Component::SafePointer<HostShell> self(this);
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::QuestionIcon)
                                     .withTitle("Last session")
                                     .withMessage(message)
                                     .withButton("Load")
                                     .withButton("Skip")
                                     .withAssociatedComponent(this),
                                 [self](int answer)
                                 {
                                     if (self == nullptr)
                                     {
                                         return;
                                     }
                                     if (answer == kAnswerLoad)
                                     {
                                         self->m_comparePage->loadAudioListFile(self->m_settings.getLastSessionAudioList());
                                     }
                                     self->askForPluginSet(pluginlab::engine::countPluginsInPluginSet(self->m_settings.getLastSessionPluginSet()));
                                 });
}

void HostShell::askForPluginSet(int numberOfPlugins)
{
    if (numberOfPlugins <= 0)
    {
        endRestore();
        return;
    }
    juce::String pluginText = juce::String(numberOfPlugins) + " plugins";
    if (numberOfPlugins == 1)
    {
        pluginText = "1 plugin";
    }
    juce::String message = "Load the plugins of the last session (" + pluginText + ")?\n\n"
                         + "A plugin can crash the program when it is loaded.";
    if (m_settings.getRestoreMarker().existsAsFile())
    {
        message = "The last attempt to load the plugins of the session did not finish: the program probably crashed in one of them.\n\n"
                  "Load " + pluginText + " anyway?";
    }
    const juce::Component::SafePointer<HostShell> self(this);
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withIconType(juce::MessageBoxIconType::QuestionIcon)
                                     .withTitle("Last session")
                                     .withMessage(message)
                                     .withButton("Load")
                                     .withButton("Skip")
                                     .withAssociatedComponent(this),
                                 [self](int answer)
                                 {
                                     if (self == nullptr)
                                     {
                                         return;
                                     }
                                     if (answer == kAnswerLoad)
                                     {
                                         self->restorePluginSet();
                                         return;
                                     }
                                     self->m_settings.getRestoreMarker().deleteFile();
                                     self->endRestore();
                                 });
}

// The marker file exists while the plugins are loaded: if the program dies in a plugin it is still there at the next start.
void HostShell::restorePluginSet()
{
    const juce::File marker = m_settings.getRestoreMarker();
    marker.getParentDirectory().createDirectory();
    marker.replaceWithText("loading the plugins of the last session");

    m_pluginsPage->closeAllEditorWindows();
    juce::String report;
    pluginlab::engine::loadPluginSet(m_engine, m_formatManager, m_settings.getLastSessionPluginSet(), report);
    marker.deleteFile();
    m_pluginsPage->refresh();
    m_comparePage->slotsChanged();
    endRestore();
}

void HostShell::endRestore()
{
    m_sessionIsSaved = true;
}
}
