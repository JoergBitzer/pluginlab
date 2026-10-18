#include "BenchmarkEditor.h"

#include "BenchmarkProcessor.h"

namespace
{
constexpr int kWidth = 1100;
constexpr int kHeight = 760;
constexpr int kMinimumWidth = 800;
constexpr int kMinimumHeight = 560;
constexpr int kTabBarHeight = 30;
constexpr int kCompareTab = 1;
}

BenchmarkEditor::BenchmarkEditor(BenchmarkProcessor& owner)
    : juce::AudioProcessorEditor(&owner), m_processor(owner)
{
    m_pluginsPage = new pluginlab::host::HostMainComponent(owner.getEngine(), owner.getFormatManager(), owner.getSettings());
    m_comparePage = new pluginlab::host::ComparePanel(owner.getEngine(), owner.getSettings());
    m_matcherPage = new pluginlab::host::MatcherPanel(owner.getEngine(), owner.getSettings());
    m_pluginsPage->onSlotsChanged = [this] { slotsChanged(); };
    m_comparePage->onShowEditorOfSlot = [this](int slot) { m_pluginsPage->showEditorOfSlot(slot); };
    m_matcherPage->onListen = [this] { m_tabs.setCurrentTabIndex(kCompareTab); };
    // the report of a match: in the system's viewer for Markdown (the report window of the Developer page is not in the plugin)
    m_matcherPage->onShowReport = [](const juce::String&, const juce::File& report) { report.startAsProcess(); };
    owner.onSlotsRestored = [this] {
        m_pluginsPage->refresh();
        m_comparePage->slotsChanged();
        m_matcherPage->slotsChanged();
    };
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    m_tabs.setTabBarDepth(kTabBarHeight);
    m_tabs.addTab("Plugins", background, m_pluginsPage, true);
    m_tabs.addTab("EQ Compare", background, m_comparePage, true);
    m_tabs.addTab("Matcher", background, m_matcherPage, true);
    addAndMakeVisible(m_tabs);
    setResizable(true, true);
    setResizeLimits(kMinimumWidth, kMinimumHeight, 4 * kWidth, 4 * kHeight);
    setSize(kWidth, kHeight);
    m_comparePage->slotsChanged();
}

BenchmarkEditor::~BenchmarkEditor()
{
    m_processor.onSlotsRestored = nullptr;
    m_pluginsPage->closeAllEditorWindows();
}

void BenchmarkEditor::slotsChanged()
{
    m_comparePage->slotsChanged();
    m_matcherPage->slotsChanged();
    m_processor.slotsChanged();
}

void BenchmarkEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void BenchmarkEditor::resized()
{
    m_tabs.setBounds(getLocalBounds());
}
