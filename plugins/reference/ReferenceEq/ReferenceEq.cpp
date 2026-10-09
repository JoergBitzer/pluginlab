#include "ReferenceEq.h"

#include <cmath>

#include "PluginProcessor.h"
#include "pluginlab/reference/Designs.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr double kHighestFrequencyRatio = 0.45;   // the corner stays below 0.45 fs
constexpr double kPlotSampleRate = 48000.0;       // the GUI shows the response at 48 kHz
constexpr double kPlotLowestHz = 20.0;
constexpr double kPlotHighestHz = 20000.0;
constexpr double kPlotRangeDb = 30.0;             // the plot shows +-30 dB
constexpr int kPlotPoints = 300;
constexpr int kControlsHeight = 230;
constexpr int kCaptionHeight = 18;
constexpr int kTimerHz = 20;
constexpr int kZoelzerSecondOrder = 2;
constexpr int kLinkwitzRileyOrders[] = {2, 4, 8};

const ref::FilterType kTypes[] = {ref::FilterType::LowPass,  ref::FilterType::HighPass, ref::FilterType::BandPass,
                                  ref::FilterType::BandPassUnity, ref::FilterType::Notch, ref::FilterType::AllPass,
                                  ref::FilterType::Peak,     ref::FilterType::LowShelf, ref::FilterType::HighShelf};
const EqAlgorithm kAlgorithms[] = {EqAlgorithm::Rbj, EqAlgorithm::Orfanidis, EqAlgorithm::Zoelzer, EqAlgorithm::StateVariable, EqAlgorithm::Butterworth,
                                   EqAlgorithm::LinkwitzRiley};

bool isPass(ref::FilterType type)
{
    return type == ref::FilterType::LowPass || type == ref::FilterType::HighPass;
}

ref::Pass getPass(ref::FilterType type)
{
    if (type == ref::FilterType::HighPass)
    {
        return ref::Pass::High;
    }
    return ref::Pass::Low;
}

int getLinkwitzRileyOrder(int order)
{
    for (const int allowed : kLinkwitzRileyOrders)
    {
        if (order <= allowed)
        {
            return allowed;
        }
    }
    return kLinkwitzRileyOrders[2];
}

EqDesign makeUnsupported(const juce::String& reason)
{
    EqDesign design;
    design.supported = false;
    design.description = reason + " - the audio passes unchanged";
    return design;
}
}

bool EqSetting::operator==(const EqSetting& other) const
{
    return algorithm == other.algorithm && type == other.type && juce::exactlyEqual(frequencyHz, other.frequencyHz)
           && juce::exactlyEqual(gainDb, other.gainDb) && juce::exactlyEqual(q, other.q) && order == other.order;
}

EqSetting makeEqSetting(int algorithmIndex, int typeIndex, double frequencyHz, double gainDb, double q, double order)
{
    EqSetting setting;
    setting.algorithm = kAlgorithms[juce::jlimit(0, static_cast<int>(std::size(kAlgorithms)) - 1, algorithmIndex)];
    setting.type = kTypes[juce::jlimit(0, static_cast<int>(std::size(kTypes)) - 1, typeIndex)];
    setting.frequencyHz = frequencyHz;
    setting.gainDb = gainDb;
    setting.q = q;
    setting.order = juce::roundToInt(order);
    return setting;
}

bool isTypeAvailable(EqAlgorithm algorithm, ref::FilterType type)
{
    switch (algorithm)
    {
        case EqAlgorithm::Rbj:
        case EqAlgorithm::StateVariable:
            return true;
        case EqAlgorithm::Orfanidis:
            return type == ref::FilterType::Peak;
        case EqAlgorithm::Zoelzer:
            return type == ref::FilterType::Peak || type == ref::FilterType::LowShelf || type == ref::FilterType::HighShelf;
        case EqAlgorithm::Butterworth:
        case EqAlgorithm::LinkwitzRiley:
            return isPass(type);
    }
    return false;
}

ref::FilterType getDefaultType(EqAlgorithm algorithm)
{
    if (algorithm == EqAlgorithm::Butterworth || algorithm == EqAlgorithm::LinkwitzRiley)
    {
        return ref::FilterType::LowPass;
    }
    return ref::FilterType::Peak;
}

EqDesign designEq(const EqSetting& setting, double sampleRate)
{
    const double frequency = std::min(setting.frequencyHz, kHighestFrequencyRatio * sampleRate);
    const juce::String typeName(ref::getFilterTypeName(setting.type));
    EqDesign design;
    design.frequencyHz = frequency;
    switch (setting.algorithm)
    {
        case EqAlgorithm::Rbj:
            design.sections.push_back(ref::designRbj(setting.type, sampleRate, frequency, setting.gainDb, setting.q));
            design.description = "RBJ cookbook " + typeName;
            return design;
        case EqAlgorithm::Orfanidis:
            if (!isTypeAvailable(setting.algorithm, setting.type))
            {
                return makeUnsupported("Orfanidis offers the peak filter only");
            }
            design.sections.push_back(ref::designOrfanidisPeak(sampleRate, frequency, setting.gainDb, setting.q));
            design.description = "Orfanidis peak (prescribed Nyquist gain)";
            return design;
        case EqAlgorithm::Zoelzer:
        {
            ref::ZoelzerType zoelzer = ref::ZoelzerType::Peak;
            if (setting.type == ref::FilterType::LowShelf)
            {
                zoelzer = ref::ZoelzerType::LowShelfFirstOrder;
                if (setting.order >= kZoelzerSecondOrder)
                {
                    zoelzer = ref::ZoelzerType::LowShelf;
                }
            }
            else if (setting.type == ref::FilterType::HighShelf)
            {
                zoelzer = ref::ZoelzerType::HighShelfFirstOrder;
                if (setting.order >= kZoelzerSecondOrder)
                {
                    zoelzer = ref::ZoelzerType::HighShelf;
                }
            }
            else if (setting.type != ref::FilterType::Peak)
            {
                return makeUnsupported("Zoelzer offers shelves and the peak only");
            }
            design.sections.push_back(ref::designZoelzer(zoelzer, sampleRate, frequency, setting.gainDb, setting.q));
            design.description = juce::String("Zoelzer ") + ref::getZoelzerTypeName(zoelzer);
            return design;
        }
        case EqAlgorithm::StateVariable:
            design.stateVariable = true;
            design.description = "state-variable filter (TPT) " + typeName;
            return design;
        case EqAlgorithm::Butterworth:
        {
            if (!isPass(setting.type))
            {
                return makeUnsupported("Butterworth offers low-pass and high-pass only");
            }
            const int order = juce::jlimit(1, 8, setting.order);
            design.sections = ref::designButterworth(getPass(setting.type), order, sampleRate, frequency);
            design.description = "Butterworth " + typeName + ", order " + juce::String(order);
            return design;
        }
        case EqAlgorithm::LinkwitzRiley:
        {
            if (!isPass(setting.type))
            {
                return makeUnsupported("Linkwitz-Riley offers low-pass and high-pass only");
            }
            const int order = getLinkwitzRileyOrder(setting.order);
            design.sections = ref::designLinkwitzRiley(getPass(setting.type), order, sampleRate, frequency);
            design.description = "Linkwitz-Riley " + typeName + ", order " + juce::String(order);
            return design;
        }
    }
    return makeUnsupported("unknown algorithm");
}

std::complex<double> getEqResponse(const EqDesign& design, const EqSetting& setting, double frequencyHz, double sampleRate)
{
    if (!design.supported)
    {
        return 1.0;
    }
    if (design.stateVariable)
    {
        // the state-variable filter realises the RBJ transfer function of the same type (tested in ReferenceTests)
        return ref::getBiquadResponse(ref::designRbj(setting.type, sampleRate, design.frequencyHz, setting.gainDb, setting.q), frequencyHz, sampleRate);
    }
    return ref::getCascadeResponse(design.sections, frequencyHz, sampleRate);
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
    const bool algorithmChanged = !m_settingValid || setting.algorithm != m_setting.algorithm;
    m_setting = setting;
    m_settingValid = true;
    m_design = designEq(setting, m_sampleRate);
    // new coefficients keep the state (no smoothing: the reference changes at once); a new algorithm starts from silence
    if (algorithmChanged)
    {
        m_cascade->reset();
        m_stateVariable->reset();
    }
    if (m_design.stateVariable)
    {
        m_stateVariable->setParameters(setting.type, m_design.frequencyHz, setting.gainDb, setting.q);
    }
    else
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
    if (!m_typesShown || setting.algorithm != m_typesAlgorithm)
    {
        for (int index = 0; index < static_cast<int>(std::size(kTypes)); ++index)
        {
            box->setItemEnabled(index + 1, isTypeAvailable(setting.algorithm, kTypes[index]));
        }
        m_typesShown = true;
        m_typesAlgorithm = setting.algorithm;
    }
    if (isTypeAvailable(setting.algorithm, setting.type))
    {
        return;
    }
    // the chosen type is not offered by the new algorithm: move it to the algorithm's default (as a user gesture, so that the host records it)
    const ref::FilterType wanted = getDefaultType(setting.algorithm);
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

void ReferenceEqGUI::timerCallback()
{
    const EqSetting setting = readSetting();
    updateTypeChoice(setting);
    if (!(setting == m_shownSetting))
    {
        m_shownSetting = setting;
        repaint();
    }
}

void ReferenceEqGUI::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f));
    const juce::Colour text = getLookAndFeel().findColour(juce::Label::textColourId);

    // the exact magnitude response at 48 kHz, 20 Hz ... 20 kHz, +-30 dB
    const EqDesign design = designEq(m_shownSetting, kPlotSampleRate);
    const juce::Rectangle<float> area = m_plotArea.toFloat();
    g.setColour(text.withAlpha(0.3f));
    g.drawRect(area);
    for (const double decibels : {-24.0, -12.0, 0.0, 12.0, 24.0})
    {
        const float y = area.getCentreY() - static_cast<float>(decibels / kPlotRangeDb) * area.getHeight() / 2.0f;
        g.drawHorizontalLine(juce::roundToInt(y), area.getX(), area.getRight());
        g.drawText(juce::String(static_cast<int>(decibels)) + " dB", juce::Rectangle<float>(area.getX() + 2.0f, y - 14.0f, 60.0f, 14.0f),
                   juce::Justification::left);
    }
    for (const double frequency : {100.0, 1000.0, 10000.0})
    {
        const float x = area.getX() + static_cast<float>(std::log(frequency / kPlotLowestHz) / std::log(kPlotHighestHz / kPlotLowestHz)) * area.getWidth();
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
    }
    juce::Path curve;
    for (int point = 0; point < kPlotPoints; ++point)
    {
        const double position = static_cast<double>(point) / (kPlotPoints - 1);
        const double frequency = kPlotLowestHz * std::pow(kPlotHighestHz / kPlotLowestHz, position);
        const double magnitude = std::abs(getEqResponse(design, m_shownSetting, frequency, kPlotSampleRate));
        const double decibels = juce::jlimit(-kPlotRangeDb, kPlotRangeDb, 20.0 * std::log10(std::max(magnitude, 1.0e-10)));
        const float x = area.getX() + static_cast<float>(position) * area.getWidth();
        const float y = area.getCentreY() - static_cast<float>(decibels / kPlotRangeDb) * area.getHeight() / 2.0f;
        if (point == 0)
        {
            curve.startNewSubPath(x, y);
        }
        else
        {
            curve.lineTo(x, y);
        }
    }
    g.setColour(getLookAndFeel().findColour(juce::Slider::thumbColourId));
    g.strokePath(curve, juce::PathStrokeType(2.0f));

    g.setColour(text);
    g.drawText(design.description + "  (exact response at 48 kHz)", m_plotArea.reduced(4).removeFromTop(16), juce::Justification::right);
    g.drawFittedText(pluginlab::refplugins::getCaption("PluginLab Reference EQ"), getLocalBounds().removeFromBottom(kCaptionHeight), juce::Justification::bottomLeft, 1);
}

void ReferenceEqGUI::resized()
{
    auto r = getLocalBounds();
    m_controls.setBounds(r.removeFromTop(kControlsHeight));
    r.removeFromBottom(kCaptionHeight);
    m_plotArea = r.reduced(8);
}
