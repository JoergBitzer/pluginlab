#include "ReferenceEq.h"

#include <cmath>

#include "PluginProcessor.h"
#include "pluginlab/reference/Designs.h"
#include "pluginlab/eqmodel/EqModel.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr double kPlotSampleRate = 48000.0;       // the GUI shows the response at 48 kHz
constexpr double kPlotLowestHz = 20.0;
constexpr double kPlotHighestHz = 0.5 * kPlotSampleRate; // exactly to Nyquist: there the designs differ most
constexpr double kPlotRangeDb = 30.0;             // the plot shows +-30 dB
constexpr int kPlotPoints = 600;
constexpr int kCaptionHeight = 18;
constexpr int kTimerHz = 20;
// the layout in design units (the editor's smallest width, PLUGINLAB_GUI_WIDTH); scaled with the editor
constexpr float kDesignWidth = static_cast<float>(PLUGINLAB_GUI_WIDTH);
constexpr float kMargin = 8.0f;
constexpr float kRowHeight = 132.0f;             // the boxes with the controls
constexpr float kTitleHeight = 18.0f;            // of a box
constexpr float kDesignBoxWidth = 210.0f;
constexpr float kCellGap = 4.0f;
constexpr float kAxisLabelHeight = 16.0f;        // the frequency labels below the plot
constexpr float kLevelLabelWidth = 44.0f;        // the dB labels left of the plot
constexpr float kNyquistLabelWidth = 80.0f;      // "24k (Nyquist)" at the right end of the frequency axis

const ref::FilterType kTypes[] = {ref::FilterType::LowPass,  ref::FilterType::HighPass, ref::FilterType::BandPass,
                                  ref::FilterType::BandPassUnity, ref::FilterType::Notch, ref::FilterType::AllPass,
                                  ref::FilterType::Peak,     ref::FilterType::LowShelf, ref::FilterType::HighShelf};
const EqAlgorithm kAlgorithms[] = {EqAlgorithm::Rbj, EqAlgorithm::Orfanidis, EqAlgorithm::Zoelzer, EqAlgorithm::StateVariable, EqAlgorithm::Butterworth,
                                   EqAlgorithm::LinkwitzRiley, EqAlgorithm::LinearPhase};

}

EqSetting makeEqSetting(int algorithmIndex, int typeIndex, double frequencyHz, double gainDb, double q, double order)
{
    EqSetting setting;
    setting.family = kAlgorithms[juce::jlimit(0, static_cast<int>(std::size(kAlgorithms)) - 1, algorithmIndex)];
    setting.type = kTypes[juce::jlimit(0, static_cast<int>(std::size(kTypes)) - 1, typeIndex)];
    setting.frequencyHz = frequencyHz;
    setting.gainDb = gainDb;
    setting.q = q;
    setting.order = juce::roundToInt(order);
    return setting;
}

ReferenceEqAudio::ReferenceEqAudio(ReferenceEqAudioProcessor* processor)
    : SynchronBlockProcessor()
    , m_processor(processor)
{
}

void ReferenceEqAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock);
    // g_desired_blocksize_ms is 0: no rebuffering, latency 0
    const int synchronblocksize = static_cast<int>(std::round(g_desired_blocksize_ms * sampleRate * 0.001));
    prepareSynchronProcessing(max_channels, synchronblocksize);
    m_Latency = synchronblocksize;
    m_sampleRate = sampleRate;
    m_cascade = std::make_unique<ref::BiquadCascade>(std::vector<ref::BiquadCoefficients>(), sampleRate);
    m_cascade->setNumChannels(max_channels);
    m_stateVariable = std::make_unique<ref::StateVariableFilter>(ref::FilterType::Peak, sampleRate, 1000.0, 0.0, 1.0);
    m_stateVariable->setNumChannels(max_channels);
    m_channels = max_channels;
    m_fir.reset();
    m_settingValid = false;
    applySetting(readSetting());
}

EqSetting ReferenceEqAudio::readSetting() const
{
    return makeEqSetting(pluginlab::refplugins::getChoiceIndex(m_algorithm), pluginlab::refplugins::getChoiceIndex(m_type), m_frequency->load(),
                         m_gain->load(), m_q->load(), m_order->load());
}

void ReferenceEqAudio::applySetting(const EqSetting& setting)
{
    if (m_settingValid && setting == m_setting)
    {
        return;
    }
    const bool algorithmChanged = !m_settingValid || setting.family != m_setting.family;
    m_setting = setting;
    m_settingValid = true;
    m_design = pluginlab::eqmodel::designBand(setting, m_sampleRate);
    // new coefficients keep the state (no smoothing: the reference changes at once); a new algorithm starts from silence
    if (algorithmChanged)
    {
        m_cascade->reset();
        m_stateVariable->reset();
    }
    // the linear-phase family: an FIR (new taps, new filter) and its latency, reported to the host
    if (m_design.latencySamples != m_Latency)
    {
        m_Latency = m_design.latencySamples;
        m_processor->setLatencySamples(m_Latency);
    }
    if (! m_design.firTaps.empty())
    {
        m_fir = std::make_unique<ref::DirectFormFilter>(m_design.firTaps, std::vector<double>{1.0}, m_sampleRate, 0, m_design.latencySamples);
        m_fir->setNumChannels(m_channels);
    }
    if (m_design.stateVariable)
    {
        m_stateVariable->setParameters(setting.type, m_design.frequencyHz, setting.gainDb, setting.q);
    }
    else if (m_design.firTaps.empty())
    {
        if (m_design.sections.size() != m_cascade->getSections().size())
        {
            m_cascade->setSections(m_design.sections);
            m_cascade->reset();
        }
        else
        {
            m_cascade->setSections(m_design.sections);
        }
    }
}

int ReferenceEqAudio::processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);
    applySetting(readSetting());
    if (!m_design.supported)
    {
        return 0;
    }
    if (m_design.stateVariable)
    {
        m_stateVariable->process(buffer);
    }
    else if (m_fir != nullptr && ! m_design.firTaps.empty())
    {
        m_fir->process(buffer);
    }
    else
    {
        m_cascade->process(buffer);
    }
    return 0;
}

void ReferenceEqAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector)
{
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramAlgorithm));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramType));
    paramVector.push_back(jade::makeParameter(g_paramFrequency));
    paramVector.push_back(jade::makeParameter(g_paramGain));
    paramVector.push_back(jade::makeParameter(g_paramQ));
    paramVector.push_back(jade::makeParameter(g_paramOrder));
}

void ReferenceEqAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts)
{
    m_algorithm = vts->getRawParameterValue(g_paramAlgorithm.ID);
    m_type = vts->getRawParameterValue(g_paramType.ID);
    m_frequency = vts->getRawParameterValue(g_paramFrequency.ID);
    m_gain = vts->getRawParameterValue(g_paramGain.ID);
    m_q = vts->getRawParameterValue(g_paramQ.ID);
    m_order = vts->getRawParameterValue(g_paramOrder.ID);
}

ReferenceEqGUI::ReferenceEqGUI(ReferenceEqAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
    : m_processor(p)
    , m_apvts(apvts)
    , m_controls(apvts)
{
    m_controls.addChoice(g_paramAlgorithm);
    m_controls.addChoice(g_paramType);
    m_controls.addKnob(g_paramFrequency);
    m_controls.addKnob(g_paramGain);
    m_controls.addKnob(g_paramQ);
    m_controls.addKnob(g_paramOrder);
    addAndMakeVisible(m_controls);
    m_shownSetting = readSetting();
    updateTypeChoice(m_shownSetting);
    updateKnobs(m_shownSetting);
    startTimerHz(kTimerHz);
}

EqSetting ReferenceEqGUI::readSetting() const
{
    return makeEqSetting(pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramAlgorithm.ID)),
                         pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramType.ID)),
                         m_apvts.getRawParameterValue(g_paramFrequency.ID)->load(), m_apvts.getRawParameterValue(g_paramGain.ID)->load(),
                         m_apvts.getRawParameterValue(g_paramQ.ID)->load(), m_apvts.getRawParameterValue(g_paramOrder.ID)->load());
}

void ReferenceEqGUI::updateTypeChoice(const EqSetting& setting)
{
    juce::ComboBox* box = m_controls.getComboBox(g_paramType.ID);
    if (box == nullptr)
    {
        return;
    }
    if (!m_typesShown || setting.family != m_typesAlgorithm)
    {
        for (int index = 0; index < static_cast<int>(std::size(kTypes)); ++index)
        {
            box->setItemEnabled(index + 1, pluginlab::eqmodel::isTypeAvailable(setting.family, kTypes[index]));
        }
        m_typesShown = true;
        m_typesAlgorithm = setting.family;
    }
    if (pluginlab::eqmodel::isTypeAvailable(setting.family, setting.type))
    {
        return;
    }
    // the chosen type is not offered by the new algorithm: move it to the algorithm's default (as a user gesture, so that the host records it)
    const ref::FilterType wanted = pluginlab::eqmodel::getDefaultType(setting.family);
    for (int index = 0; index < static_cast<int>(std::size(kTypes)); ++index)
    {
        if (kTypes[index] == wanted)
        {
            juce::RangedAudioParameter* parameter = m_apvts.getParameter(g_paramType.ID);
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(index)));
            parameter->endChangeGesture();
        }
    }
}

void ReferenceEqGUI::updateKnobs(const EqSetting& setting)
{
    using Family = pluginlab::eqmodel::DesignFamily;
    const bool available = pluginlab::eqmodel::isTypeAvailable(setting.family, setting.type);
    const bool shelf = setting.type == ref::FilterType::LowShelf || setting.type == ref::FilterType::HighShelf;
    const bool passFamily = setting.family == Family::Butterworth || setting.family == Family::LinkwitzRiley;
    const bool zoelzerShelf = setting.family == Family::Zoelzer && shelf;
    m_controls.setControlEnabled(g_paramFrequency.ID, available);
    m_controls.setControlEnabled(g_paramGain.ID, available && (setting.type == ref::FilterType::Peak || shelf));
    m_controls.setControlEnabled(g_paramQ.ID, available && ! passFamily && ! zoelzerShelf);
    m_controls.setControlEnabled(g_paramOrder.ID, available && (passFamily || zoelzerShelf));
}

void ReferenceEqGUI::timerCallback()
{
    const EqSetting setting = readSetting();
    updateTypeChoice(setting);
    updateKnobs(setting);
    if (!(setting == m_shownSetting))
    {
        m_shownSetting = setting;
        repaint();
    }
}

void ReferenceEqGUI::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f);
    g.fillAll(background);
    const juce::Colour text = getLookAndFeel().findColour(juce::Label::textColourId);
    pluginlab::refplugins::drawGroup(g, m_designBox, "Design", kTitleHeight * m_scale, background, text);
    pluginlab::refplugins::drawGroup(g, m_bandBox, "Band", kTitleHeight * m_scale, background, text);

    // the exact magnitude response at 48 kHz, 20 Hz ... Nyquist (24 kHz), +-30 dB, logarithmic frequency axis
    const EqDesign design = pluginlab::eqmodel::designBand(m_shownSetting, kPlotSampleRate);
    const juce::Rectangle<float> area = m_plotArea.toFloat();
    const auto toX = [&area](double frequency) {
        return area.getX() + static_cast<float>(std::log(frequency / kPlotLowestHz) / std::log(kPlotHighestHz / kPlotLowestHz)) * area.getWidth();
    };
    const auto toY = [&area](double decibels) { return area.getCentreY() - static_cast<float>(decibels / kPlotRangeDb) * area.getHeight() / 2.0f; };
    g.setFont(juce::FontOptions(12.0f * std::max(m_scale, 0.8f)));
    g.setColour(text.withAlpha(0.3f));
    g.drawRect(area);
    for (const double decibels : {-24.0, -12.0, 0.0, 12.0, 24.0})
    {
        const float y = toY(decibels);
        g.setColour(text.withAlpha(juce::exactlyEqual(decibels, 0.0) ? 0.5f : 0.25f));
        g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
        g.setColour(text.withAlpha(0.8f));
        g.drawText(juce::String(static_cast<int>(decibels)) + " dB", juce::Rectangle<float>(area.getX() - kLevelLabelWidth * m_scale, y - 7.0f, kLevelLabelWidth * m_scale - 4.0f, 14.0f),
                   juce::Justification::centredRight);
    }
    // frequency ticks: 1-2-5 per decade (labelled) and the other whole multiples (lines only), and Nyquist at the right edge
    for (double decade = 10.0; decade < kPlotHighestHz; decade *= 10.0)
    {
        for (int multiple = 1; multiple <= 9; ++multiple)
        {
            const double frequency = decade * multiple;
            if (frequency < kPlotLowestHz || frequency > kPlotHighestHz)
            {
                continue;
            }
            const bool labelled = multiple == 1 || multiple == 2 || multiple == 5;
            const float x = toX(frequency);
            g.setColour(text.withAlpha(labelled ? 0.25f : 0.1f));
            g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
            if (labelled && area.getRight() - x > kNyquistLabelWidth * m_scale) // the last label before the edge gives way to Nyquist's
            {
                const juce::String label = frequency >= 1000.0 ? juce::String(juce::roundToInt(frequency / 1000.0)) + "k" : juce::String(juce::roundToInt(frequency));
                g.setColour(text.withAlpha(0.8f));
                g.drawText(label, juce::Rectangle<float>(x - 20.0f, area.getBottom() + 2.0f, 40.0f, kAxisLabelHeight * m_scale - 2.0f), juce::Justification::centredTop);
            }
        }
    }
    g.setColour(text.withAlpha(0.8f));
    g.drawText(juce::String(kPlotHighestHz / 1000.0, 0) + "k (Nyquist)",
               juce::Rectangle<float>(area.getRight() - kNyquistLabelWidth * m_scale, area.getBottom() + 2.0f, kNyquistLabelWidth * m_scale + kMargin * m_scale,
                                      kAxisLabelHeight * m_scale - 2.0f),
               juce::Justification::topRight);
    juce::Path curve;
    for (int point = 0; point < kPlotPoints; ++point)
    {
        const double position = static_cast<double>(point) / (kPlotPoints - 1);
        const double frequency = kPlotLowestHz * std::pow(kPlotHighestHz / kPlotLowestHz, position);
        const double magnitude = std::abs(pluginlab::eqmodel::getBandResponse(design, m_shownSetting, frequency, kPlotSampleRate));
        const double decibels = juce::jlimit(-kPlotRangeDb, kPlotRangeDb, 20.0 * std::log10(std::max(magnitude, 1.0e-10)));
        const float x = area.getX() + static_cast<float>(position) * area.getWidth();
        const float y = toY(decibels);
        if (point == 0)
        {
            curve.startNewSubPath(x, y);
        }
        else
        {
            curve.lineTo(x, y);
        }
    }
    g.saveState();
    g.reduceClipRegion(m_plotArea);
    g.setColour(getLookAndFeel().findColour(juce::Slider::thumbColourId));
    g.strokePath(curve, juce::PathStrokeType(2.0f));
    g.restoreState();

    g.setColour(text);
    g.drawText(design.description + "  (exact response at 48 kHz, 20 Hz ... Nyquist)", m_plotArea.reduced(6).removeFromTop(16), juce::Justification::right);
    g.drawFittedText(pluginlab::refplugins::getCaption("PluginLab Reference EQ"), getLocalBounds().removeFromBottom(kCaptionHeight), juce::Justification::bottomLeft, 1);
}

void ReferenceEqGUI::resized()
{
    // one row: the design box (algorithm over type) and the band box (frequency, gain, Q, order side by side); the plot below
    m_scale = static_cast<float>(getWidth()) / kDesignWidth;
    const float s = m_scale;
    m_controls.setBounds(getLocalBounds());
    const juce::Rectangle<float> row(kMargin * s, kMargin * s, static_cast<float>(getWidth()) - 2.0f * kMargin * s, kRowHeight * s);
    juce::Rectangle<float> rest = row;
    m_designBox = rest.removeFromLeft(kDesignBoxWidth * s);
    rest.removeFromLeft(kMargin * s);
    m_bandBox = rest;
    juce::Rectangle<float> design = m_designBox.reduced(kCellGap * s).withTrimmedTop(kTitleHeight * s);
    const float half = design.getHeight() / 2.0f;
    m_controls.placeCell(g_paramAlgorithm.ID, design.removeFromTop(half).toNearestInt());
    m_controls.placeCell(g_paramType.ID, design.toNearestInt());
    juce::Rectangle<float> band = m_bandBox.reduced(kCellGap * s).withTrimmedTop(kTitleHeight * s);
    const float cellWidth = band.getWidth() / 4.0f;
    for (const std::string* id : {&g_paramFrequency.ID, &g_paramGain.ID, &g_paramQ.ID, &g_paramOrder.ID})
    {
        m_controls.placeCell(*id, band.removeFromLeft(cellWidth).reduced(kCellGap * s, 0.0f).toNearestInt());
    }
    auto r = getLocalBounds();
    r.removeFromTop(juce::roundToInt((2.0f * kMargin + kRowHeight) * s));
    r.removeFromBottom(kCaptionHeight);
    r.removeFromBottom(juce::roundToInt(kAxisLabelHeight * s));
    r.removeFromLeft(juce::roundToInt(kLevelLabelWidth * s));
    r.removeFromRight(juce::roundToInt(kMargin * s));
    m_plotArea = r.reduced(0, juce::roundToInt(kMargin * s / 2.0f));
}
