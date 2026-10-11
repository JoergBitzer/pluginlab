#include "ReferenceNonlinear.h"

#include <cmath>

#include "PluginProcessor.h"

namespace
{
namespace ref = pluginlab::reference;
namespace sig = pluginlab::signals;

constexpr int kShaperOff = 0;
constexpr int kShaperPolynomial = 1;
constexpr int kShaperHardClip = 2;
constexpr int kQuantizerOff = 0;
constexpr int kQuantizerDither = 2;
constexpr int kNoiseOff = 0;
constexpr int kNoiseWhite = 1;
constexpr int kHumOff = 0;
constexpr int kHum50 = 1;
constexpr double kMains50Hz = 50.0;
constexpr double kMains60Hz = 60.0;
constexpr int kNoiseSeed = 7;
constexpr int kDitherSeed = 1;
constexpr int kCaptionHeight = 18;
constexpr int kTimerHz = 10;          // how often the GUI greys out what has no effect
constexpr float kLayoutMargin = 10.0f; // around the signal flow, in design units

double toGain(double decibels)
{
    return std::pow(10.0, decibels / 20.0);
}

void applyGain(juce::AudioBuffer<float>& buffer, double gain)
{
    buffer.applyGain(static_cast<float>(gain));
}
}

bool ReferenceNonlinearAudio::Setting::operator==(const Setting& other) const
{
    return shaper == other.shaper && juce::exactlyEqual(drive, other.drive) && juce::exactlyEqual(a2, other.a2) && juce::exactlyEqual(a3, other.a3)
           && juce::exactlyEqual(threshold, other.threshold) && juce::exactlyEqual(output, other.output) && quantizer == other.quantizer
           && bits == other.bits && noise == other.noise && juce::exactlyEqual(noiseLevel, other.noiseLevel) && hum == other.hum
           && juce::exactlyEqual(humLevel, other.humLevel);
}

ReferenceNonlinearAudio::ReferenceNonlinearAudio(ReferenceNonlinearAudioProcessor* processor)
    : SynchronBlockProcessor()
    , m_processor(processor)
{
}

void ReferenceNonlinearAudio::prepareToPlay(double sampleRate, int max_samplesPerBlock, int max_channels)
{
    juce::ignoreUnused(max_samplesPerBlock);
    // g_desired_blocksize_ms is 0: no rebuffering, latency 0
    const int synchronblocksize = static_cast<int>(std::round(g_desired_blocksize_ms * sampleRate * 0.001));
    prepareSynchronProcessing(max_channels, synchronblocksize);
    m_Latency = synchronblocksize;
    m_sampleRate = sampleRate;
    m_settingValid = false;
    applySetting(readSetting());
}

ReferenceNonlinearAudio::Setting ReferenceNonlinearAudio::readSetting() const
{
    Setting setting;
    setting.shaper = pluginlab::refplugins::getChoiceIndex(m_shaperParam);
    setting.drive = m_driveParam->load();
    setting.a2 = m_a2Param->load();
    setting.a3 = m_a3Param->load();
    setting.threshold = m_thresholdParam->load();
    setting.output = m_outputParam->load();
    setting.quantizer = pluginlab::refplugins::getChoiceIndex(m_quantizerParam);
    setting.bits = juce::roundToInt(m_bitsParam->load());
    setting.noise = pluginlab::refplugins::getChoiceIndex(m_noiseParam);
    setting.noiseLevel = m_noiseLevelParam->load();
    setting.hum = pluginlab::refplugins::getChoiceIndex(m_humParam);
    setting.humLevel = m_humLevelParam->load();
    return setting;
}

void ReferenceNonlinearAudio::applySetting(const Setting& setting)
{
    if (m_settingValid && setting == m_setting)
    {
        return;
    }
    // the stages whose settings changed are made anew (noise, hum and dither start again from their seed / phase 0)
    const bool first = !m_settingValid;
    if (first || setting.shaper != m_setting.shaper || !juce::exactlyEqual(setting.a2, m_setting.a2) || !juce::exactlyEqual(setting.a3, m_setting.a3)
        || !juce::exactlyEqual(setting.threshold, m_setting.threshold) || !juce::exactlyEqual(setting.drive, m_setting.drive))
    {
        m_shaper.reset();
        if (setting.shaper == kShaperPolynomial)
        {
            m_shaper = std::make_unique<ref::Waveshaper>(ref::Waveshaper::makePolynomial({0.0, 1.0, setting.a2, setting.a3}));
        }
        else if (setting.shaper == kShaperHardClip)
        {
            m_shaper = std::make_unique<ref::Waveshaper>(ref::Waveshaper::makeHardClip(toGain(setting.threshold)));
        }
        else if (setting.shaper != kShaperOff)
        {
            m_shaper = std::make_unique<ref::Waveshaper>(ref::Waveshaper::makeSoftClip(1.0));
        }
    }
    if (first || setting.quantizer != m_setting.quantizer || setting.bits != m_setting.bits)
    {
        m_quantizer.reset();
        if (setting.quantizer != kQuantizerOff)
        {
            m_quantizer = std::make_unique<ref::Quantizer>(setting.bits, setting.quantizer == kQuantizerDither, kDitherSeed);
        }
    }
    if (first || setting.noise != m_setting.noise || !juce::exactlyEqual(setting.noiseLevel, m_setting.noiseLevel))
    {
        m_noise.reset();
        if (setting.noise != kNoiseOff)
        {
            sig::NoiseColour colour = sig::NoiseColour::Pink;
            if (setting.noise == kNoiseWhite)
            {
                colour = sig::NoiseColour::WhiteGaussian;
            }
            m_noise = std::make_unique<ref::NoiseAdder>(colour, setting.noiseLevel, kNoiseSeed);
        }
    }
    if (first || setting.hum != m_setting.hum || !juce::exactlyEqual(setting.humLevel, m_setting.humLevel))
    {
        m_hum.reset();
        if (setting.hum != kHumOff)
        {
            double mains = kMains60Hz;
            if (setting.hum == kHum50)
            {
                mains = kMains50Hz;
            }
            m_hum = std::make_unique<ref::HumAdder>(m_sampleRate, mains, setting.humLevel);
        }
    }
    m_driveGain = toGain(setting.drive);
    m_outputGain = toGain(setting.output);
    m_setting = setting;
    m_settingValid = true;
}

int ReferenceNonlinearAudio::processSynchronBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int NrOfBlocksSinceLastProcessBlock)
{
    juce::ignoreUnused(midiMessages, NrOfBlocksSinceLastProcessBlock);
    applySetting(readSetting());
    applyGain(buffer, m_driveGain);
    if (m_shaper != nullptr)
    {
        m_shaper->process(buffer);
    }
    applyGain(buffer, m_outputGain);
    if (m_hum != nullptr)
    {
        m_hum->process(buffer);
    }
    if (m_noise != nullptr)
    {
        m_noise->process(buffer);
    }
    if (m_quantizer != nullptr)
    {
        m_quantizer->process(buffer);
    }
    return 0;
}

void ReferenceNonlinearAudio::addParameter(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& paramVector)
{
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramShaper));
    paramVector.push_back(jade::makeParameter(g_paramDrive));
    paramVector.push_back(jade::makeParameter(g_paramA2));
    paramVector.push_back(jade::makeParameter(g_paramA3));
    paramVector.push_back(jade::makeParameter(g_paramThreshold));
    paramVector.push_back(jade::makeParameter(g_paramOutput));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramQuantizer));
    paramVector.push_back(jade::makeParameter(g_paramBits));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramNoise));
    paramVector.push_back(jade::makeParameter(g_paramNoiseLevel));
    paramVector.push_back(pluginlab::refplugins::makeChoiceParameter(g_paramHum));
    paramVector.push_back(jade::makeParameter(g_paramHumLevel));
}

void ReferenceNonlinearAudio::prepareParameter(std::unique_ptr<juce::AudioProcessorValueTreeState>& vts)
{
    m_shaperParam = vts->getRawParameterValue(g_paramShaper.ID);
    m_driveParam = vts->getRawParameterValue(g_paramDrive.ID);
    m_a2Param = vts->getRawParameterValue(g_paramA2.ID);
    m_a3Param = vts->getRawParameterValue(g_paramA3.ID);
    m_thresholdParam = vts->getRawParameterValue(g_paramThreshold.ID);
    m_outputParam = vts->getRawParameterValue(g_paramOutput.ID);
    m_quantizerParam = vts->getRawParameterValue(g_paramQuantizer.ID);
    m_bitsParam = vts->getRawParameterValue(g_paramBits.ID);
    m_noiseParam = vts->getRawParameterValue(g_paramNoise.ID);
    m_noiseLevelParam = vts->getRawParameterValue(g_paramNoiseLevel.ID);
    m_humParam = vts->getRawParameterValue(g_paramHum.ID);
    m_humLevelParam = vts->getRawParameterValue(g_paramHumLevel.ID);
}

ReferenceNonlinearGUI::ReferenceNonlinearGUI(ReferenceNonlinearAudioProcessor& p, juce::AudioProcessorValueTreeState& apvts)
    : m_processor(p)
    , m_apvts(apvts)
    , m_controls(apvts)
{
    m_controls.addChoice(g_paramShaper);
    m_controls.addKnob(g_paramDrive);
    m_controls.addKnob(g_paramA2);
    m_controls.addKnob(g_paramA3);
    m_controls.addKnob(g_paramThreshold);
    m_controls.addKnob(g_paramOutput);
    m_controls.addChoice(g_paramQuantizer);
    m_controls.addKnob(g_paramBits);
    m_controls.addChoice(g_paramNoise);
    m_controls.addKnob(g_paramNoiseLevel);
    m_controls.addChoice(g_paramHum);
    m_controls.addKnob(g_paramHumLevel);
    addAndMakeVisible(m_controls);
    // the order of processSynchronBlock: drive, curve, output, hum, noise, quantizer
    m_flow = {{"Drive", {g_paramDrive.ID}, 0, {}},
              {"Curve (memoryless)", {g_paramShaper.ID, g_paramA2.ID, g_paramA3.ID, g_paramThreshold.ID}, 0, {}},
              {"Output", {g_paramOutput.ID}, 0, {}},
              {"Hum (added)", {g_paramHum.ID, g_paramHumLevel.ID}, 1, {}},
              {"Noise (added)", {g_paramNoise.ID, g_paramNoiseLevel.ID}, 1, {}},
              {"Quantizer", {g_paramQuantizer.ID, g_paramBits.ID}, 1, {}}};
    updateControls();
    startTimerHz(kTimerHz);
}

void ReferenceNonlinearGUI::updateControls()
{
    const int shaper = pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramShaper.ID));
    m_controls.setControlEnabled(g_paramA2.ID, shaper == kShaperPolynomial);
    m_controls.setControlEnabled(g_paramA3.ID, shaper == kShaperPolynomial);
    m_controls.setControlEnabled(g_paramThreshold.ID, shaper == kShaperHardClip);
    m_controls.setControlEnabled(g_paramBits.ID, pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramQuantizer.ID)) != kQuantizerOff);
    m_controls.setControlEnabled(g_paramNoiseLevel.ID, pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramNoise.ID)) != kNoiseOff);
    m_controls.setControlEnabled(g_paramHumLevel.ID, pluginlab::refplugins::getChoiceIndex(m_apvts.getRawParameterValue(g_paramHum.ID)) != kHumOff);
}

void ReferenceNonlinearGUI::timerCallback()
{
    updateControls();
}

void ReferenceNonlinearGUI::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).brighter(0.3f);
    const juce::Colour text = getLookAndFeel().findColour(juce::Label::textColourId);
    g.fillAll(background);
    pluginlab::refplugins::drawFlow(g, m_flow, m_scale, background, text);
    g.setColour(text);
    g.drawFittedText(pluginlab::refplugins::getCaption("PluginLab Reference Nonlinear"), getLocalBounds().removeFromBottom(kCaptionHeight),
                     juce::Justification::bottomLeft, 1);
}

void ReferenceNonlinearGUI::resized()
{
    m_scale = static_cast<float>(getWidth()) / static_cast<float>(PLUGINLAB_GUI_WIDTH);
    m_controls.setBounds(getLocalBounds());
    const float margin = kLayoutMargin * m_scale;
    pluginlab::refplugins::layoutFlow(m_controls, m_flow, getLocalBounds().toFloat().reduced(margin).withTrimmedBottom(static_cast<float>(kCaptionHeight)), m_scale);
}
