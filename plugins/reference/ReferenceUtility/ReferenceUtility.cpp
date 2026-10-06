#include "ReferenceUtility.h"

#include <cmath>

#include "PluginProcessor.h"
#include "pluginlab/reference/Delays.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr int kPolarityInverted = 1;
constexpr int kInterpolationLagrange = 1;
constexpr int kInterpolationOrder = 3;
constexpr float kCrosstalkOffDb = -120.0f;
constexpr double kWholeSample = 1.0e-6;          // a delay this close to whole samples needs no interpolation
constexpr double kShortestThiranOrder3 = 2.5;    // Thiran of order N needs a delay of at least N - 0.5 samples
constexpr double kShortestOrder1 = 0.5;
constexpr int kCaptionHeight = 18;

// The delay line for a delay in samples: integer, or Thiran / Lagrange of order 3 (order 1 for very short delays); nullptr for no delay
std::unique_ptr<ref::DirectFormFilter> makeDelay(double samples, int interpolation, double sampleRate)
{
    if (samples < kWholeSample)
    {
        return nullptr;
    }
    if (std::abs(samples - std::round(samples)) < kWholeSample)
    {
        return std::make_unique<ref::DirectFormFilter>(ref::makeIntegerDelay(static_cast<int>(std::lround(samples)), sampleRate));
    }
    if (interpolation == kInterpolationLagrange || samples < kShortestOrder1)
    {
        int order = kInterpolationOrder;
        if (samples < kShortestOrder1)
        {
            order = 1;
        }
        return std::make_unique<ref::DirectFormFilter>(ref::makeLagrangeDelay(samples, order, sampleRate));
    }
    int order = kInterpolationOrder;
    if (samples < kShortestThiranOrder3)
    {
        order = 1;
    }
    return std::make_unique<ref::DirectFormFilter>(ref::makeThiranDelay(samples, order, sampleRate));
}

// Width first, then crosstalk: the product of the two 2 x 2 matrices
ref::ChannelMatrix makeMatrix(double width, double crosstalkDb)
{
    const ref::ChannelMatrix widthMatrix = ref::ChannelMatrix::makeWidth(width);
    double crosstalk = 0.0;
    if (crosstalkDb > kCrosstalkOffDb)
    {
        crosstalk = std::pow(10.0, crosstalkDb / 20.0);
    }
    const double ll = widthMatrix.getLeftFromLeft() + crosstalk * widthMatrix.getRightFromLeft();
    const double lr = widthMatrix.getLeftFromRight() + crosstalk * widthMatrix.getRightFromRight();
    const double rl = crosstalk * widthMatrix.getLeftFromLeft() + widthMatrix.getRightFromLeft();
    const double rr = crosstalk * widthMatrix.getLeftFromRight() + widthMatrix.getRightFromRight();
    return ref::ChannelMatrix(ll, lr, rl, rr);
}
}

bool ReferenceUtilityAudio::Setting::operator==(const Setting& other) const
{
    return juce::exactlyEqual(gain, other.gain) && polarity == other.polarity && juce::exactlyEqual(delay, other.delay) && interpolation == other.interpolation
           && juce::exactlyEqual(width, other.width) && juce::exactlyEqual(crosstalk, other.crosstalk) && juce::exactlyEqual(dc, other.dc)
           && juce::exactlyEqual(rate, other.rate) && juce::exactlyEqual(depth, other.depth);
}

ReferenceUtilityAudio::ReferenceUtilityAudio(ReferenceUtilityAudioProcessor* processor)
    : SynchronBlockProcessor()
    , m_processor(processor)
{
}

void ReferenceUtilityAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock);
    // g_desired_blocksize_ms is 0: no rebuffering, latency 0
    const int synchronblocksize = static_cast<int>(std::round(g_desired_blocksize_ms * sampleRate * 0.001));
    prepareSynchronProcessing(max_channels, synchronblocksize);
    m_Latency = synchronblocksize;
    m_sampleRate = sampleRate;
    m_channels = max_channels;
    m_settingValid = false;
    applySetting(readSetting());
}

ReferenceUtilityAudio::Setting ReferenceUtilityAudio::readSetting() const
{
    Setting setting;
    setting.gain = m_gainParam->load();
    setting.polarity = pluginlab::refplugins::getChoiceIndex(m_polarityParam);
    setting.delay = m_delayParam->load();
    setting.interpolation = pluginlab::refplugins::getChoiceIndex(m_interpolationParam);
    setting.width = m_widthParam->load();
    setting.crosstalk = m_crosstalkParam->load();
    setting.dc = m_dcParam->load();
    setting.rate = m_rateParam->load();
    setting.depth = m_depthParam->load();
    return setting;
}

void ReferenceUtilityAudio::applySetting(const Setting& setting)
{
    if (m_settingValid && setting == m_setting)
    {
        return;
    }
    const bool first = !m_settingValid;
    m_gain = std::make_unique<ref::Gain>(setting.gain, setting.polarity == kPolarityInverted);
    if (first || !juce::exactlyEqual(setting.delay, m_setting.delay) || setting.interpolation != m_setting.interpolation)
    {
        // a new delay starts empty
        m_delay = makeDelay(setting.delay, setting.interpolation, m_sampleRate);
        if (m_delay != nullptr)
        {
            m_delay->setNumChannels(m_channels);
        }
    }
    m_matrix = std::make_unique<ref::ChannelMatrix>(makeMatrix(setting.width, setting.crosstalk));
    if (first || !juce::exactlyEqual(setting.rate, m_setting.rate) || !juce::exactlyEqual(setting.depth, m_setting.depth))
    {
        // a new tremolo starts at gain 1
        m_tremolo = std::make_unique<ref::Tremolo>(m_sampleRate, setting.rate, setting.depth);
    }
    m_dc = std::make_unique<ref::DcOffset>(setting.dc);
    m_setting = setting;
    m_settingValid = true;
}

int ReferenceUtilityAudio::processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);
    applySetting(readSetting());
    m_gain->process(buffer);
    if (m_delay != nullptr)
    {
        m_delay->process(buffer);
    }
    m_matrix->process(buffer);
    m_tremolo->process(buffer);
    m_dc->process(buffer);
    return 0;
}

void ReferenceUtilityAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector)
{
    paramVector.push_back(jade::makeParameter(g_paramGain));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramPolarity));
    paramVector.push_back(jade::makeParameter(g_paramDelay));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramInterpolation));
    paramVector.push_back(jade::makeParameter(g_paramWidth));
    paramVector.push_back(jade::makeParameter(g_paramCrosstalk));
    paramVector.push_back(jade::makeParameter(g_paramDc));
    paramVector.push_back(jade::makeParameter(g_paramRate));
    paramVector.push_back(jade::makeParameter(g_paramDepth));
}

void ReferenceUtilityAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts)
{
    m_gainParam = vts->getRawParameterValue(g_paramGain.ID);
    m_polarityParam = vts->getRawParameterValue(g_paramPolarity.ID);
    m_delayParam = vts->getRawParameterValue(g_paramDelay.ID);
    m_interpolationParam = vts->getRawParameterValue(g_paramInterpolation.ID);
    m_widthParam = vts->getRawParameterValue(g_paramWidth.ID);
    m_crosstalkParam = vts->getRawParameterValue(g_paramCrosstalk.ID);
    m_dcParam = vts->getRawParameterValue(g_paramDc.ID);
    m_rateParam = vts->getRawParameterValue(g_paramRate.ID);
    m_depthParam = vts->getRawParameterValue(g_paramDepth.ID);
}

ReferenceUtilityGUI::ReferenceUtilityGUI(ReferenceUtilityAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
    : m_processor(p)
    , m_apvts(apvts)
    , m_controls(apvts)
{
    m_controls.addKnob(g_paramGain);
    m_controls.addChoice(g_paramPolarity);
    m_controls.addKnob(g_paramDelay);
    m_controls.addChoice(g_paramInterpolation);
    m_controls.addKnob(g_paramWidth);
    m_controls.addKnob(g_paramCrosstalk);
    m_controls.addKnob(g_paramDc);
    m_controls.addKnob(g_paramRate);
    m_controls.addKnob(g_paramDepth);
    addAndMakeVisible(m_controls);
}

void ReferenceUtilityGUI::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f));
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
    g.drawFittedText(pluginlab::refplugins::getCaption("PluginLab Reference Utility"), getLocalBounds().removeFromBottom(kCaptionHeight),
                     juce::Justification::bottomLeft, 1);
}

void ReferenceUtilityGUI::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom(kCaptionHeight);
    m_controls.setBounds(r);
}
