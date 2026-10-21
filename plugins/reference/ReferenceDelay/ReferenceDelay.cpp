#include "ReferenceDelay.h"

#include <cmath>

#include "PluginProcessor.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr int kSyncOff = 0;
constexpr int kLoopFilterOn = 1;
constexpr int kInterpolationAllPass = 1;
constexpr int kLoopOrderAuthors = 1;
constexpr double kTempoWithoutHost = 120.0;
constexpr double kSecondsPerMinute = 60.0;
constexpr double kShortestDelaySamples = 3.0; // the interpolators need their neighbours
constexpr double kTailDecayDb = 120.0;
constexpr double kLongestTailSeconds = 30.0;
constexpr double kPercent = 0.01;
constexpr int kCaptionHeight = 18;
constexpr int kTimerHz = 10;
constexpr float kLayoutMargin = 10.0f;

// the note values of the Sync choice in quarter notes (index 0: off)
constexpr double kQuarters[] = {0.0, 1.0, 0.5, 0.75, 1.0 / 3.0, 0.25};
}

ReferenceDelayAudio::ReferenceDelayAudio(ReferenceDelayAudioProcessor* processor)
    : SynchronBlockProcessor()
    , m_processor(processor)
{
}

void ReferenceDelayAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock, max_channels);
    // g_desired_blocksize_ms is 0: no rebuffering, latency 0
    const int synchronblocksize = static_cast<int>(std::round(g_desired_blocksize_ms * sampleRate * 0.001));
    prepareSynchronProcessing(max_channels, synchronblocksize);
    m_Latency = synchronblocksize;
    m_sampleRate = sampleRate;
    // the longest delay: the time knob's maximum, or a quarter note at the slowest tempo the line allows (the delay is limited to it)
    const double longest = static_cast<double>(g_paramTime.maxValue) * 0.001 * sampleRate;
    m_delay = std::make_unique<ref::FeedbackDelay>(sampleRate, readSetting(), longest);
    juce::AudioBuffer<float> empty(max_channels, 0);
    m_delay->process(empty); // the line of every channel now, not in the first processBlock
}

double ReferenceDelayAudio::getTempo() const
{
    if (juce::AudioPlayHead* playHead = m_processor->getPlayHead())
    {
        if (const juce::Optional<juce::AudioPlayHead::PositionInfo> position = playHead->getPosition())
        {
            if (const juce::Optional<double> bpm = position->getBpm())
            {
                if (*bpm > 0.0)
                {
                    return *bpm;
                }
            }
        }
    }
    return kTempoWithoutHost;
}

ref::FeedbackDelaySettings ReferenceDelayAudio::readSetting() const
{
    ref::FeedbackDelaySettings settings;
    const int sync = pluginlab::refplugins::getChoiceIndex(m_syncParam);
    double seconds = m_timeParam->load() * 0.001;
    if (sync != kSyncOff)
    {
        seconds = kQuarters[sync] * kSecondsPerMinute / getTempo();
    }
    settings.delaySamples = std::max(kShortestDelaySamples, seconds * m_sampleRate);
    settings.feedback = m_feedbackParam->load() * kPercent;
    settings.mix = m_mixParam->load() * kPercent;
    if (pluginlab::refplugins::getChoiceIndex(m_loopFilterParam) == kLoopFilterOn)
    {
        settings.lowCutHz = m_lowCutParam->load();
        settings.highCutHz = std::min(static_cast<double>(m_highCutParam->load()), 0.45 * m_sampleRate);
    }
    settings.interpolation = ref::DelayInterpolation::Lagrange;
    if (pluginlab::refplugins::getChoiceIndex(m_interpolationParam) == kInterpolationAllPass)
    {
        settings.interpolation = ref::DelayInterpolation::AllPass;
    }
    settings.order = ref::LoopOrder::Exact;
    if (pluginlab::refplugins::getChoiceIndex(m_loopOrderParam) == kLoopOrderAuthors)
    {
        settings.order = ref::LoopOrder::AuthorsOrder;
    }
    return settings;
}

double ReferenceDelayAudio::getTailSeconds() const
{
    return m_tailSeconds.load();
}

int ReferenceDelayAudio::processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);
    const ref::FeedbackDelaySettings settings = readSetting();
    m_delay->setSettings(settings);
    // the tail: one delay per repeat until the echoes are 120 dB down
    double repeats = 1.0;
    const double feedback = std::abs(settings.feedback);
    if (feedback > 0.0)
    {
        repeats += kTailDecayDb / (-20.0 * std::log10(feedback));
    }
    m_tailSeconds.store(std::min(kLongestTailSeconds, repeats * m_delay->getSettings().delaySamples / m_sampleRate));
    m_delay->process(buffer);
    return 0;
}

void ReferenceDelayAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector)
{
    paramVector.push_back(jade::makeParameter(g_paramTime));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramSync));
    paramVector.push_back(jade::makeParameter(g_paramFeedback));
    paramVector.push_back(jade::makeParameter(g_paramMix));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramLoopFilter));
    paramVector.push_back(jade::makeParameter(g_paramLowCut));
    paramVector.push_back(jade::makeParameter(g_paramHighCut));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramInterpolation));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramLoopOrder));
}

void ReferenceDelayAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts)
{
    m_timeParam = vts->getRawParameterValue(g_paramTime.ID);
    m_syncParam = vts->getRawParameterValue(g_paramSync.ID);
    m_feedbackParam = vts->getRawParameterValue(g_paramFeedback.ID);
    m_mixParam = vts->getRawParameterValue(g_paramMix.ID);
    m_loopFilterParam = vts->getRawParameterValue(g_paramLoopFilter.ID);
    m_lowCutParam = vts->getRawParameterValue(g_paramLowCut.ID);
    m_highCutParam = vts->getRawParameterValue(g_paramHighCut.ID);
    m_interpolationParam = vts->getRawParameterValue(g_paramInterpolation.ID);
    m_loopOrderParam = vts->getRawParameterValue(g_paramLoopOrder.ID);
}

ReferenceDelayGUI::ReferenceDelayGUI(ReferenceDelayAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
    : m_processor(p)
    , m_apvts(apvts)
    , m_controls(apvts)
{
    m_controls.addKnob(g_paramTime);
    m_controls.addChoice(g_paramSync);
    m_controls.addChoice(g_paramInterpolation);
    m_controls.addKnob(g_paramFeedback);
    m_controls.addChoice(g_paramLoopFilter);
    m_controls.addKnob(g_paramLowCut);
    m_controls.addKnob(g_paramHighCut);
    m_controls.addChoice(g_paramLoopOrder);
    m_controls.addKnob(g_paramMix);
    addAndMakeVisible(m_controls);
    m_flow = {{"Delay line", {g_paramTime.ID, g_paramSync.ID, g_paramInterpolation.ID}, 0, {}},
              {"Loop", {g_paramFeedback.ID, g_paramLoopFilter.ID, g_paramLowCut.ID, g_paramHighCut.ID, g_paramLoopOrder.ID}, 0, {}},
              {"Mix", {g_paramMix.ID}, 0, {}}};
    updateControls();
    startTimerHz(kTimerHz);
}

void ReferenceDelayGUI::updateControls()
{
    m_controls.setControlEnabled(g_paramTime.ID, pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramSync.ID)) == kSyncOff);
    const bool filters = pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramLoopFilter.ID)) == kLoopFilterOn;
    m_controls.setControlEnabled(g_paramLowCut.ID, filters);
    m_controls.setControlEnabled(g_paramHighCut.ID, filters);
}

void ReferenceDelayGUI::timerCallback()
{
    updateControls();
}

void ReferenceDelayGUI::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f);
    const juce::Colour text = getLookAndFeel().findColour(juce::Label::textColourId);
    g.fillAll(background);
    pluginlab::refplugins::drawFlow(g, m_flow, m_scale, background, text);
    g.setColour(text);
    g.drawFittedText(pluginlab::refplugins::getCaption("PluginLab Reference Delay"), getLocalBounds().removeFromBottom(kCaptionHeight),
                     juce::Justification::bottomLeft, 1);
}

void ReferenceDelayGUI::resized()
{
    m_scale = static_cast<float>(getWidth()) / static_cast<float>(PLUGINLAB_GUI_WIDTH);
    m_controls.setBounds(getLocalBounds());
    const float margin = kLayoutMargin * m_scale;
    pluginlab::refplugins::layoutFlow(m_controls, m_flow, getLocalBounds().toFloat().reduced(margin).withTrimmedBottom(static_cast<float>(kCaptionHeight)), m_scale);
}
