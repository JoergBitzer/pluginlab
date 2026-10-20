#include "MatcherPanel.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "LiveWorker.h"
#include "MatchJob.h"
#include "PageServices.h"
#include "pluginlab/engine/LiveResponse.h"
#include "pluginlab/engine/Mapping.h"
#include "pluginlab/engine/OfflineRenderer.h"
#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/PluginDisplayName.h"
#include "pluginlab/match/EqDomain.h"

namespace pluginlab::host
{
namespace
{
constexpr int kLiveMaster = 0;            // the slots of the live worker
constexpr int kLiveTarget = 1;
constexpr int kMargin = 8;
constexpr int kRowHeight = 26;
constexpr int kLabelWidth = 70;
constexpr int kBoxWidth = 220;
constexpr int kSmallBoxWidth = 120;
constexpr int kSliderWidth = 110;
constexpr int kTableHeight = 150;
constexpr int kProgressHeight = 22;
constexpr int kButtonWidth = 140;
constexpr int kColumnMatch = 1;
constexpr int kColumnPlugin = 2;
constexpr int kColumnReleased = 3;
constexpr int kColumnChoose = 4;
constexpr int kWidthMatch = 60;
constexpr int kWidthPlugin = 240;
constexpr int kWidthReleased = 520;
constexpr int kWidthChoose = 90;
constexpr int kLiveIntervalMs = 300;            // the live view reads the knobs this often (no audio plays while matching)
constexpr int kCurveItem = 1;                   // the first item of the master box
constexpr double kPlotRangeDb = 24.0;           // the curves
constexpr double kDifferenceRangeDb = 2.0;      // the difference below them
constexpr double kSampleRate = 48000.0;
const juce::Colour kMasterColour(0xffe0e0e0);
const juce::Colour kTargetColour(0xff4fa3e0);
const juce::Colour kDifferenceColour(0xffffa040);
const juce::Colour kProgressColour(0xff3a6ea5);
const juce::Colour kProgressBackground(0xff2a2a2a);
const juce::StringArray kCurveTypes = {"peak", "low shelf", "high shelf", "low-pass", "high-pass"};
const juce::StringArray kToleranceNames = {"exact (0.01 dB)", "strict (0.1 dB)", "medium (0.25 dB)", "loose (0.5 dB)"};
constexpr int kDefaultTolerance = 2;            // strict (combo box ids start at 1)

void setUpSlider(juce::Slider& slider, double low, double high, double value, double interval, bool logarithmic, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearBar);
    slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, kSliderWidth, kRowHeight);
    slider.setRange(low, high, interval);
    if (logarithmic)
    {
        slider.setSkewFactorFromMidPoint(std::sqrt(low * high));
    }
    slider.setValue(value, juce::dontSendNotification);
    slider.setTextValueSuffix(suffix);
}
}

void MatcherPanel::Plot::setCurves(std::vector<double> frequencies, std::vector<double> master, std::vector<double> target, juce::String caption)
{
    m_frequencies = std::move(frequencies);
    m_master = std::move(master);
    m_target = std::move(target);
    m_caption = std::move(caption);
    repaint();
}

void MatcherPanel::Plot::paint(juce::Graphics& g)
{
    const juce::Colour text = getLookAndFeel().findColour(juce::Label::textColourId);
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(0.2f));
    juce::Rectangle<float> area = getLocalBounds().toFloat().reduced(static_cast<float>(kMargin));
    area.removeFromLeft(40.0f);
    area.removeFromBottom(16.0f);
    juce::Rectangle<float> curves = area.removeFromTop(area.getHeight() * 0.7f);
    area.removeFromTop(6.0f);
    const juce::Rectangle<float> difference = area;
    const double lowest = 20.0;
    const double highest = m_frequencies.empty() ? 20000.0 : m_frequencies.back();
    const auto toX = [&](double frequency) { return curves.getX() + static_cast<float>(std::log(frequency / lowest) / std::log(highest / lowest)) * curves.getWidth(); };
    g.setFont(juce::FontOptions(12.0f));
    for (const juce::Rectangle<float>& box : {curves, difference})
    {
        g.setColour(text.withAlpha(0.3f));
        g.drawRect(box);
    }
    for (double decade = 10.0; decade < highest; decade *= 10.0)
    {
        for (const double multiple : {1.0, 2.0, 5.0})
        {
            const double frequency = decade * multiple;
            if (frequency < lowest || frequency > highest)
            {
                continue;
            }
            const float x = toX(frequency);
            g.setColour(text.withAlpha(0.15f));
            g.drawVerticalLine(juce::roundToInt(x), curves.getY(), difference.getBottom());
            g.setColour(text.withAlpha(0.8f));
            const juce::String label = frequency >= 1000.0 ? juce::String(juce::roundToInt(frequency / 1000.0)) + "k" : juce::String(juce::roundToInt(frequency));
            g.drawText(label, juce::Rectangle<float>(x - 20.0f, difference.getBottom() + 2.0f, 40.0f, 14.0f), juce::Justification::centredTop);
        }
    }
    const auto drawLevels = [&](const juce::Rectangle<float>& box, double range, std::initializer_list<double> levels) {
        for (const double level : levels)
        {
            const float y = box.getCentreY() - static_cast<float>(level / range) * box.getHeight() / 2.0f;
            g.setColour(text.withAlpha(0.15f));
            g.drawHorizontalLine(juce::roundToInt(y), box.getX(), box.getRight());
            g.setColour(text.withAlpha(0.8f));
            g.drawText(juce::String(level, range < 5.0 ? 1 : 0), juce::Rectangle<float>(box.getX() - 40.0f, y - 7.0f, 36.0f, 14.0f), juce::Justification::centredRight);
        }
    };
    drawLevels(curves, kPlotRangeDb, {-24.0, -12.0, 0.0, 12.0, 24.0});
    drawLevels(difference, kDifferenceRangeDb, {-2.0, 0.0, 2.0});
    const auto drawCurve = [&](const std::vector<double>& values, const juce::Rectangle<float>& box, double range, juce::Colour colour) {
        if (values.size() != m_frequencies.size() || values.empty())
        {
            return;
        }
        juce::Path path;
        for (size_t index = 0; index < values.size(); ++index)
        {
            const float x = toX(m_frequencies[index]);
            const float y = box.getCentreY() - static_cast<float>(juce::jlimit(-range, range, values[index]) / range) * box.getHeight() / 2.0f;
            index == 0 ? path.startNewSubPath(x, y) : path.lineTo(x, y);
        }
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(2.0f));
    };
    drawCurve(m_master, curves, kPlotRangeDb, kMasterColour);
    drawCurve(m_target, curves, kPlotRangeDb, kTargetColour);
    if (m_master.size() == m_target.size())
    {
        std::vector<double> deviation(m_master.size());
        for (size_t index = 0; index < deviation.size(); ++index)
        {
            deviation[index] = m_target[index] - m_master[index];
        }
        drawCurve(deviation, difference, kDifferenceRangeDb, kDifferenceColour);
    }
    g.setColour(text);
    g.drawText(m_caption, curves.reduced(6.0f).removeFromTop(16.0f), juce::Justification::left);
    g.setColour(kMasterColour);
    g.drawText("master", curves.reduced(6.0f).removeFromTop(16.0f), juce::Justification::right);
    g.setColour(kTargetColour);
    g.drawText("target", curves.reduced(6.0f).withTrimmedTop(16.0f).removeFromTop(16.0f), juce::Justification::right);
    g.setColour(kDifferenceColour);
    g.drawText("target - master (dB)", difference.reduced(4.0f).removeFromTop(14.0f), juce::Justification::right);
}

MatcherPanel::MatcherPanel(pluginlab::engine::MeasurementEngine& engine, HostSettings& settings)
    : m_engine(engine), m_settings(settings)
{
    hosting::addHeadlessFormats(m_formatManager);
    for (juce::Label* label : {&m_masterLabel, &m_toleranceLabel, &m_rangeLabel, &m_secondsLabel})
    {
        addAndMakeVisible(label);
    }
    addAndMakeVisible(m_masterBox);
    m_masterBox.onChange = [this] {
        rebuildTargets();
        rebuildLive();
    };
    m_curveType.addItemList(kCurveTypes, 1);
    m_curveType.setSelectedId(1, juce::dontSendNotification);
    setUpSlider(m_curveFrequency, 20.0, 20000.0, 1000.0, 1.0, true, " Hz");
    setUpSlider(m_curveGain, -24.0, 24.0, 6.0, 0.1, false, " dB");
    setUpSlider(m_curveQ, 0.1, 10.0, 1.0, 0.01, true, " Q");
    for (juce::Component* control : std::initializer_list<juce::Component*>{&m_curveType, &m_curveFrequency, &m_curveGain, &m_curveQ})
    {
        addAndMakeVisible(control);
    }
    m_curveType.onChange = [this] { rebuildLive(); };
    for (juce::Slider* slider : {&m_curveFrequency, &m_curveGain, &m_curveQ})
    {
        slider->onDragEnd = [this] { rebuildLive(); };
        slider->onValueChange = [this] {
            if (! isMouseButtonDownAnywhere())
            {
                rebuildLive();
            }
        };
    }
    m_toleranceBox.addItemList(kToleranceNames, 1);
    m_toleranceBox.setSelectedId(kDefaultTolerance, juce::dontSendNotification);
    addAndMakeVisible(m_toleranceBox);
    setUpSlider(m_lowest, 20.0, 1000.0, 20.0, 1.0, true, " Hz");
    setUpSlider(m_highest, 1000.0, 20000.0, 20000.0, 1.0, true, " Hz");
    setUpSlider(m_seconds, 5.0, 60.0, 15.0, 1.0, false, " s");
    for (juce::Component* control : std::initializer_list<juce::Component*>{&m_lowest, &m_highest, &m_seconds, &m_modelStart})
    {
        addAndMakeVisible(control);
    }
    m_table.setModel(this);
    m_table.setRowHeight(kRowHeight);
    m_table.getHeader().addColumn("Match", kColumnMatch, kWidthMatch);
    m_table.getHeader().addColumn("Target", kColumnPlugin, kWidthPlugin);
    m_table.getHeader().addColumn("Released parameters (the matcher moves only these)", kColumnReleased, kWidthReleased);
    m_table.getHeader().addColumn("", kColumnChoose, kWidthChoose);
    addAndMakeVisible(m_table);
    addAndMakeVisible(m_plot);
    m_matchButton.onClick = [this] { startMatch(); };
    m_reportButton.onClick = [this] { showLastReport(); };
    m_applyButton.onClick = [this] { applyResults(); };
    m_saveButton.onClick = [this] { savePresets(); };
    m_listenButton.onClick = [this] {
        if (onListen)
        {
            onListen();
        }
    };
    for (juce::Component* button : std::initializer_list<juce::Component*>{&m_matchButton, &m_reportButton, &m_applyButton, &m_saveButton, &m_listenButton, &m_status})
    {
        addAndMakeVisible(button);
    }
    m_status.setText("Load the plugins on the page Plugins; choose the master and release the target's parameters. No audio plays while matching.",
                     juce::dontSendNotification);
    slotsChanged();
    startTimer(kLiveIntervalMs);
}

MatcherPanel::~MatcherPanel()
{
    stopTimer();
    m_live.reset();
    m_job.reset();
    m_table.setModel(nullptr);
}

void MatcherPanel::slotsChanged()
{
    m_live.reset(); // the loaded plugins changed: the copies are made again
    const int previous = getMasterSlot();
    m_masterSlots.clear();
    m_masterBox.clear(juce::dontSendNotification);
    m_masterBox.addItem("band curve (typed in)", kCurveItem);
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        if (hosting::HostedPlugin* plugin = m_engine.getPlugin(slot))
        {
            m_masterSlots.push_back(slot);
            m_masterBox.addItem(hosting::getDisplayName(plugin->getDescription()) + " (as it is set)", kCurveItem + static_cast<int>(m_masterSlots.size()));
        }
    }
    int selected = kCurveItem;
    for (size_t index = 0; index < m_masterSlots.size(); ++index)
    {
        if (m_masterSlots[index] == previous)
        {
            selected = kCurveItem + 1 + static_cast<int>(index);
        }
    }
    m_masterBox.setSelectedId(selected, juce::dontSendNotification);
    rebuildTargets();
    rebuildLive();
}

void MatcherPanel::chooseFirstAsMaster()
{
    if (! m_masterSlots.empty())
    {
        m_masterBox.setSelectedId(kCurveItem + 1, juce::sendNotificationSync);
    }
}

int MatcherPanel::getMasterSlot() const
{
    const int id = m_masterBox.getSelectedId();
    if (id <= kCurveItem || id - kCurveItem - 1 >= static_cast<int>(m_masterSlots.size()))
    {
        return -1;
    }
    return m_masterSlots[static_cast<size_t>(id - kCurveItem - 1)];
}

void MatcherPanel::rebuildTargets()
{
    const int master = getMasterSlot();
    std::vector<TargetRow> rows;
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(slot);
        if (plugin == nullptr || slot == master)
        {
            continue;
        }
        TargetRow row;
        row.slot = slot;
        row.key = plugin->getDescription().createIdentifierString();
        const auto known = std::find_if(m_targets.begin(), m_targets.end(), [&row](const TargetRow& old) { return old.key == row.key; });
        if (known != m_targets.end())
        {
            row.match = known->match;
            row.released = known->released;
            row.source = known->source;
        }
        else
        {
            // the band parameters of the mapping (W8.5) are released by default; without a mapping the user chooses
            row.released = getMappedParameters(m_settings, plugin->getDescription());
            if (! row.released.empty())
            {
                row.source = "from the mapping";
            }
        }
        rows.push_back(row);
    }
    m_targets = rows;
    m_table.updateContent();
    m_table.repaint();
    if (! m_targets.empty() && m_table.getSelectedRow() < 0)
    {
        m_table.selectRow(0);
    }
    const bool curve = master < 0;
    for (juce::Component* control : std::initializer_list<juce::Component*>{&m_curveType, &m_curveFrequency, &m_curveGain, &m_curveQ})
    {
        control->setVisible(curve);
    }
    updateButtons();
}

void MatcherPanel::rebuildLive()
{
    m_live.reset();
    m_liveGenerations = {-1, -1};
    const int row = m_table.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(m_targets.size()))
    {
        m_plot.setCurves({}, {}, {}, "no target");
        return;
    }
    hosting::HostedPlugin* target = m_engine.getPlugin(m_targets[static_cast<size_t>(row)].slot);
    if (target == nullptr)
    {
        return;
    }
    // the live deviation: copies of the master and the target in the live worker (W10.7: a crash there does not end the program); the band
    // curve as master is computed here on the worker's grid
    m_live = std::make_unique<RemoteLiveResponse>(kSampleRate);
    std::optional<juce::PluginDescription> master;
    const int masterSlot = getMasterSlot();
    if (masterSlot >= 0 && m_engine.getPlugin(masterSlot) != nullptr)
    {
        master = m_engine.getPlugin(masterSlot)->getDescription();
    }
    m_liveCurve.clear();
    if (! master)
    {
        eqmodel::Band band;
        band.type = static_cast<reference::FilterType>(std::array<int, 5>{6, 7, 8, 0, 1}[static_cast<size_t>(std::max(0, m_curveType.getSelectedItemIndex()))]);
        band.frequencyHz = m_curveFrequency.getValue();
        band.gainDb = m_curveGain.getValue();
        band.q = m_curveQ.getValue();
        m_liveCurve = match::getCurveMagnitude(eqmodel::EqModel{{band}, 0.0}, m_live->getFrequencies(), kSampleRate);
    }
    m_live->setSlots({master, std::optional<juce::PluginDescription>(target->getDescription())});
    timerCallback();
}

void MatcherPanel::timerCallback()
{
    if (m_job != nullptr)
    {
        repaint(m_progressArea); // the elapsed time
    }
    // the knobs of the loaded plugins, read here on the message thread and handed to the live worker; its new curves shown
    if (m_live == nullptr)
    {
        return;
    }
    const int row = m_table.getSelectedRow();
    if (row < 0 || row >= static_cast<int>(m_targets.size()))
    {
        return;
    }
    hosting::HostedPlugin* target = m_engine.getPlugin(m_targets[static_cast<size_t>(row)].slot);
    if (target == nullptr)
    {
        return;
    }
    const int masterSlot = getMasterSlot();
    const bool pluginMaster = masterSlot >= 0 && m_engine.getPlugin(masterSlot) != nullptr;
    if (pluginMaster)
    {
        m_live->setSetting(kLiveMaster, engine::getSetting(m_engine.getPlugin(masterSlot)->getInstance()));
    }
    m_live->setSetting(kLiveTarget, engine::getSetting(target->getInstance()));
    const engine::LiveSlotResponse masterResponse = m_live->getResponse(kLiveMaster);
    const engine::LiveSlotResponse targetResponse = m_live->getResponse(kLiveTarget);
    if (targetResponse.error.isNotEmpty())
    {
        m_plot.setCurves({}, {}, {}, "live view: " + targetResponse.error);
        return;
    }
    const std::array<int, 2> generations{pluginMaster ? masterResponse.generation : 0, targetResponse.generation};
    if (targetResponse.generation == 0 || (pluginMaster && masterResponse.generation == 0) || generations == m_liveGenerations)
    {
        return;
    }
    m_liveGenerations = generations;
    liveUpdate(m_live->getFrequencies(), pluginMaster ? masterResponse.magnitudeDb : m_liveCurve, targetResponse.magnitudeDb);
}

void MatcherPanel::liveUpdate(const std::vector<double>& frequencies, const std::vector<double>& master, const std::vector<double>& target)
{
    double largest = 0.0;
    double sum = 0.0;
    for (size_t index = 0; index < master.size() && index < target.size(); ++index)
    {
        const double difference = target[index] - master[index];
        largest = std::max(largest, std::abs(difference));
        sum += difference * difference;
    }
    const double rms = master.empty() ? 0.0 : std::sqrt(sum / static_cast<double>(master.size()));
    m_plot.setCurves(frequencies, master, target, "live: largest deviation " + juce::String(largest, 2) + " dB, rms " + juce::String(rms, 2) + " dB");
}

juce::String MatcherPanel::writeJob(const juce::File& jobFile) const
{
    MatchJobSpec spec;
    const int masterSlot = getMasterSlot();
    if (masterSlot >= 0 && m_engine.getPlugin(masterSlot) != nullptr)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(masterSlot);
        spec.masterPlugin = plugin->getDescription().fileOrIdentifier;
        spec.masterValues = engine::getSetting(plugin->getInstance());
    }
    else
    {
        auto band = std::make_unique<juce::DynamicObject>();
        band->setProperty("family", "RBJ cookbook");
        band->setProperty("type", kCurveTypes[std::max(0, m_curveType.getSelectedItemIndex())]);
        band->setProperty("frequency", m_curveFrequency.getValue());
        band->setProperty("gain", m_curveGain.getValue());
        band->setProperty("q", m_curveQ.getValue());
        spec.masterCurve = juce::Array<juce::var>{juce::var(band.release())};
    }
    juce::String problem;
    for (const TargetRow& row : m_targets)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(row.slot);
        if (! row.match || plugin == nullptr)
        {
            continue;
        }
        if (row.released.empty())
        {
            problem = hosting::getDisplayName(plugin->getDescription()) + ": no parameter released";
            continue;
        }
        spec.targets.push_back({plugin->getDescription().fileOrIdentifier, engine::getSetting(plugin->getInstance()), row.released, m_modelStart.getToggleState()});
    }
    spec.tolerance = kToleranceNames[std::max(0, m_toleranceBox.getSelectedItemIndex())].upToFirstOccurrenceOf(" ", false, false);
    spec.seconds = m_seconds.getValue();
    spec.lowestHz = m_lowest.getValue();
    spec.highestHz = m_highest.getValue();
    if (spec.targets.empty() && problem.isNotEmpty())
    {
        return problem;
    }
    return writeMatchJob(spec, jobFile);
}

void MatcherPanel::startMatch()
{
    if (m_job != nullptr)
    {
        return;
    }
    const juce::File folder = m_settings.getMatcherFolder();
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
    const juce::File job = folder.getChildFile("match_" + stamp + ".json");
    const juce::String problem = writeJob(job);
    if (problem.isNotEmpty())
    {
        m_status.setText("Cannot match: " + problem + ".", juce::dontSendNotification);
        return;
    }
    m_live.reset(); // the copies of the live view would compete with the match for the processor
    m_lastReport = folder.getChildFile("match_" + stamp + ".md");
    m_fraction = 0.0;
    m_step = "starting";
    m_startedMs = juce::Time::getMillisecondCounter();
    const juce::Component::SafePointer<MatcherPanel> self(this);
    m_job = std::make_unique<MatchJob>(
        job, m_lastReport,
        [self](double fraction, const juce::String& step) {
            if (self != nullptr)
            {
                self->jobProgress(fraction, step);
            }
        },
        [self](bool success, const juce::String& message) {
            if (self != nullptr)
            {
                self->jobFinished(success, message);
            }
        });
    m_status.setText("Matching in a process of its own ...", juce::dontSendNotification);
    updateButtons();
    repaint(m_progressArea);
}

void MatcherPanel::jobProgress(double fraction, const juce::String& step)
{
    if (fraction < 0.0)
    {
        return; // the child has not written its progress yet: the bar stays at "starting"
    }
    m_fraction = fraction;
    m_step = step;
    repaint(m_progressArea);
}

void MatcherPanel::jobFinished(bool success, const juce::String& message)
{
    m_job.reset();
    m_fraction = -1.0;
    repaint(m_progressArea);
    m_status.setText(success ? "Done. \"View report\" shows the result; \"Apply to targets\" sets the found values." : "Failed: " + message,
                     juce::dontSendNotification);
    updateButtons();
    rebuildLive();
    if (success && onMatchFinished)
    {
        onMatchFinished();
    }
}

void MatcherPanel::showLastReport()
{
    if (m_lastReport.existsAsFile() && onShowReport)
    {
        onShowReport("Match: " + m_lastReport.getFileNameWithoutExtension(), m_lastReport);
    }
}

void MatcherPanel::applyResults()
{
    std::vector<int> slots;
    for (const TargetRow& row : m_targets)
    {
        slots.push_back(row.slot);
    }
    const int applied = applyMatchOutcomes(m_engine, readMatchResult(m_lastReport), slots);
    m_status.setText("Applied to " + juce::String(applied) + " target(s). \"Listen\" compares them on the page EQ Compare.", juce::dontSendNotification);
}

void MatcherPanel::savePresets()
{
    const juce::String master = getMasterSlot() >= 0 && m_engine.getPlugin(getMasterSlot()) != nullptr
                                    ? hosting::getDisplayName(m_engine.getPlugin(getMasterSlot())->getDescription())
                                    : juce::String("band curve");
    std::vector<int> slots;
    for (const TargetRow& row : m_targets)
    {
        if (row.match)
        {
            slots.push_back(row.slot);
        }
    }
    const int saved = saveMatchedPresets(m_engine, m_settings, slots, master);
    m_status.setText("Saved " + juce::String(saved) + " preset(s) in " + m_settings.getPresetFolder().getFullPathName() + ".", juce::dontSendNotification);
}

void MatcherPanel::updateButtons()
{
    const bool busy = m_job != nullptr;
    m_matchButton.setEnabled(! busy && ! m_targets.empty());
    const bool haveResult = m_lastReport.existsAsFile() && ! busy;
    m_reportButton.setEnabled(haveResult);
    m_applyButton.setEnabled(haveResult);
    m_saveButton.setEnabled(! busy && ! m_targets.empty());
}

void MatcherPanel::chooseParameters(int row)
{
    if (row < 0 || row >= static_cast<int>(m_targets.size()))
    {
        return;
    }
    hosting::HostedPlugin* plugin = m_engine.getPlugin(m_targets[static_cast<size_t>(row)].slot);
    if (plugin == nullptr)
    {
        return;
    }
    juce::PopupMenu menu;
    const std::vector<int>& released = m_targets[static_cast<size_t>(row)].released;
    for (int index = 0; index < plugin->getNumParameters(); ++index)
    {
        const hosting::ParameterInfo info = plugin->getParameter(index);
        const bool ticked = std::find(released.begin(), released.end(), index) != released.end();
        menu.addItem(index + 1, info.name + "  (" + info.valueText + ")", true, ticked);
    }
    const juce::Component::SafePointer<MatcherPanel> self(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&m_table), [self, row](int chosen) {
        if (self == nullptr || chosen <= 0 || row >= static_cast<int>(self->m_targets.size()))
        {
            return;
        }
        std::vector<int>& list = self->m_targets[static_cast<size_t>(row)].released;
        const int index = chosen - 1;
        const auto found = std::find(list.begin(), list.end(), index);
        if (found != list.end())
        {
            list.erase(found);
        }
        else
        {
            list.push_back(index);
            std::sort(list.begin(), list.end());
        }
        self->m_targets[static_cast<size_t>(row)].source = "chosen";
        self->m_table.repaintRow(row);
        self->chooseParameters(row); // open again: several parameters in a row
    });
}

int MatcherPanel::getNumRows()
{
    return static_cast<int>(m_targets.size());
}

void MatcherPanel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused(rowNumber, width, height);
    if (rowIsSelected)
    {
        g.fillAll(juce::Colour(0xff4060a0));
    }
}

void MatcherPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused(rowIsSelected);
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_targets.size()))
    {
        return;
    }
    const TargetRow& row = m_targets[static_cast<size_t>(rowNumber)];
    hosting::HostedPlugin* plugin = m_engine.getPlugin(row.slot);
    juce::String text;
    if (columnId == kColumnMatch)
    {
        text = row.match ? "[x]" : "[ ]";
    }
    else if (columnId == kColumnPlugin && plugin != nullptr)
    {
        text = hosting::getDisplayName(plugin->getDescription());
    }
    else if (columnId == kColumnReleased && plugin != nullptr)
    {
        juce::StringArray names;
        for (const int index : row.released)
        {
            names.add(plugin->getParameter(index).name);
        }
        text = names.isEmpty() ? juce::String("none: choose ... (without a mapping the user decides)") : names.joinIntoString(", ") + "  (" + row.source + ")";
    }
    else if (columnId == kColumnChoose)
    {
        text = "choose ...";
    }
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
    g.drawText(text, kMargin / 2, 0, width - kMargin, height, juce::Justification::centredLeft, true);
}

void MatcherPanel::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_targets.size()))
    {
        return;
    }
    if (columnId == kColumnMatch)
    {
        m_targets[static_cast<size_t>(rowNumber)].match = ! m_targets[static_cast<size_t>(rowNumber)].match;
        m_table.repaintRow(rowNumber);
    }
    else if (columnId == kColumnChoose)
    {
        chooseParameters(rowNumber);
    }
}

void MatcherPanel::selectedRowsChanged(int lastRowSelected)
{
    juce::ignoreUnused(lastRowSelected);
    rebuildLive();
}

void MatcherPanel::paint(juce::Graphics& g)
{
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
    g.drawText("Match: " + juce::String(juce::roundToInt(100.0 * m_fraction)) + " %, " + m_step + ", " + juce::String(seconds / 60) + ":"
                   + juce::String(seconds % 60).paddedLeft('0', 2),
               m_progressArea.reduced(kMargin / 2, 0), juce::Justification::centredLeft, true);
}

void MatcherPanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds().reduced(kMargin);
    juce::Rectangle<int> row = area.removeFromTop(kRowHeight);
    m_masterLabel.setBounds(row.removeFromLeft(kLabelWidth));
    m_masterBox.setBounds(row.removeFromLeft(kBoxWidth + kBoxWidth / 2));
    row.removeFromLeft(kMargin);
    m_curveType.setBounds(row.removeFromLeft(kSmallBoxWidth));
    for (juce::Slider* slider : {&m_curveFrequency, &m_curveGain, &m_curveQ})
    {
        row.removeFromLeft(kMargin);
        slider->setBounds(row.removeFromLeft(kSliderWidth));
    }
    area.removeFromTop(kMargin);
    row = area.removeFromTop(kRowHeight);
    m_toleranceLabel.setBounds(row.removeFromLeft(kLabelWidth));
    m_toleranceBox.setBounds(row.removeFromLeft(kSmallBoxWidth + 20));
    row.removeFromLeft(kMargin);
    m_rangeLabel.setBounds(row.removeFromLeft(kLabelWidth + 10));
    m_lowest.setBounds(row.removeFromLeft(kSliderWidth));
    row.removeFromLeft(kMargin);
    m_highest.setBounds(row.removeFromLeft(kSliderWidth));
    row.removeFromLeft(kMargin);
    m_secondsLabel.setBounds(row.removeFromLeft(kLabelWidth + 60));
    m_seconds.setBounds(row.removeFromLeft(kSliderWidth - 30));
    row.removeFromLeft(kMargin);
    m_modelStart.setBounds(row);
    area.removeFromTop(kMargin);
    m_table.setBounds(area.removeFromTop(kTableHeight));
    area.removeFromTop(kMargin);
    row = area.removeFromBottom(kRowHeight);
    for (juce::TextButton* button : {&m_matchButton, &m_reportButton, &m_applyButton, &m_saveButton, &m_listenButton})
    {
        button->setBounds(row.removeFromLeft(kButtonWidth));
        row.removeFromLeft(kMargin);
    }
    m_status.setBounds(row);
    area.removeFromBottom(kMargin);
    m_progressArea = area.removeFromBottom(kProgressHeight);
    area.removeFromBottom(kMargin);
    m_plot.setBounds(area);
}
}
