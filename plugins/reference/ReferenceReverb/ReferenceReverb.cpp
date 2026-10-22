#include "ReferenceReverb.h"

#include <cmath>

#include "PluginProcessor.h"

namespace
{
namespace ref = pluginlab::reference;

constexpr double kTailDecayDb = 120.0;
constexpr double kPercent = 0.01;
constexpr int kCaptionHeight = 18;
constexpr int kTimerHz = 10;
constexpr float kLayoutMargin = 10.0f;
}

bool ReferenceReverbAudio::Setting::operator==(const Setting& other) const
{
    return juce::exactlyEqual(t60, other.t60) && juce::exactlyEqual(preDelay, other.preDelay) && juce::exactlyEqual(modRate, other.modRate)
           && juce::exactlyEqual(modDepth, other.modDepth);
}

ReferenceReverbAudio::ReferenceReverbAudio(ReferenceReverbAudioProcessor* processor)
    : SynchronBlockProcessor()
    , m_processor(processor)
{
}

void ReferenceReverbAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    const int synchronblocksize = static_cast<int>(std::round(g_desired_blocksize_ms * sampleRate * 0.001));
    prepareSynchronProcessing(max_channels, synchronblocksize);
    m_Latency = synchronblocksize;
    m_sampleRate = sampleRate;
    m_channels = max_channels;
    m_wet.setSize(max_channels, std::max(1, max_samplesPerBlock));
    m_settingValid = false;
}

ReferenceReverbAudio::Setting ReferenceReverbAudio::readSetting() const
{
    Setting setting;
    setting.t60 = m_t60Param->load();
    setting.preDelay = m_preDelayParam->load();
    setting.modRate = m_modRateParam->load();
    setting.modDepth = m_modDepthParam->load();
    return setting;
}

double ReferenceReverbAudio::getTailSeconds() const
{
    return m_tailSeconds.load();
}

int ReferenceReverbAudio::processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);
    const Setting setting = readSetting();
    if (! m_settingValid || ! (setting == m_setting))
    {
        // a new setting: a new network (it starts empty)
        ref::FdnReverbSettings settings;
        settings.sampleRate = m_sampleRate;
        settings.t60Seconds = setting.t60;
        settings.preDelaySeconds = setting.preDelay * 0.001;
        settings.lfoRateHz = setting.modRate;
        settings.lfoDepthSamples = setting.modDepth;
        m_fdn = std::make_unique<ref::FdnReverb>(settings);
        m_setting = setting;
        m_settingValid = true;
        m_tailSeconds.store(settings.preDelaySeconds + kTailDecayDb / 60.0 * settings.t60Seconds);
    }
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    if (m_wet.getNumChannels() < channels || m_wet.getNumSamples() < samples)
    {
        m_wet.setSize(channels, samples, false, false, true);
    }
    juce::AudioBuffer<float> wet(m_wet.getArrayOfWritePointers(), channels, samples);
    for (int channel = 0; channel < channels; ++channel)
    {
        wet.copyFrom(channel, 0, buffer, channel, 0, samples);
    }
    m_fdn->process(wet);
    const float mix = static_cast<float>(m_mixParam->load() * kPercent);
    for (int channel = 0; channel < channels; ++channel)
    {
        buffer.applyGain(channel, 0, samples, 1.0f - mix);
        buffer.addFrom(channel, 0, wet, channel, 0, samples, mix);
    }
    return 0;
}

void ReferenceReverbAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector)
{
    paramVector.push_back(jade::makeParameter(g_paramT60));
    paramVector.push_back(jade::makeParameter(g_paramPreDelay));
    paramVector.push_back(jade::makeParameter(g_paramModRate));
    paramVector.push_back(jade::makeParameter(g_paramModDepth));
    paramVector.push_back(jade::makeParameter(g_paramMix));
}

void ReferenceReverbAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts)
{
    m_t60Param = vts->getRawParameterValue(g_paramT60.ID);
    m_preDelayParam = vts->getRawParameterValue(g_paramPreDelay.ID);
    m_modRateParam = vts->getRawParameterValue(g_paramModRate.ID);
    m_modDepthParam = vts->getRawParameterValue(g_paramModDepth.ID);
    m_mixParam = vts->getRawParameterValue(g_paramMix.ID);
}

ReferenceReverbGUI::ReferenceReverbGUI(ReferenceReverbAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
    : m_processor(p)
    , m_apvts(apvts)
    , m_controls(apvts)
{
    m_controls.addKnob(g_paramPreDelay);
    m_controls.addKnob(g_paramT60);
    m_controls.addKnob(g_paramModRate);
    m_controls.addKnob(g_paramModDepth);
    m_controls.addKnob(g_paramMix);
    addAndMakeVisible(m_controls);
    m_flow = {{"Pre-delay", {g_paramPreDelay.ID}, 0, {}},
              {"Feedback delay network", {g_paramT60.ID, g_paramModRate.ID, g_paramModDepth.ID}, 0, {}},
              {"Mix", {g_paramMix.ID}, 0, {}}};
    startTimerHz(kTimerHz);
}

void ReferenceReverbGUI::timerCallback()
{
    m_controls.setControlEnabled(g_paramModRate.ID, m_apvts.getRawParameterValue(g_paramModDepth.ID)->load() > 0.0f);
}

void ReferenceReverbGUI::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f);
    const juce::Colour text = getLookAndFeel().findColour(juce::Label::textColourId);
    g.fillAll(background);
    pluginlab::refplugins::drawFlow(g, m_flow, m_scale, background, text);
    g.setColour(text);
    g.drawFittedText(pluginlab::refplugins::getCaption("PluginLab Reference Reverb"), getLocalBounds().removeFromBottom(kCaptionHeight),
                     juce::Justification::bottomLeft, 1);
}

void ReferenceReverbGUI::resized()
{
    m_scale = static_cast<float>(getWidth()) / static_cast<float>(PLUGINLAB_GUI_WIDTH);
    m_controls.setBounds(getLocalBounds());
    const float margin = kLayoutMargin * m_scale;
    pluginlab::refplugins::layoutFlow(m_controls, m_flow, getLocalBounds().toFloat().reduced(margin).withTrimmedBottom(static_cast<float>(kCaptionHeight)), m_scale);
}
