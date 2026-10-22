#include "ReferenceCompressor.h"

#include <cmath>

#include "PluginProcessor.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr double kMilliseconds = 0.001;
constexpr double kPercent = 0.01;
constexpr int kCaptionHeight = 18;
constexpr int kTimerHz = 10;
constexpr float kLayoutMargin = 10.0f;
}

ReferenceCompressorAudio::ReferenceCompressorAudio(ReferenceCompressorAudioProcessor* processor)
    : SynchronBlockProcessor()
    , m_processor(processor)
{
}

void ReferenceCompressorAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock);
    const int synchronblocksize = static_cast<int>(std::round(g_desired_blocksize_ms * sampleRate * 0.001));
    prepareSynchronProcessing(max_channels, synchronblocksize);
    m_sampleRate = sampleRate;
    m_settingValid = false;
}

ReferenceCompressorAudio::Setting ReferenceCompressorAudio::readSetting() const
{
    Setting setting{};
    for (size_t index = 0; index < setting.size(); ++index)
    {
        setting[index] = m_params[index]->load();
    }
    return setting;
}

int ReferenceCompressorAudio::getLatency() const
{
    if (m_params[Lookahead] == nullptr)
    {
        return 0;
    }
    return static_cast<int>(std::lround(m_params[Lookahead]->load() * kMilliseconds * m_sampleRate));
}

double ReferenceCompressorAudio::getTailSeconds() const
{
    if (m_params[Lookahead] == nullptr)
    {
        return 0.0;
    }
    return m_params[Lookahead]->load() * kMilliseconds;
}

int ReferenceCompressorAudio::processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);
    const Setting setting = readSetting();
    if (! m_settingValid || setting != m_setting)
    {
        // a new setting: a new compressor (no reduction at its start)
        ref::CompressorSettings settings;
        settings.sampleRate = m_sampleRate;
        settings.thresholdDb = setting[Threshold];
        settings.ratio = setting[Ratio];
        settings.kneeDb = setting[Knee];
        settings.makeupDb = setting[Makeup];
        settings.attackSeconds = setting[Attack] * kMilliseconds;
        settings.releaseSeconds = setting[Release] * kMilliseconds;
        settings.holdSeconds = setting[Hold] * kMilliseconds;
        settings.rmsSeconds = setting[RmsWindow] * kMilliseconds;
        settings.lookaheadSeconds = setting[Lookahead] * kMilliseconds;
        settings.stereoLink = setting[Link] * kPercent;
        settings.sidechainHighPassHz = setting[SidechainHighPass];
        settings.detector = ref::Detector::Peak;
        if (static_cast<int>(std::lround(setting[DetectorChoice])) == 1)
        {
            settings.detector = ref::Detector::Rms;
        }
        settings.smoothing = ref::Smoothing::GainLog;
        const int smoothing = static_cast<int>(std::lround(setting[SmoothingChoice]));
        if (smoothing == 1)
        {
            settings.smoothing = ref::Smoothing::LevelLog;
        }
        else if (smoothing == 2)
        {
            settings.smoothing = ref::Smoothing::TwoStage;
        }
        else if (smoothing == 3)
        {
            settings.smoothing = ref::Smoothing::LevelLinear;
        }
        m_compressor = std::make_unique<ref::Compressor>(settings);
        m_setting = setting;
        m_settingValid = true;
    }
    m_compressor->process(buffer);
    return 0;
}

void ReferenceCompressorAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector)
{
    paramVector.push_back(jade::makeParameter(g_paramThreshold));
    paramVector.push_back(jade::makeParameter(g_paramRatio));
    paramVector.push_back(jade::makeParameter(g_paramKnee));
    paramVector.push_back(jade::makeParameter(g_paramMakeup));
    paramVector.push_back(jade::makeParameter(g_paramAttack));
    paramVector.push_back(jade::makeParameter(g_paramRelease));
    paramVector.push_back(jade::makeParameter(g_paramHold));
    paramVector.push_back(jade::makeParameter(g_paramRmsWindow));
    paramVector.push_back(jade::makeParameter(g_paramLookahead));
    paramVector.push_back(jade::makeParameter(g_paramLink));
    paramVector.push_back(jade::makeParameter(g_paramSidechainHighPass));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramDetector));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramSmoothing));
}

void ReferenceCompressorAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts)
{
    m_params[Threshold] = vts->getRawParameterValue(g_paramThreshold.ID);
    m_params[Ratio] = vts->getRawParameterValue(g_paramRatio.ID);
    m_params[Knee] = vts->getRawParameterValue(g_paramKnee.ID);
    m_params[Makeup] = vts->getRawParameterValue(g_paramMakeup.ID);
    m_params[Attack] = vts->getRawParameterValue(g_paramAttack.ID);
    m_params[Release] = vts->getRawParameterValue(g_paramRelease.ID);
    m_params[Hold] = vts->getRawParameterValue(g_paramHold.ID);
    m_params[RmsWindow] = vts->getRawParameterValue(g_paramRmsWindow.ID);
    m_params[Lookahead] = vts->getRawParameterValue(g_paramLookahead.ID);
    m_params[Link] = vts->getRawParameterValue(g_paramLink.ID);
    m_params[SidechainHighPass] = vts->getRawParameterValue(g_paramSidechainHighPass.ID);
    m_params[DetectorChoice] = vts->getRawParameterValue(g_paramDetector.ID);
    m_params[SmoothingChoice] = vts->getRawParameterValue(g_paramSmoothing.ID);
}

ReferenceCompressorGUI::ReferenceCompressorGUI(ReferenceCompressorAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
    : m_processor(p)
    , m_apvts(apvts)
    , m_controls(apvts)
{
    m_controls.addKnob(g_paramSidechainHighPass);
    m_controls.addChoice(g_paramDetector);
    m_controls.addKnob(g_paramRmsWindow);
    m_controls.addKnob(g_paramLink);
    m_controls.addKnob(g_paramThreshold);
    m_controls.addKnob(g_paramRatio);
    m_controls.addKnob(g_paramKnee);
    m_controls.addChoice(g_paramSmoothing);
    m_controls.addKnob(g_paramAttack);
    m_controls.addKnob(g_paramRelease);
    m_controls.addKnob(g_paramHold);
    m_controls.addKnob(g_paramLookahead);
    m_controls.addKnob(g_paramMakeup);
    addAndMakeVisible(m_controls);
    m_flow = {{"Detector", {g_paramSidechainHighPass.ID, g_paramDetector.ID, g_paramRmsWindow.ID, g_paramLink.ID}, 0, {}},
              {"Gain computer", {g_paramThreshold.ID, g_paramRatio.ID, g_paramKnee.ID}, 0, {}},
              {"Smoothing", {g_paramSmoothing.ID, g_paramAttack.ID, g_paramRelease.ID, g_paramHold.ID}, 0, {}},
              {"Output", {g_paramLookahead.ID, g_paramMakeup.ID}, 0, {}}};
    startTimerHz(kTimerHz);
}

void ReferenceCompressorGUI::timerCallback()
{
    m_controls.setControlEnabled(g_paramRmsWindow.ID, pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramDetector.ID)) == 1);
}

void ReferenceCompressorGUI::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f);
    const juce::Colour text = getLookAndFeel().findColour(juce::Label::textColourId);
    g.fillAll(background);
    pluginlab::refplugins::drawFlow(g, m_flow, m_scale, background, text);
    g.setColour(text);
    g.drawFittedText(pluginlab::refplugins::getCaption("PluginLab Reference Compressor"), getLocalBounds().removeFromBottom(kCaptionHeight),
                     juce::Justification::bottomLeft, 1);
}

void ReferenceCompressorGUI::resized()
{
    m_scale = static_cast<float>(getWidth()) / static_cast<float>(PLUGINLAB_GUI_WIDTH);
    m_controls.setBounds(getLocalBounds());
    const float margin = kLayoutMargin * m_scale;
    pluginlab::refplugins::layoutFlow(m_controls, m_flow, getLocalBounds().toFloat().reduced(margin).withTrimmedBottom(static_cast<float>(kCaptionHeight)), m_scale);
}
