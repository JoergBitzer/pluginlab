#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ComparePanel.h"
#include "HostMainComponent.h"
#include "MatcherPanel.h"

class BenchmarkProcessor;

// The editor of Jade Benchmark (W10.2): the pages Plugins, Compare and Matcher of the pages library as tabs (the Developer page stays in the
// stand-alone application: the author, "to avoid confusion for the musician"). The pages work on the processor's engine.
class BenchmarkEditor : public juce::AudioProcessorEditor
{
public:
    explicit BenchmarkEditor(BenchmarkProcessor& processor);
    ~BenchmarkEditor() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void slotsChanged();

    BenchmarkProcessor& m_processor;
    juce::TabbedComponent m_tabs{juce::TabbedButtonBar::TabsAtTop};
    juce::TooltipWindow m_tooltips{this};
    pluginlab::host::HostMainComponent* m_pluginsPage = nullptr; // owned by the tabs
    pluginlab::host::ComparePanel* m_comparePage = nullptr;
    pluginlab::host::MatcherPanel* m_matcherPage = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BenchmarkEditor)
};
