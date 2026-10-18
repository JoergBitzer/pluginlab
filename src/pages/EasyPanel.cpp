#include "EasyPanel.h"

#include <algorithm>
#include <cmath>

#include "PageServices.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginDisplayName.h"

namespace pluginlab::host
{
namespace
{
constexpr int kMargin = 8;
constexpr int kRowHeight = 28;
constexpr int kStepWidth = 190;
constexpr int kBoxWidth = 300;
constexpr int kButtonWidth = 160;
constexpr int kListenWidth = 90;
constexpr int kNameWidth = 260;
constexpr int kTrimWidth = 230;
constexpr int kProgressHeight = 22;
constexpr int kTimerMs = 200;
constexpr int kMaximumCompared = 3;             // the musician compares the own EQ with up to three others
constexpr double kSameDb = 0.25;                // the tolerance "medium" (the author's choice for the easy page)
constexpr double kAudiblyCloseDb = 1.0;         // the just noticeable difference of a level in a third-octave band (the author)
constexpr double kTrimRangeDb = 12.0;
constexpr double kMatchSeconds = 15.0;
const juce::String kTolerance = "medium";
const juce::Colour kActiveColour(0xff3a6ea5);
const juce::Colour kProgressColour(0xff3a6ea5);
const juce::Colour kProgressBackground(0xff2a2a2a);

juce::String formatDb(double value)
{
    return juce::String(value, 2) + " dB";
}

void setUpStep(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(15.0f, juce::Font::bold));
}
}

juce::String EasyPanel::describeCloseness(double largestDb)
{
    if (largestDb <= kSameDb)
    {
        return "the same";
    }
    if (largestDb <= kAudiblyCloseDb)
    {
        return "audibly close";
    }
    return "different";
}

EasyPanel::EasyPanel(engine::MeasurementEngine& engine, HostSettings& settings)
    : m_engine(engine), m_settings(settings)
{
    m_title.setText("Compare your EQ with others: make them sound the same, then listen.", juce::dontSendNotification);
    m_title.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    setUpStep(m_stepMine, "1. My EQ");
    m_masterBox.setTooltip("The EQ you own and know: set it as it sounds good to you (\"Show its editor\"); the others are made to sound like it");
    m_masterBox.onChange = [this] {
        rebuildRows();
    };
    m_editorButton.onClick = [this] {
        if (onShowEditorOfSlot && getMasterSlot() >= 0)
        {
            onShowEditorOfSlot(getMasterSlot());
        }
    };
    m_loadButton.setTooltip("Load the EQs to compare on the page Plugins (up to four: yours and three others)");
    m_loadButton.onClick = [this] {
        if (onShowPlugins)
        {
            onShowPlugins();
        }
    };
    setUpStep(m_stepCompare, "2. Compare with");
    setUpStep(m_stepMatch, "3. Make them the same");
    m_matchButton.setTooltip("Sets the knobs of the other EQs so that they sound like yours (the frequency response; it takes about 15 s per EQ, "
                             "in a process of its own: a crashing plugin does not take the program down)");
    m_matchButton.onClick = [this] { startMatch(); };
    setUpStep(m_stepListen, "4. Listen");
    m_playButton.onClick = [this] {
        if (onPlayOrStop)
        {
            onPlayOrStop();
        }
    };
    m_columns.setText("Listen, EQ, how close it came (the largest difference) and why not closer; its level", juce::dontSendNotification);
    m_columns.setColour(juce::Label::textColourId, juce::Colours::grey);
    setUpStep(m_stepSave, "5. Keep it");
    m_saveButton.setTooltip("Saves the settings of the other EQs as presets (in the settings folder, \"presets\"; the same in the application and the plugin)");
    m_saveButton.onClick = [this] { savePresets(); };
    for (juce::Component* component : std::initializer_list<juce::Component*>{&m_title, &m_stepMine, &m_masterBox, &m_editorButton, &m_loadButton,
                                                                               &m_stepCompare, &m_compareList, &m_stepMatch, &m_matchButton, &m_stepListen,
                                                                               &m_playButton, &m_columns, &m_stepSave, &m_saveButton, &m_status})
    {
        addAndMakeVisible(component);
    }
    slotsChanged();
    startTimer(kTimerMs);
}

EasyPanel::~EasyPanel()
{
    stopTimer();
    m_job.reset();
}

void EasyPanel::slotsChanged()
{
    const int previous = getMasterSlot();
    m_pluginSlots.clear();
    m_masterBox.clear(juce::dontSendNotification);
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        if (hosting::HostedPlugin* plugin = m_engine.getPlugin(slot))
        {
            m_pluginSlots.push_back(slot);
            m_masterBox.addItem(hosting::getDisplayName(plugin->getDescription()), static_cast<int>(m_pluginSlots.size()));
        }
    }
    int selected = m_pluginSlots.empty() ? 0 : 1;
    for (size_t index = 0; index < m_pluginSlots.size(); ++index)
    {
        if (m_pluginSlots[index] == previous)
        {
            selected = static_cast<int>(index) + 1;
        }
    }
    m_masterBox.setSelectedId(selected, juce::dontSendNotification);
    rebuildRows();
}

int EasyPanel::getMasterSlot() const
{
    const int id = m_masterBox.getSelectedId();
    if (id <= 0 || id > static_cast<int>(m_pluginSlots.size()))
    {
        return -1;
    }
    return m_pluginSlots[static_cast<size_t>(id - 1)];
}

std::vector<int> EasyPanel::getComparedSlots() const
{
    std::vector<int> slots;
    for (const int slot : m_pluginSlots)
    {
        if (slot != getMasterSlot() && static_cast<int>(slots.size()) < kMaximumCompared)
        {
            slots.push_back(slot);
        }
    }
    return slots;
}

void EasyPanel::rebuildRows()
{
    for (Row& row : m_rows)
    {
        removeChildComponent(row.listen.get());
        removeChildComponent(row.nameLabel.get());
        removeChildComponent(row.closeLabel.get());
        removeChildComponent(row.trim.get());
    }
    std::vector<Row> old = std::move(m_rows);
    m_rows.clear();
    const int master = getMasterSlot();
    std::vector<int> slots;
    if (master >= 0)
    {
        slots.push_back(master);
    }
    for (const int slot : getComparedSlots())
    {
        slots.push_back(slot);
    }
    slots.push_back(0); // the dry reference: "without EQ"
    int key = 1;
    for (const int slot : slots)
    {
        Row row;
        row.slot = slot;
        row.master = slot == master;
        hosting::HostedPlugin* plugin = m_engine.getPlugin(slot);
        row.name = plugin != nullptr ? hosting::getDisplayName(plugin->getDescription()) : juce::String("without EQ (the dry signal)");
        if (plugin != nullptr && ! row.master)
        {
            row.released = getMappedParameters(m_settings, plugin->getDescription()).empty() ? "no mapping: every continuous knob is moved" : "";
        }
        for (const Row& before : old)
        {
            if (before.slot == slot && before.outcome)
            {
                row.outcome = before.outcome;
            }
        }
        row.listen = std::make_unique<juce::TextButton>(juce::String(key++));
        row.listen->setTooltip("Listen to this one (or press its number on the page EQ Compare); the switch is a short crossfade");
        row.listen->setClickingTogglesState(false);
        const int rowSlot = slot;
        row.listen->onClick = [this, rowSlot] {
            m_engine.setActiveSlot(rowSlot);
            timerCallback();
        };
        row.nameLabel = std::make_unique<juce::Label>();
        row.closeLabel = std::make_unique<juce::Label>();
        row.trim = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        row.trim->setRange(-kTrimRangeDb, kTrimRangeDb, 0.1);
        row.trim->setTextValueSuffix(" dB");
        row.trim->setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, kRowHeight - 4);
        row.trim->setValue(m_engine.getSlotTrimDb(slot), juce::dontSendNotification);
        row.trim->setTooltip("The level of this one while you listen: the match sets it so that you compare the sound and not the loudness; move it if you like");
        row.trim->onValueChange = [this, rowSlot, slider = row.trim.get()] { m_engine.setSlotTrimDb(rowSlot, static_cast<float>(slider->getValue())); };
        addAndMakeVisible(*row.listen);
        addAndMakeVisible(*row.nameLabel);
        addAndMakeVisible(*row.closeLabel);
        addAndMakeVisible(*row.trim);
        m_rows.push_back(std::move(row));
    }
    juce::StringArray compared;
    for (const int slot : getComparedSlots())
    {
        compared.add(hosting::getDisplayName(m_engine.getPlugin(slot)->getDescription()));
    }
    const int others = static_cast<int>(m_pluginSlots.size()) - (master >= 0 ? 1 : 0);
    juce::String text = compared.isEmpty() ? juce::String("nothing yet: load the EQs to compare on the page Plugins") : compared.joinIntoString(", ");
    if (others > kMaximumCompared)
    {
        text += "  (only the first three of the " + juce::String(others) + " loaded)";
    }
    m_compareList.setText(text, juce::dontSendNotification);
    m_matchButton.setEnabled(m_job == nullptr && ! compared.isEmpty());
    m_saveButton.setEnabled(! compared.isEmpty());
    m_editorButton.setEnabled(master >= 0);
    updateRowTexts();
    resized();
}

void EasyPanel::updateRowTexts()
{
    for (Row& row : m_rows)
    {
        juce::String name = row.name;
        if (row.master)
        {
            name += "  (yours)";
        }
        row.nameLabel->setText(name, juce::dontSendNotification);
        juce::String close;
        if (row.master)
        {
            close = "the reference: the others are made to sound like it";
        }
        else if (row.slot == 0)
        {
            close = "for comparison: no EQ at all";
        }
        else if (! row.outcome)
        {
            close = "not matched yet";
            if (row.released.isNotEmpty())
            {
                close += " (" + row.released + ")";
            }
        }
        else if (row.outcome->refused)
        {
            close = "cannot be matched: " + (row.outcome->reason.isNotEmpty() ? row.outcome->reason : juce::String("it does not behave like an EQ"));
        }
        else
        {
            close = describeCloseness(row.outcome->largestDb) + " (" + formatDb(row.outcome->largestDb);
            if (row.outcome->worstHz > 0.0)
            {
                close += " at " + juce::String(juce::roundToInt(row.outcome->worstHz)) + " Hz";
            }
            close += ")";
            if (row.outcome->largestDb > kSameDb && row.outcome->cause.isNotEmpty())
            {
                close += "; mainly: " + row.outcome->cause;
            }
        }
        row.closeLabel->setText(close, juce::dontSendNotification);
        row.closeLabel->setTooltip(row.outcome && row.outcome->evidence.isNotEmpty() ? row.outcome->evidence : close);
    }
}

void EasyPanel::startMatch()
{
    if (m_job != nullptr || getMasterSlot() < 0)
    {
        return;
    }
    hosting::HostedPlugin* master = m_engine.getPlugin(getMasterSlot());
    MatchJobSpec spec;
    spec.masterPlugin = master->getDescription().fileOrIdentifier;
    spec.masterValues = engine::getSetting(master->getInstance());
    spec.tolerance = kTolerance;
    spec.seconds = kMatchSeconds;
    for (const int slot : getComparedSlots())
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(slot);
        std::vector<int> released = getMappedParameters(m_settings, plugin->getDescription());
        const bool mapped = ! released.empty();
        if (! mapped)
        {
            released = getContinuousParameters(m_engine, slot);
        }
        if (! released.empty())
        {
            spec.targets.push_back({plugin->getDescription().fileOrIdentifier, engine::getSetting(plugin->getInstance()), released, mapped});
        }
    }
    const juce::File folder = m_settings.getMatcherFolder();
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    const juce::File job = folder.getChildFile("easy_" + stamp + ".json");
    const juce::String problem = writeMatchJob(spec, job);
    if (problem.isNotEmpty())
    {
        setStatus("Cannot match: " + problem + ".");
        return;
    }
    m_lastReport = folder.getChildFile("easy_" + stamp + ".md");
    m_fraction = 0.0;
    m_step = "starting";
    m_startedMs = juce::Time::getMillisecondCounter();
    const juce::Component::SafePointer<EasyPanel> self(this);
    m_job = std::make_unique<MatchJob>(
        job, m_lastReport,
        [self](double fraction, const juce::String& step) {
            if (self != nullptr && fraction >= 0.0)
            {
                self->m_fraction = fraction;
                self->m_step = step;
                self->repaint(self->m_progressArea);
            }
        },
        [self](bool success, const juce::String& message) {
            if (self != nullptr)
            {
                self->matchFinished(success, message);
            }
        });
    m_matchButton.setEnabled(false);
    setStatus("Making them the same ... (about " + juce::String(juce::roundToInt(kMatchSeconds * static_cast<double>(spec.targets.size()))) + " s)");
    repaint(m_progressArea);
}

bool EasyPanel::isMatching() const
{
    return m_job != nullptr;
}

void EasyPanel::matchFinished(bool success, const juce::String& message)
{
    m_job.reset();
    m_fraction = -1.0;
    repaint(m_progressArea);
    m_matchButton.setEnabled(! getComparedSlots().empty());
    if (! success)
    {
        setStatus("The match failed: " + message);
        return;
    }
    const std::vector<MatchOutcome> outcomes = readMatchResult(m_lastReport);
    const int applied = applyMatchOutcomes(m_engine, outcomes, getComparedSlots());
    // the result per row, and the level trim: the matched EQ at the level of yours
    for (Row& row : m_rows)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(row.slot);
        if (plugin == nullptr || row.master)
        {
            continue;
        }
        for (const MatchOutcome& outcome : outcomes)
        {
            if (outcome.plugin == plugin->getDescription().createIdentifierString())
            {
                row.outcome = outcome;
                if (! outcome.refused)
                {
                    row.trim->setValue(juce::jlimit(-kTrimRangeDb, kTrimRangeDb, -outcome.levelDb), juce::sendNotificationSync);
                }
            }
        }
    }
    updateRowTexts();
    setStatus("Done: " + juce::String(applied) + " EQ(s) set. Listen and switch with the numbers; the level sliders keep the loudness the same.");
    if (onMatchFinished)
    {
        onMatchFinished();
    }
}

void EasyPanel::savePresets()
{
    const int master = getMasterSlot();
    if (master < 0)
    {
        return;
    }
    const int saved = saveMatchedPresets(m_engine, m_settings, getComparedSlots(), hosting::getDisplayName(m_engine.getPlugin(master)->getDescription()));
    setStatus("Saved " + juce::String(saved) + " preset(s) in " + m_settings.getPresetFolder().getFullPathName() + ".");
}

juce::String EasyPanel::describe() const
{
    juce::StringArray lines;
    for (const Row& row : m_rows)
    {
        lines.add(row.listen->getButtonText() + " " + row.nameLabel->getText() + ": " + row.closeLabel->getText() + "; level "
                  + juce::String(row.trim->getValue(), 1) + " dB" + (m_engine.getActiveSlot() == row.slot ? " [listening]" : ""));
    }
    return lines.joinIntoString("\n");
}

void EasyPanel::setStatus(const juce::String& text)
{
    m_status.setText(text, juce::dontSendNotification);
}

void EasyPanel::timerCallback()
{
    for (Row& row : m_rows)
    {
        const bool active = m_engine.getActiveSlot() == row.slot;
        row.listen->setColour(juce::TextButton::buttonColourId, active ? kActiveColour : getLookAndFeel().findColour(juce::TextButton::buttonColourId));
    }
    const bool playing = isPlaying && isPlaying();
    m_playButton.setButtonText(playing ? "Stop" : "Play");
    if (m_job != nullptr)
    {
        repaint(m_progressArea); // the elapsed time
    }
}

void EasyPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    if (m_fraction < 0.0)
    {
        return;
    }
    const juce::Rectangle<float> bar = m_progressArea.toFloat();
    g.setColour(kProgressBackground);
    g.fillRoundedRectangle(bar, 4.0f);
    g.setColour(kProgressColour);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * static_cast<float>(juce::jlimit(0.0, 1.0, m_fraction))), 4.0f);
    const int seconds = static_cast<int>((juce::Time::getMillisecondCounter() - m_startedMs) / 1000);
    g.setColour(juce::Colours::white);
    g.drawText(juce::String(juce::roundToInt(100.0 * m_fraction)) + " %, " + m_step + ", " + juce::String(seconds) + " s", m_progressArea.reduced(kMargin / 2, 0),
               juce::Justification::centredLeft, true);
}

void EasyPanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin * 2);
    m_title.setBounds(area.removeFromTop(kRowHeight));
    area.removeFromTop(kMargin);

    juce::Rectangle<int> row = area.removeFromTop(kRowHeight);
    m_stepMine.setBounds(row.removeFromLeft(kStepWidth));
    m_masterBox.setBounds(row.removeFromLeft(kBoxWidth));
    row.removeFromLeft(kMargin);
    m_editorButton.setBounds(row.removeFromLeft(kButtonWidth));
    row.removeFromLeft(kMargin);
    m_loadButton.setBounds(row.removeFromLeft(kButtonWidth));
    area.removeFromTop(kMargin);

    row = area.removeFromTop(kRowHeight);
    m_stepCompare.setBounds(row.removeFromLeft(kStepWidth));
    m_compareList.setBounds(row);
    area.removeFromTop(kMargin);

    row = area.removeFromTop(kRowHeight);
    m_stepMatch.setBounds(row.removeFromLeft(kStepWidth));
    m_matchButton.setBounds(row.removeFromLeft(kButtonWidth));
    row.removeFromLeft(kMargin);
    m_progressArea = row.withSizeKeepingCentre(row.getWidth(), kProgressHeight);
    area.removeFromTop(kMargin * 2);

    row = area.removeFromTop(kRowHeight);
    m_stepListen.setBounds(row.removeFromLeft(kStepWidth));
    m_playButton.setVisible(onPlayOrStop != nullptr && ! isInsideDaw());
    if (m_playButton.isVisible())
    {
        m_playButton.setBounds(row.removeFromLeft(kListenWidth));
        row.removeFromLeft(kMargin);
    }
    m_columns.setBounds(row);
    area.removeFromTop(kMargin / 2);
    for (Row& entry : m_rows)
    {
        row = area.removeFromTop(kRowHeight);
        row.removeFromLeft(kStepWidth);
        entry.listen->setBounds(row.removeFromLeft(kListenWidth / 2));
        row.removeFromLeft(kMargin);
        entry.nameLabel->setBounds(row.removeFromLeft(kNameWidth));
        entry.trim->setBounds(row.removeFromRight(kTrimWidth));
        entry.closeLabel->setBounds(row);
        area.removeFromTop(kMargin / 2);
    }
    area.removeFromTop(kMargin * 2);

    row = area.removeFromTop(kRowHeight);
    m_stepSave.setBounds(row.removeFromLeft(kStepWidth));
    m_saveButton.setBounds(row.removeFromLeft(kButtonWidth));
    area.removeFromTop(kMargin);
    m_status.setBounds(area.removeFromTop(kRowHeight * 2));
}
}
