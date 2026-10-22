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
const juce::String kCompareTabName = "EQ Compare"; // the curves and the deviations assume EQs (LTI); listening works for any plugin
const juce::String kDeveloperTabName = "Developer";
const juce::String kMatcherTabName = "Matcher";
const juce::String kExpertTabName = "Expert";    // the measurement units and null tests (W10.5)
const juce::String kEasyTabName = "Easy";        // the musician's walkthrough (W10.4); last, so that the tabs of the tests keep their places
constexpr int kPluginsTab = 0;
constexpr int kCompareTab = 1;
constexpr int kMatcherTab = 3;
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
    m_developerPage = new DeveloperPanel(m_engine, m_settings);
    m_matcherPage = new MatcherPanel(m_engine, m_settings);
    m_easyPage = new EasyPanel(m_engine, m_settings);
    m_easyPage->onShowPlugins = [this] { setCurrentTabIndex(kPluginsTab); };
    m_easyPage->onShowEditorOfSlot = [this](int slot) { m_pluginsPage->showEditorOfSlot(slot); };
    m_easyPage->onPlayOrStop = [this] {
        if (m_comparePage->isPlaying())
        {
            m_comparePage->stopPlaying();
        }
        else
        {
            m_comparePage->startPlaying(); // the audio files of the page EQ Compare
        }
    };
    m_easyPage->isPlaying = [this] { return m_comparePage->isPlaying(); };
    m_expertPage = new ExpertPanel(m_engine, m_settings);
    m_expertPage->onShowReport = [this](const juce::String& title, const juce::File& report) { m_developerPage->showReport(title, report); };
    m_expertPage->onShowMatcher = [this] { setCurrentTabIndex(kMatcherTab); };
    m_expertPage->onShowCompare = [this] { setCurrentTabIndex(kCompareTab); };
    m_matcherPage->onListen = [this] { setCurrentTabIndex(kCompareTab); };
    m_matcherPage->onShowReport = [this](const juce::String& title, const juce::File& report) { m_developerPage->showReport(title, report); };
    m_pluginsPage->onSlotsChanged = [this] { slotsChanged(); };
    m_comparePage->onFilesChanged = [this] { filesChanged(); };
    m_comparePage->onShowEditorOfSlot = [this](int slot) { m_pluginsPage->showEditorOfSlot(slot); };
    m_comparePage->onChangeEngineRate = [this](double rate) { changeEngineRate(rate); };

    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    setTabBarDepth(kTabBarHeight);
    addTab(kPluginsTabName, background, m_pluginsPage, true);
    addTab(kCompareTabName, background, m_comparePage, true);
    addTab(kDeveloperTabName, background, m_developerPage, true);
    addTab(kMatcherTabName, background, m_matcherPage, true);
    addTab(kEasyTabName, background, m_easyPage, true);
    addTab(kExpertTabName, background, m_expertPage, true);
    setSize(kWindowWidth, kWindowHeight);
    slotsChanged(); // the dry slot on every page (saves nothing: the session is saved after the questions of the start)

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

    if (options.matcherMaster != juce::File())
    {
        // the Matcher page with the master and the target loaded (W9.6)
        juce::AudioPluginFormatManager manager;
        ui::addGuiFormats(manager);
        const bool curve = options.matcherMaster.getFileName() == "curve"; // the band curve as master instead of a plugin
        for (const juce::File& file : {options.matcherMaster, options.matcherTarget})
        {
            if (curve && file == options.matcherMaster)
            {
                continue;
            }
            const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(manager, file);
            if (! scan.descriptions.isEmpty())
            {
                m_pluginsPage->loadPlugin(scan.descriptions[0]);
            }
        }
        setCurrentTabIndex(kMatcherTab);
        if (! curve)
        {
            m_matcherPage->chooseFirstAsMaster();
        }
        if (options.matcherRun)
        {
            m_matcherPage->onMatchFinished = [this] {
                m_matcherPage->applyResults();
                m_matcherPage->showLastReport();
            };
            m_matcherPage->startMatch();
        }
    }
    if (options.developerPlugin != juce::File())
    {
        juce::AudioPluginFormatManager manager;
        ui::addGuiFormats(manager);
        const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(manager, options.developerPlugin);
        if (! scan.descriptions.isEmpty())
        {
            m_pluginsPage->loadPlugin(scan.descriptions[0]);
            setCurrentTabIndex(2);
            if (options.developerCalibration)
            {
                // the calibration and the interactions (W8.6, W8.7)
                if (options.developerView)
                {
                    m_developerPage->onCalibrationReady = [this](int row) { m_developerPage->viewCalibration(row); };
                }
                m_developerPage->generateCalibration(0);
            }
            else if (options.developerMapping)
            {
                // the survey and mapping proposal (W8.5)
                if (options.developerView)
                {
                    m_developerPage->onMappingReady = [this](int row) { m_developerPage->viewMapping(row); };
                }
                m_developerPage->generateMapping(0);
            }
            else if (options.developerGui)
            {
                // the GUI review instead of the report (W5d.7)
                if (options.developerView)
                {
                    m_developerPage->onGuiReviewReady = [this](int row) { m_developerPage->viewGuiReview(row); };
                }
                m_developerPage->generateGuiReview(0);
            }
            else
            {
                if (options.developerView)
                {
                    m_developerPage->onReportReady = [this](int row) { m_developerPage->viewReport(row); };
                }
                m_developerPage->generateReport(0);
            }
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
    slotsChanged();
}

// every page that shows the slots (the one place that lists them: the Expert and Easy pages were missing here until 0.95.0, so plugins loaded
// on page 1 did not reach them)
void HostShell::slotsChanged()
{
    m_comparePage->slotsChanged();
    m_developerPage->refresh();
    m_matcherPage->slotsChanged();
    m_easyPage->slotsChanged();
    m_expertPage->slotsChanged();
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
    slotsChanged(); // saves nothing yet: the session is saved from endRestore on
    endRestore();
}

void HostShell::endRestore()
{
    m_sessionIsSaved = true;
}
}
