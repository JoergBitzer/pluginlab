#include "MatcherPanel.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "PageServices.h"
#include "pluginlab/engine/LivePluginDevice.h"
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
constexpr double kLivePerOctave = 48.0; // the live view: dense up to fs / 2
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
constexpr int kPollMs = 200;
constexpr juce::uint32 kProgressIntervalMs = 1000;
constexpr int kStopTimeoutMs = 10000;
constexpr int kProcessTimeoutMs = 10 * 60 * 1000;
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

// Runs PluginLabHost --match <job> <report> in a child process and tells the page its progress and its end
class MatchJobThread : public juce::Thread
{
public:
    MatchJobThread(MatcherPanel& owner, juce::File job, juce::File report)
        : juce::Thread("MatchJob"), m_owner(owner), m_job(std::move(job)), m_report(std::move(report))
    {
    }

    ~MatchJobThread() override
    {
        signalThreadShouldExit();
        stopThread(kStopTimeoutMs);
    }

    void run() override
    {
        juce::ChildProcess process;
        const juce::File host = getHelperExecutable(); // the application, or the PluginLabHost in the plugin's bundle
        bool success = false;
        juce::String message;
        if (process.start(juce::StringArray{host.getFullPathName(), "--match", m_job.getFullPathName(), m_report.getFullPathName()}, 0))
        {
            const juce::uint32 started = juce::Time::getMillisecondCounter();
            juce::uint32 lastProgress = 0;
            while (process.isRunning() && ! threadShouldExit() && juce::Time::getMillisecondCounter() - started < static_cast<juce::uint32>(kProcessTimeoutMs))
            {
                if (juce::Time::getMillisecondCounter() - lastProgress >= kProgressIntervalMs)
                {
                    lastProgress = juce::Time::getMillisecondCounter();
                    const JobProgress progress = readProgressFile(getProgressFile(m_report));
                    post([progress](MatcherPanel& panel) { panel.jobProgress(progress.fraction, progress.step); });
                }
                wait(kPollMs);
            }
            if (process.isRunning())
            {
                process.kill();
                message = "stopped (time limit)";
            }
            success = m_report.existsAsFile() && m_report.getSize() > 0;
            if (! success && message.isEmpty())
            {
                message = "the match process ended without a report (a crash?)";
            }
        }
        else
        {
            message = "could not start the match process";
        }
        post([success, message](MatcherPanel& panel) { panel.jobFinished(success, message); });
    }

private:
    void post(std::function<void(MatcherPanel&)> call)
    {
        const juce::Component::SafePointer<MatcherPanel> owner(&m_owner);
        juce::MessageManager::callAsync([owner, call] {
            if (owner != nullptr)
            {
                call(*owner);
            }
        });
    }

    MatcherPanel& m_owner;
    juce::File m_job;
    juce::File m_report;
};

// The live deviation: copies of the master and the target (live instances of the fast path, W9.3) render the settings the page hands over
// (read from the loaded plugins on the message thread); the probe's magnitude goes back to the page. No audio, no access to the loaded plugins.
class LiveDeviationThread : public juce::Thread
{
public:
    LiveDeviationThread(MatcherPanel& owner, juce::AudioPluginFormatManager& formatManager, std::optional<juce::PluginDescription> master,
                        std::vector<double> curve, const juce::PluginDescription& target)
        : juce::Thread("LiveDeviation"), m_owner(owner), m_curve(std::move(curve))
    {
        match::MagnitudeProbeSettings settings;
        settings.sampleRate = kSampleRate;
        settings.stimulus = match::ProbeStimulus::Impulse;
        settings.upToNyquist = true; // the curves up to fs / 2 (the match itself keeps its frequency range)
        settings.perOctave = kLivePerOctave;
        m_probe = std::make_unique<match::MagnitudeProbe>(settings);
        if (master)
        {
            m_master = std::make_unique<engine::LivePluginTarget>(formatManager, *master, kSampleRate);
        }
        m_target = std::make_unique<engine::LivePluginTarget>(formatManager, target, kSampleRate);
    }

    ~LiveDeviationThread() override
    {
        signalThreadShouldExit();
        notify();
        stopThread(kStopTimeoutMs);
    }

    const std::vector<double>& getFrequencies() const { return m_probe->getFrequencies(); }

    // the current knob values of the loaded plugins (message thread); rendered on the next turn if they changed
    void setSettings(const std::vector<float>& master, const std::vector<float>& target)
    {
        const juce::ScopedLock lock(m_lock);
        if (master != m_masterSetting || target != m_targetSetting)
        {
            m_masterSetting = master;
            m_targetSetting = target;
            m_changed = true;
            notify();
        }
    }

    void run() override
    {
        while (! threadShouldExit())
        {
            std::vector<float> master;
            std::vector<float> target;
            {
                const juce::ScopedLock lock(m_lock);
                if (m_changed)
                {
                    master = m_masterSetting;
                    target = m_targetSetting;
                    m_changed = false;
                }
            }
            if (! target.empty() && m_target->isValid())
            {
                std::vector<double> masterFeatures = m_curve;
                if (m_master != nullptr && m_master->isValid() && ! master.empty())
                {
                    masterFeatures = m_probe->measure(m_master->makeDevice(master));
                }
                const std::vector<double> targetFeatures = m_probe->measure(m_target->makeDevice(target));
                const std::vector<double> frequencies = m_probe->getFrequencies();
                const juce::Component::SafePointer<MatcherPanel> owner(&m_owner);
                juce::MessageManager::callAsync([owner, frequencies, masterFeatures, targetFeatures] {
                    if (owner != nullptr)
                    {
                        owner->liveUpdate(frequencies, masterFeatures, targetFeatures);
                    }
                });
            }
            wait(-1);
        }
    }

private:
    MatcherPanel& m_owner;
    std::vector<double> m_curve;
    std::unique_ptr<match::MagnitudeProbe> m_probe;
    std::unique_ptr<engine::LivePluginTarget> m_master;
    std::unique_ptr<engine::LivePluginTarget> m_target;
    juce::CriticalSection m_lock;
    std::vector<float> m_masterSetting;
    std::vector<float> m_targetSetting;
    bool m_changed = false;
};

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
            juce::String error;
            const juce::File file = m_settings.getMappingFolder().getChildFile(engine::getMappingFileName(plugin->getDescription().name));
            if (const std::optional<engine::Mapping> mapping = engine::Mapping::read(file, error))
            {
                for (const engine::MappedParameter& parameter : mapping->parameters)
                {
                    if (parameter.quantity == engine::MappedQuantity::Frequency || parameter.quantity == engine::MappedQuantity::Gain
                        || parameter.quantity == engine::MappedQuantity::Q)
                    {
                        row.released.push_back(parameter.index);
                    }
                }
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
    std::optional<juce::PluginDescription> master;
    std::vector<double> curve;
    const int masterSlot = getMasterSlot();
    if (masterSlot >= 0 && m_engine.getPlugin(masterSlot) != nullptr)
    {
        master = m_engine.getPlugin(masterSlot)->getDescription();
    }
    if (! master)
    {
        // the band curve: its exact magnitude on the probe's grid
        match::MagnitudeProbeSettings settings;
        settings.sampleRate = kSampleRate;
        settings.stimulus = match::ProbeStimulus::Impulse; // the grid of the live view (up to fs / 2)
        settings.upToNyquist = true;
        settings.perOctave = kLivePerOctave;
        const match::MagnitudeProbe grid(settings);
        eqmodel::Band band;
        band.type = static_cast<reference::FilterType>(std::array<int, 5>{6, 7, 8, 0, 1}[static_cast<size_t>(std::max(0, m_curveType.getSelectedItemIndex()))]);
        band.frequencyHz = m_curveFrequency.getValue();
        band.gainDb = m_curveGain.getValue();
        band.q = m_curveQ.getValue();
        curve = match::getCurveMagnitude(eqmodel::EqModel{{band}, 0.0}, grid.getFrequencies(), kSampleRate);
    }
    m_live = std::make_unique<LiveDeviationThread>(*this, m_formatManager, master, curve, target->getDescription());
    m_live->startThread();
    timerCallback();
}

void MatcherPanel::timerCallback()
{
    if (m_job != nullptr)
    {
        repaint(m_progressArea); // the elapsed time
    }
    // the knobs of the loaded plugins, read here on the message thread and handed to the live view
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
    std::vector<float> master;
    const int masterSlot = getMasterSlot();
    if (masterSlot >= 0 && m_engine.getPlugin(masterSlot) != nullptr)
    {
        master = engine::getSetting(m_engine.getPlugin(masterSlot)->getInstance());
    }
    m_live->setSettings(master, engine::getSetting(target->getInstance()));
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
    auto root = std::make_unique<juce::DynamicObject>();
    auto master = std::make_unique<juce::DynamicObject>();
    const int masterSlot = getMasterSlot();
    if (masterSlot >= 0 && m_engine.getPlugin(masterSlot) != nullptr)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(masterSlot);
        master->setProperty("plugin", plugin->getDescription().fileOrIdentifier);
        juce::Array<juce::var> values;
        for (const float value : engine::getSetting(plugin->getInstance()))
        {
            values.add(value);
        }
        master->setProperty("values", values);
    }
    else
    {
        auto band = std::make_unique<juce::DynamicObject>();
        band->setProperty("family", "RBJ cookbook");
        band->setProperty("type", kCurveTypes[std::max(0, m_curveType.getSelectedItemIndex())]);
        band->setProperty("frequency", m_curveFrequency.getValue());
        band->setProperty("gain", m_curveGain.getValue());
        band->setProperty("q", m_curveQ.getValue());
        master->setProperty("curve", juce::Array<juce::var>{juce::var(band.release())});
    }
    root->setProperty("master", juce::var(master.release()));
    juce::Array<juce::var> targets;
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
        auto target = std::make_unique<juce::DynamicObject>();
        target->setProperty("plugin", plugin->getDescription().fileOrIdentifier);
        juce::Array<juce::var> values;
        for (const float value : engine::getSetting(plugin->getInstance()))
        {
            values.add(value);
        }
        target->setProperty("values", values);
        juce::Array<juce::var> released;
        for (const int index : row.released)
        {
            released.add(index);
        }
        target->setProperty("releaseIndices", released);
        target->setProperty("modelStart", m_modelStart.getToggleState());
        targets.add(juce::var(target.release()));
    }
    root->setProperty("targets", targets);
    root->setProperty("tolerance", kToleranceNames[std::max(0, m_toleranceBox.getSelectedItemIndex())].upToFirstOccurrenceOf(" ", false, false));
    root->setProperty("seconds", m_seconds.getValue());
    root->setProperty("lowestHz", m_lowest.getValue());
    root->setProperty("highestHz", m_highest.getValue());
    if (targets.isEmpty())
    {
        return problem.isNotEmpty() ? problem : juce::String("no target chosen");
    }
    jobFile.getParentDirectory().createDirectory();
    jobFile.replaceWithText(juce::JSON::toString(juce::var(root.release())));
    return {};
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
    m_job = std::make_unique<MatchJobThread>(*this, job, m_lastReport);
    m_job->startThread();
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
    const juce::var result = juce::JSON::parse(juce::File(m_lastReport.getFullPathName() + ".json").loadFileAsString());
    const juce::Array<juce::var>* targets = result.getProperty("targets", {}).getArray();
    if (targets == nullptr)
    {
        return;
    }
    int applied = 0;
    for (const juce::var& entry : *targets)
    {
        if (static_cast<bool>(entry.getProperty("refused", false)))
        {
            continue;
        }
        const juce::String key = entry.getProperty("plugin", "").toString();
        const juce::Array<juce::var>* setting = entry.getProperty("setting", {}).getArray();
        const juce::Array<juce::var>* released = entry.getProperty("released", {}).getArray();
        for (const TargetRow& row : m_targets)
        {
            hosting::HostedPlugin* plugin = m_engine.getPlugin(row.slot);
            if (plugin == nullptr || row.key != key || setting == nullptr || released == nullptr)
            {
                continue;
            }
            for (const juce::var& index : *released)
            {
                const int parameter = static_cast<int>(index);
                if (parameter >= 0 && parameter < setting->size())
                {
                    plugin->setParameterNormalised(parameter, static_cast<float>(static_cast<double>((*setting)[parameter])));
                }
            }
            ++applied;
        }
    }
    m_status.setText("Applied to " + juce::String(applied) + " target(s). \"Listen\" compares them on the page Compare.", juce::dontSendNotification);
}

void MatcherPanel::savePresets()
{
    int saved = 0;
    const juce::String master = getMasterSlot() >= 0 && m_engine.getPlugin(getMasterSlot()) != nullptr
                                    ? hosting::getDisplayName(m_engine.getPlugin(getMasterSlot())->getDescription())
                                    : juce::String("band curve");
    const juce::String stamp = juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M");
    for (const TargetRow& row : m_targets)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(row.slot);
        if (plugin == nullptr || ! row.match)
        {
            continue;
        }
        juce::MemoryBlock state;
        plugin->getInstance().getStateInformation(state);
        const juce::String name = engine::makeFileNamePart(hosting::getDisplayName(plugin->getDescription()));
        const juce::File folder = m_settings.getPresetFolder().getChildFile(name);
        folder.createDirectory();
        const juce::String stem = "matched to " + engine::makeFileNamePart(master) + " " + stamp;
        if (folder.getChildFile(stem + ".state").replaceWithData(state.getData(), state.getSize()))
        {
            // what the state is, for people and for a later "load preset"
            auto info = std::make_unique<juce::DynamicObject>();
            info->setProperty("plugin", plugin->getDescription().createIdentifierString());
            info->setProperty("master", master);
            info->setProperty("date", stamp);
            juce::Array<juce::var> values;
            for (const float value : engine::getSetting(plugin->getInstance()))
            {
                values.add(value);
            }
            info->setProperty("values", values);
            folder.getChildFile(stem + ".json").replaceWithText(juce::JSON::toString(juce::var(info.release())));
            ++saved;
        }
    }
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
