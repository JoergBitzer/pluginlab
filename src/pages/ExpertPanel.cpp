#include "ExpertPanel.h"

#include <algorithm>

#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/hosting/PluginDisplayName.h"

namespace pluginlab::host
{
namespace
{
constexpr int kMargin = 8;
constexpr int kRowHeight = 26;
constexpr int kLabelWidth = 130;
constexpr int kToggleWidth = 190;
constexpr int kButtonWidth = 180;
constexpr int kProgressHeight = 22;
constexpr int kTimerMs = 500;
constexpr int kColumnQuantity = 1;
constexpr int kWidthQuantity = 330;
constexpr int kWidthSlot = 230;
constexpr int kNoNullItem = 1;           // the first item of the null box: no null test
const juce::Colour kProgressColour(0xff3a6ea5);
const juce::Colour kProgressBackground(0xff2a2a2a);
const juce::String kNullDepthRow = "null depth against ";
constexpr double kIdenticalDb = -290.0;  // the null test's floor: bit-identical outputs
const juce::String kNullAlignmentRow = "null test: delay, gain of the alignment";
}

ExpertPanel::ExpertPanel(engine::MeasurementEngine& engine, HostSettings& settings)
    : m_engine(engine), m_settings(settings)
{
    m_title.setText("Measure the slots: the AES17 units with the system class first, null tests against a master, a report.", juce::dontSendNotification);
    m_title.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    m_systemClassNote.setText("system class: always, first", juce::dontSendNotification);
    m_systemClassNote.setColour(juce::Label::textColourId, juce::Colours::grey);
    for (const juce::String& key : engine::getMeasurementUnitKeys())
    {
        auto toggle = std::make_unique<juce::ToggleButton>(engine::getMeasurementUnitTitle(key));
        toggle->setToggleState(key != "linearity", juce::dontSendNotification); // the slowest unit is off by default
        toggle->setComponentID(key);
        addAndMakeVisible(*toggle);
        m_unitToggles.push_back(std::move(toggle));
    }
    m_nullBox.setTooltip("Each other chosen slot against this one: aligned in delay and gain and subtracted (docs/measurements/null-test.md)");
    m_measureButton.setTooltip("Runs the chosen units on the chosen slots in a process of its own (a crashing plugin does not take the program down)");
    m_measureButton.onClick = [this] { startMeasurement(); };
    m_reportButton.onClick = [this] {
        if (onShowReport && m_lastReport.existsAsFile())
        {
            onShowReport("Expert: " + m_lastReport.getFileNameWithoutExtension(), m_lastReport);
        }
    };
    m_matcherButton.onClick = [this] {
        if (onShowMatcher)
        {
            onShowMatcher();
        }
    };
    m_compareButton.onClick = [this] {
        if (onShowCompare)
        {
            onShowCompare();
        }
    };
    m_presetBox.setTooltip("Measurement presets: the units, the slots by role and the null test, shared by the application and the plugin "
                           "(the settings folder, measurement_presets)");
    m_presetBox.onChange = [this] {
        const int index = m_presetBox.getSelectedItemIndex();
        if (index >= 0 && index < static_cast<int>(m_presets.size()))
        {
            applyPreset(m_presets[static_cast<size_t>(index)]);
        }
    };
    m_savePresetButton.onClick = [this] { askToSavePreset(); };
    m_presetNote.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_presetNote.setJustificationType(juce::Justification::topLeft);
    m_table.setModel(this);
    m_table.setRowHeight(kRowHeight - 4);
    for (juce::Component* component : std::initializer_list<juce::Component*>{&m_title, &m_presetLabel, &m_presetBox, &m_savePresetButton, &m_presetNote, &m_slotsLabel, &m_unitsLabel, &m_systemClassNote, &m_nullLabel,
                                                                               &m_nullBox, &m_measureButton, &m_reportButton, &m_matcherButton,
                                                                               &m_compareButton, &m_table, &m_status})
    {
        addAndMakeVisible(component);
    }
    slotsChanged();
    rebuildTable();
    refreshPresets();
    startTimer(kTimerMs);
}

ExpertPanel::~ExpertPanel()
{
    stopTimer();
    m_job.reset();
}

void ExpertPanel::slotsChanged()
{
    rebuildSlotToggles();
    const int previous = m_nullBox.getSelectedId();
    m_nullBox.clear(juce::dontSendNotification);
    m_nullBox.addItem("none", kNoNullItem);
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        m_nullBox.addItem(juce::String(slot + 1) + ": " + m_engine.getSlotInfo(slot).name, kNoNullItem + 1 + slot);
    }
    m_nullBox.setSelectedId(previous > 0 && previous <= kNoNullItem + m_engine.getNumSlots() ? previous : kNoNullItem, juce::dontSendNotification);
    resized();
}

void ExpertPanel::rebuildSlotToggles()
{
    for (const auto& toggle : m_slotToggles)
    {
        removeChildComponent(toggle.get());
    }
    m_slotToggles.clear();
    m_slotChosen.resize(static_cast<size_t>(m_engine.getNumSlots()), false);
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        auto toggle = std::make_unique<juce::ToggleButton>(juce::String(slot + 1) + ": " + m_engine.getSlotInfo(slot).name);
        // the plugins are chosen by default, the dry slot not (it is the identity)
        const bool chosen = m_slotChosen[static_cast<size_t>(slot)] || m_engine.getPlugin(slot) != nullptr;
        m_slotChosen[static_cast<size_t>(slot)] = chosen;
        toggle->setToggleState(chosen, juce::dontSendNotification);
        toggle->onClick = [this, slot, button = toggle.get()] { m_slotChosen[static_cast<size_t>(slot)] = button->getToggleState(); };
        addAndMakeVisible(*toggle);
        m_slotToggles.push_back(std::move(toggle));
    }
}

void ExpertPanel::setSlotChosen(int slot, bool chosen)
{
    if (slot >= 0 && slot < static_cast<int>(m_slotChosen.size()))
    {
        m_slotChosen[static_cast<size_t>(slot)] = chosen;
        m_slotToggles[static_cast<size_t>(slot)]->setToggleState(chosen, juce::dontSendNotification);
    }
}

void ExpertPanel::setUnitChosen(const juce::String& key, bool chosen)
{
    for (const auto& toggle : m_unitToggles)
    {
        if (toggle->getComponentID() == key)
        {
            toggle->setToggleState(chosen, juce::dontSendNotification);
        }
    }
}

void ExpertPanel::setNullMaster(int slot)
{
    m_nullBox.setSelectedId(slot >= 0 ? kNoNullItem + 1 + slot : kNoNullItem, juce::dontSendNotification);
}

juce::StringArray ExpertPanel::getChosenUnits() const
{
    juce::StringArray units;
    for (const auto& toggle : m_unitToggles)
    {
        if (toggle->getToggleState())
        {
            units.add(toggle->getComponentID());
        }
    }
    return units;
}

int ExpertPanel::getNullMaster() const
{
    return m_nullBox.getSelectedId() > kNoNullItem ? m_nullBox.getSelectedId() - kNoNullItem - 1 : -1;
}

std::vector<int> ExpertPanel::getChosenSlots() const
{
    std::vector<int> slots;
    for (size_t slot = 0; slot < m_slotChosen.size(); ++slot)
    {
        if (m_slotChosen[slot])
        {
            slots.push_back(static_cast<int>(slot));
        }
    }
    return slots;
}

MeasurementPreset ExpertPanel::makePreset(const juce::String& name, const juce::String& note) const
{
    MeasurementPreset preset;
    preset.name = name;
    preset.note = note;
    preset.units = getChosenUnits();
    preset.includeDry = ! m_slotChosen.empty() && m_slotChosen[0];
    const int master = getNullMaster();
    int firstPlugin = -1;
    for (int slot = 0; slot < m_engine.getNumSlots() && firstPlugin < 0; ++slot)
    {
        if (m_engine.getPlugin(slot) != nullptr)
        {
            firstPlugin = slot;
        }
    }
    // by role: a master that is a plugin is stored as "first plugin" (the slots of another session hold other plugins)
    juce::ignoreUnused(firstPlugin);
    preset.nullMaster = master < 0 ? "none" : master == 0 ? "dry" : "first plugin";
    return preset;
}

void ExpertPanel::applyPreset(const MeasurementPreset& preset)
{
    for (const juce::String& key : engine::getMeasurementUnitKeys())
    {
        setUnitChosen(key, preset.units.contains(key));
    }
    int firstPlugin = -1;
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        const bool plugin = m_engine.getPlugin(slot) != nullptr;
        setSlotChosen(slot, plugin || (slot == 0 && preset.includeDry));
        if (plugin && firstPlugin < 0)
        {
            firstPlugin = slot;
        }
    }
    setNullMaster(preset.nullMaster == "dry" ? 0 : preset.nullMaster == "first plugin" ? firstPlugin : -1);
    m_presetNote.setText(preset.note, juce::dontSendNotification);
    setStatus("Preset \"" + preset.name + "\" chosen.");
}

bool ExpertPanel::selectPreset(const juce::String& name)
{
    refreshPresets();
    for (size_t index = 0; index < m_presets.size(); ++index)
    {
        if (m_presets[index].name == name)
        {
            m_presetBox.setSelectedItemIndex(static_cast<int>(index), juce::dontSendNotification);
            applyPreset(m_presets[index]);
            return true;
        }
    }
    return false;
}

void ExpertPanel::refreshPresets(const juce::String& select)
{
    m_presets = loadMeasurementPresets(m_settings);
    m_presetBox.clear(juce::dontSendNotification);
    for (size_t index = 0; index < m_presets.size(); ++index)
    {
        m_presetBox.addItem(m_presets[index].name + (m_presets[index].shipped ? "  (shipped)" : ""), static_cast<int>(index) + 1);
        if (m_presets[index].name == select)
        {
            m_presetBox.setSelectedItemIndex(static_cast<int>(index), juce::dontSendNotification);
        }
    }
    m_presetBox.setTextWhenNothingSelected("choose a preset ...");
}

void ExpertPanel::askToSavePreset()
{
    m_saveDialog = std::make_unique<juce::AlertWindow>("Save as preset", "The units, the slots by role and the null test of this page.",
                                                       juce::MessageBoxIconType::NoIcon, this);
    m_saveDialog->addTextEditor("name", "My measurement", "Name:");
    m_saveDialog->addTextEditor("note", "", "Note:");
    m_saveDialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    m_saveDialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<ExpertPanel> self(this);
    m_saveDialog->enterModalState(true, juce::ModalCallbackFunction::create([self](int result) {
        if (self == nullptr || self->m_saveDialog == nullptr)
        {
            return;
        }
        const juce::String name = self->m_saveDialog->getTextEditorContents("name").trim();
        const juce::String note = self->m_saveDialog->getTextEditorContents("note").trim();
        self->m_saveDialog->setVisible(false);
        if (result == 1 && name.isNotEmpty())
        {
            const bool saved = saveMeasurementPreset(self->m_settings, self->makePreset(name, note));
            self->refreshPresets(name);
            self->setStatus(saved ? "Preset \"" + name + "\" saved." : "The preset could not be saved.");
        }
    }), false);
}

void ExpertPanel::startMeasurement()
{
    if (m_job != nullptr)
    {
        return;
    }
    auto root = std::make_unique<juce::DynamicObject>();
    juce::Array<juce::var> slots;
    int nullMaster = -1;
    const int chosenMaster = m_nullBox.getSelectedId() - kNoNullItem - 1;
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        if (! m_slotChosen[static_cast<size_t>(slot)] && slot != chosenMaster)
        {
            continue;
        }
        auto entry = std::make_unique<juce::DynamicObject>();
        entry->setProperty("name", juce::String(slot + 1) + ": " + m_engine.getSlotInfo(slot).name);
        if (hosting::HostedPlugin* plugin = m_engine.getPlugin(slot))
        {
            entry->setProperty("plugin", plugin->getDescription().fileOrIdentifier);
            juce::Array<juce::var> values;
            for (const float value : engine::getSetting(plugin->getInstance()))
            {
                values.add(value);
            }
            entry->setProperty("values", values);
        }
        if (slot == chosenMaster)
        {
            nullMaster = slots.size();
        }
        slots.add(juce::var(entry.release()));
    }
    if (slots.isEmpty())
    {
        setStatus("Choose at least one slot.");
        return;
    }
    juce::Array<juce::var> units;
    bool linearity = false;
    for (const auto& toggle : m_unitToggles)
    {
        if (toggle->getToggleState())
        {
            units.add(toggle->getComponentID());
            linearity = linearity || toggle->getComponentID() == "linearity";
        }
    }
    if (units.isEmpty())
    {
        units.add("none"); // only the system class
    }
    root->setProperty("slots", slots);
    root->setProperty("units", units);
    root->setProperty("linearity", linearity);
    root->setProperty("sampleRate", m_engine.getSampleRate());
    root->setProperty("nullMaster", nullMaster);
    const juce::File folder = m_settings.getExpertFolder();
    folder.createDirectory();
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    const juce::File job = folder.getChildFile("expert_" + stamp + ".json");
    job.replaceWithText(juce::JSON::toString(juce::var(root.release())));
    m_lastReport = folder.getChildFile("expert_" + stamp + ".md");
    m_fraction = 0.0;
    m_step = "starting";
    m_startedMs = juce::Time::getMillisecondCounter();
    const juce::Component::SafePointer<ExpertPanel> self(this);
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
                self->measurementFinished(success, message);
            }
        },
        "--expert");
    m_measureButton.setEnabled(false);
    setStatus("Measuring in a process of its own ...");
}

bool ExpertPanel::isMeasuring() const
{
    return m_job != nullptr;
}

void ExpertPanel::measurementFinished(bool success, const juce::String& message)
{
    m_job.reset();
    m_fraction = -1.0;
    m_measureButton.setEnabled(true);
    repaint(m_progressArea);
    if (! success)
    {
        setStatus("The measurement failed: " + message);
        return;
    }
    const juce::var result = juce::JSON::parse(juce::File(m_lastReport.getFullPathName() + ".json").loadFileAsString());
    m_results.clear();
    if (const juce::Array<juce::var>* slots = result.getProperty("slots", {}).getArray())
    {
        for (const juce::var& entry : *slots)
        {
            engine::MeasurementSummary summary = engine::parseMeasurementJson(entry);
            summary.deviceName = entry.getProperty("name", summary.deviceName).toString();
            m_results.push_back(summary);
        }
    }
    m_nulls.clear();
    m_nullMasterName = result.getProperty("nullMaster", {}).toString();
    if (const juce::Array<juce::var>* nulls = result.getProperty("nulls", {}).getArray())
    {
        for (const juce::var& entry : *nulls)
        {
            const double depth = entry.getProperty("nullDepthDb", 0.0);
            m_nulls.push_back({entry.getProperty("name", "").toString(),
                               (depth <= kIdenticalDb ? juce::String("identical") : juce::String(depth, 1) + " dB") + "|"
                                   + juce::String(static_cast<double>(entry.getProperty("delaySamples", 0.0)), 2) + " samples, "
                                   + juce::String(static_cast<double>(entry.getProperty("gainDb", 0.0)), 2) + " dB"
                                   + (static_cast<bool>(entry.getProperty("inverted", false)) ? ", inverted" : "")});
        }
    }
    rebuildTable();
    setStatus("Done: " + juce::String(static_cast<int>(m_results.size())) + " slot(s) measured. \"View report\" shows the report with the clauses.");
    if (onMeasurementFinished)
    {
        onMeasurementFinished();
    }
}

void ExpertPanel::rebuildTable()
{
    m_quantities.clear();
    for (const engine::MeasurementSummary& summary : m_results)
    {
        for (const engine::MeasurementRow& row : summary.rows)
        {
            m_quantities.addIfNotAlreadyThere(row.quantity);
        }
    }
    if (! m_nulls.empty())
    {
        m_quantities.add(kNullDepthRow + m_nullMasterName);
        m_quantities.add(kNullAlignmentRow);
    }
    juce::TableHeaderComponent& header = m_table.getHeader();
    header.removeAllColumns();
    header.addColumn("Quantity", kColumnQuantity, kWidthQuantity);
    for (size_t index = 0; index < m_results.size(); ++index)
    {
        header.addColumn(m_results[index].deviceName, kColumnQuantity + 1 + static_cast<int>(index), kWidthSlot);
    }
    m_table.updateContent();
    m_table.repaint();
}

int ExpertPanel::getNumResultRows() const
{
    return m_quantities.size();
}

juce::String ExpertPanel::getCellText(int row, int column) const
{
    if (row < 0 || row >= m_quantities.size())
    {
        return {};
    }
    const juce::String& quantity = m_quantities[row];
    if (column == 0)
    {
        return quantity;
    }
    const auto index = static_cast<size_t>(column - 1);
    if (index >= m_results.size())
    {
        return {};
    }
    const juce::String& slotName = m_results[index].deviceName;
    if (quantity.startsWith(kNullDepthRow) || quantity == kNullAlignmentRow)
    {
        if (slotName == m_nullMasterName)
        {
            return "(the master)";
        }
        for (const NullRow& entry : m_nulls)
        {
            if (entry.name == slotName)
            {
                return quantity == kNullAlignmentRow ? entry.text.fromFirstOccurrenceOf("|", false, false) : entry.text.upToFirstOccurrenceOf("|", false, false);
            }
        }
        return {};
    }
    if (const engine::MeasurementRow* found = engine::findRow(m_results[index], quantity))
    {
        return found->text;
    }
    return "-";
}

int ExpertPanel::getNumRows()
{
    return getNumResultRows();
}

void ExpertPanel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused(width, height);
    if (rowIsSelected)
    {
        g.fillAll(juce::Colour(0xff4060a0));
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(juce::Colours::white.withAlpha(0.04f));
    }
}

void ExpertPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused(rowIsSelected);
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
    g.drawText(getCellText(rowNumber, columnId - kColumnQuantity), kMargin / 2, 0, width - kMargin, height, juce::Justification::centredLeft, true);
}

void ExpertPanel::setStatus(const juce::String& text)
{
    m_status.setText(text, juce::dontSendNotification);
}

void ExpertPanel::timerCallback()
{
    m_reportButton.setEnabled(m_job == nullptr && m_lastReport.existsAsFile());
    if (m_job != nullptr)
    {
        repaint(m_progressArea);
    }
}

void ExpertPanel::paint(juce::Graphics& g)
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

void ExpertPanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin * 2);
    m_title.setBounds(area.removeFromTop(kRowHeight));
    area.removeFromTop(kMargin);
    juce::Rectangle<int> presetRow = area.removeFromTop(kRowHeight);
    m_presetLabel.setBounds(presetRow.removeFromLeft(kLabelWidth));
    m_presetBox.setBounds(presetRow.removeFromLeft(kToggleWidth + 60));
    presetRow.removeFromLeft(kMargin);
    m_savePresetButton.setBounds(presetRow.removeFromLeft(kButtonWidth - 20));
    presetRow.removeFromLeft(kMargin);
    m_presetNote.setBounds(presetRow);
    area.removeFromTop(kMargin);

    // the slots and the units as rows of toggles that wrap
    const auto layOutToggles = [&area](juce::Label& label, std::vector<juce::Component*> toggles) {
        juce::Rectangle<int> row = area.removeFromTop(kRowHeight);
        label.setBounds(row.removeFromLeft(kLabelWidth));
        for (juce::Component* toggle : toggles)
        {
            if (row.getWidth() < kToggleWidth)
            {
                row = area.removeFromTop(kRowHeight).withTrimmedLeft(kLabelWidth);
            }
            toggle->setBounds(row.removeFromLeft(kToggleWidth));
        }
        area.removeFromTop(kMargin);
    };
    std::vector<juce::Component*> slots;
    for (const auto& toggle : m_slotToggles)
    {
        slots.push_back(toggle.get());
    }
    layOutToggles(m_slotsLabel, slots);
    std::vector<juce::Component*> units{&m_systemClassNote};
    for (const auto& toggle : m_unitToggles)
    {
        units.push_back(toggle.get());
    }
    layOutToggles(m_unitsLabel, units);

    juce::Rectangle<int> row = area.removeFromTop(kRowHeight);
    m_nullLabel.setBounds(row.removeFromLeft(kLabelWidth));
    m_nullBox.setBounds(row.removeFromLeft(kToggleWidth + 60));
    area.removeFromTop(kMargin);

    row = area.removeFromTop(kRowHeight);
    for (juce::TextButton* button : {&m_measureButton, &m_reportButton, &m_matcherButton, &m_compareButton})
    {
        button->setBounds(row.removeFromLeft(kButtonWidth));
        row.removeFromLeft(kMargin);
    }
    area.removeFromTop(kMargin);
    m_progressArea = area.removeFromTop(kProgressHeight);
    area.removeFromTop(kMargin);
    m_status.setBounds(area.removeFromBottom(kRowHeight));
    m_table.setBounds(area);
}
}
