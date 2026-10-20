#include "LiveResponseView.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "pluginlab/engine/PluginDevice.h"
#include "pluginlab/hosting/FormatManager.h"
#include "pluginlab/hosting/HostedPlugin.h"
#include "pluginlab/ui/ResponsePlotImage.h"

namespace pluginlab::host
{
namespace
{
constexpr int kTimerMs = 50;              // the knobs are read 20 times per second; the monitor renders at most 5 curves per second per slot
constexpr int kMargin = 8;
constexpr int kRowHeight = 26;
constexpr int kLabelWidth = 80;
constexpr int kToggleWidth = 130;
constexpr int kReferenceWidth = 220;
constexpr int kInfoHeight = 76;           // a line per slot
constexpr double kRangeStepDb = 6.0;
constexpr double kHeadroomDb = 0.5;      // a curve does not touch the edge
constexpr double kSmallestRangeDb = 12.0;
constexpr double kLargestRangeDb = 48.0;
constexpr double kShownFloorDb = -60.0;   // a stop band does not stretch the axis
constexpr double kSmallestDifferenceDb = 1.0;
constexpr double kFineDifferenceDb = 3.0;
constexpr float kReferenceThickness = 3.0f;
constexpr float kCurveThickness = 1.6f;
const juce::Colour kNotLti = juce::Colour(0xff808080);

juce::String formatDb(double value)
{
    return juce::String(value, 2) + " dB";
}
}

LiveResponseView::LiveResponseView(engine::MeasurementEngine& engine)
    : m_engine(engine)
{
    m_enabled.setToggleState(true, juce::dontSendNotification);
    m_enabled.setTooltip("The magnitude response of every slot while you turn its knobs (measuring copies of the plugins, 5 updates per second "
                         "per changed slot; the audio is not touched). Valid for LTI plugins only: the system class is checked when the knobs rest.");
    m_enabled.onClick = [this] { setLive(m_enabled.getToggleState()); };
    addAndMakeVisible(m_enabled);
    m_referenceLabel.setText("Reference", juce::dontSendNotification);
    m_referenceLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_referenceLabel);
    m_reference.setTooltip("The slot the others are compared with (drawn thicker; the deviations below are from it)");
    m_reference.onChange = [this] { rebuildPlot(); };
    addAndMakeVisible(m_reference);
    m_info.setJustificationType(juce::Justification::topLeft);
    m_info.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(m_info);
    startTimer(kTimerMs);
}

LiveResponseView::~LiveResponseView()
{
    stopTimer();
    m_monitor.reset();
}

void LiveResponseView::setLive(bool enabled)
{
    if (! enabled)
    {
        m_monitor.reset(); // the measuring instances go
        rebuildPlot();
        return;
    }
    slotsChanged();
}

void LiveResponseView::slotsChanged()
{
    // the reference choice: the first plugin by default (the dry slot is flat)
    const int previous = m_reference.getSelectedItemIndex();
    m_reference.clear(juce::dontSendNotification);
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        m_reference.addItem(juce::String(slot + 1) + ": " + m_engine.getSlotInfo(slot).name, slot + 1);
    }
    const int fallback = m_engine.getNumSlots() > 1 ? 1 : 0;
    m_reference.setSelectedItemIndex(previous >= 0 && previous < m_engine.getNumSlots() ? previous : fallback, juce::dontSendNotification);

    if (! m_enabled.getToggleState())
    {
        rebuildPlot();
        return;
    }
    if (m_monitor == nullptr || ! juce::approximatelyEqual(m_monitor->getSampleRate(), m_engine.getSampleRate()))
    {
        m_monitor = std::make_unique<RemoteLiveResponse>(m_engine.getSampleRate());
    }
    std::vector<std::optional<juce::PluginDescription>> slots;
    for (int slot = 0; slot < m_engine.getNumSlots(); ++slot)
    {
        hosting::HostedPlugin* plugin = m_engine.getPlugin(slot);
        slots.push_back(plugin != nullptr ? std::optional<juce::PluginDescription>(plugin->getDescription()) : std::nullopt);
    }
    m_monitor->setSlots(slots);
    m_generations.assign(slots.size(), -1);
    m_classifications.assign(slots.size(), -1);
    timerCallback();
    rebuildPlot();
}

void LiveResponseView::timerCallback()
{
    if (m_monitor == nullptr || ! isVisibleWithParents())
    {
        return; // another page is shown: nothing is measured
    }
    // the knobs of the playing instances, read here on the message thread; the monitor renders what changed
    bool changed = false;
    const int slots = std::min(m_monitor->getNumSlots(), m_engine.getNumSlots());
    for (int slot = 0; slot < slots; ++slot)
    {
        if (hosting::HostedPlugin* plugin = m_engine.getPlugin(slot))
        {
            m_monitor->setSetting(slot, engine::getSetting(plugin->getInstance()));
        }
        const engine::LiveSlotResponse response = m_monitor->getResponse(slot);
        const auto index = static_cast<size_t>(slot);
        if (index < m_generations.size() && (response.generation != m_generations[index] || response.classifications != m_classifications[index]))
        {
            m_generations[index] = response.generation;
            m_classifications[index] = response.classifications;
            changed = true;
        }
    }
    if (changed)
    {
        rebuildPlot();
    }
}

bool LiveResponseView::isVisibleWithParents() const
{
    // the tabs hide the pages that are not shown (isShowing() also asks for a window on the screen, which the offscreen tests do not have)
    for (const juce::Component* component = this; component != nullptr; component = component->getParentComponent())
    {
        if (! component->isVisible())
        {
            return false;
        }
    }
    return true;
}

int LiveResponseView::getReferenceSlot() const
{
    return std::max(0, m_reference.getSelectedItemIndex());
}

void LiveResponseView::rebuildPlot()
{
    if (m_plotArea.isEmpty())
    {
        return;
    }
    std::vector<ui::PlotCurve> curves;
    std::vector<ui::PlotCurve> differences;
    juce::StringArray lines;
    double largest = kSmallestRangeDb - kRangeStepDb;
    double largestDifference = 0.0;
    std::vector<double> frequencies;
    if (m_monitor != nullptr)
    {
        frequencies = m_monitor->getFrequencies();
        const int reference = getReferenceSlot();
        const engine::LiveSlotResponse master = m_monitor->getResponse(reference);
        const int slots = std::min(m_monitor->getNumSlots(), m_engine.getNumSlots());
        int colour = 1;
        for (int slot = 0; slot < slots; ++slot)
        {
            const engine::LiveSlotResponse response = m_monitor->getResponse(slot);
            const juce::String name = juce::String(slot + 1) + ": " + m_engine.getSlotInfo(slot).name;
            if (response.error.isNotEmpty())
            {
                lines.add(name + ": no curve (" + response.error + ")");
                continue;
            }
            if (response.generation == 0)
            {
                lines.add(name + ": turn a knob or wait for the first curve");
                continue;
            }
            for (const double value : response.magnitudeDb)
            {
                if (value > kShownFloorDb)
                {
                    largest = std::max(largest, std::abs(value));
                }
            }
            ui::PlotCurve curve;
            curve.valuesDb = response.magnitudeDb;
            curve.colour = slot == reference ? ui::getPlotColour(0) : ui::getPlotColour(colour++);
            curve.thickness = slot == reference ? kReferenceThickness : kCurveThickness;
            curve.name = name;
            juce::StringArray notes;
            if (response.classified && ! response.lti)
            {
                curve.colour = kNotLti;
                curve.name += " (not LTI)";
                notes.add("not LTI (" + response.systemClass + ": " + response.reason + "), the curve is not its transfer function");
            }
            else if (! response.classified || ! response.classOfCurrentSetting)
            {
                notes.add("system class being checked");
            }
            if (slot == reference)
            {
                notes.add("reference");
            }
            else if (master.generation > 0)
            {
                const engine::LiveDeviation deviation = engine::getLiveDeviation(master.magnitudeDb, response.magnitudeDb);
                ui::PlotCurve difference = curve;
                difference.thickness = kCurveThickness;
                for (size_t index = 0; index < difference.valuesDb.size() && index < master.magnitudeDb.size(); ++index)
                {
                    difference.valuesDb[index] -= master.magnitudeDb[index];
                    if (response.magnitudeDb[index] > kShownFloorDb || master.magnitudeDb[index] > kShownFloorDb)
                    {
                        largestDifference = std::max(largestDifference, std::abs(difference.valuesDb[index]));
                    }
                }
                difference.name = juce::String(slot + 1) + ": largest " + formatDb(deviation.largestDb) + ", rms " + formatDb(deviation.rmsDb);
                differences.push_back(difference);
                notes.add("deviation from the reference: largest " + formatDb(deviation.largestDb) + ", rms " + formatDb(deviation.rmsDb));
            }
            curves.push_back(curve);
            lines.add(notes.isEmpty() ? name : name + ": " + notes.joinIntoString("; "));
        }
    }
    ui::ResponsePlotOptions options;
    options.width = std::max(1, m_plotArea.getWidth());
    options.height = std::max(1, m_plotArea.getHeight());
    options.rangeDb = juce::jlimit(kSmallestRangeDb, kLargestRangeDb, std::ceil((largest + kHeadroomDb) / kRangeStepDb) * kRangeStepDb);
    const double differenceStep = largestDifference > kFineDifferenceDb ? kRangeStepDb / 2.0 : 1.0; // 1 dB steps for small deviations, else 3
    options.differenceRangeDb = std::max(kSmallestDifferenceDb, std::ceil((largestDifference + kHeadroomDb) / differenceStep) * differenceStep);
    options.differenceTitle = "slot - reference (dB)";
    options.title = m_monitor != nullptr ? "EQ magnitude responses up to Nyquist (impulse, " + juce::String(juce::roundToInt(m_monitor->getSampleRate())) + " Hz, at most 5 updates/s per changed slot)"
                                         : "Live response switched off";
    if (frequencies.empty())
    {
        frequencies = {20.0, 20000.0};
    }
    m_plot = ui::renderResponsePlot(frequencies, curves, differences, options);
    m_description = lines.joinIntoString("\n");
    m_info.setText(m_description, juce::dontSendNotification);
    repaint();
}

juce::String LiveResponseView::describe() const
{
    return m_description;
}

void LiveResponseView::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    if (m_plot.isValid())
    {
        g.drawImageAt(m_plot, m_plotArea.getX(), m_plotArea.getY());
    }
}

void LiveResponseView::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    juce::Rectangle<int> row = area.removeFromTop(kRowHeight);
    m_enabled.setBounds(row.removeFromLeft(kToggleWidth));
    row.removeFromLeft(kMargin);
    m_referenceLabel.setBounds(row.removeFromLeft(kLabelWidth));
    m_reference.setBounds(row.removeFromLeft(std::min(kReferenceWidth, row.getWidth())));
    area.removeFromTop(kMargin / 2);
    m_info.setBounds(area.removeFromBottom(kInfoHeight));
    m_plotArea = area;
    rebuildPlot();
}
}
