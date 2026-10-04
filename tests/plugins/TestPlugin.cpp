#include "TestPlugin.h"

namespace
{
constexpr int kParameterVersion = 1;
constexpr float kMinGainDb = -24.0f;
constexpr float kMaxGainDb = 24.0f;
constexpr float kGainStepDb = 0.1f;
constexpr float kDefaultGainDb = 0.0f;
constexpr float kMinFrequencyHz = 20.0f;
constexpr float kMaxFrequencyHz = 20000.0f;
constexpr float kFrequencySkew = 0.3f;
constexpr float kDefaultFrequencyHz = 1000.0f;
constexpr int kGainDecimals = 1;
const juce::String kStateTag = "TestPluginState";
constexpr int kNumberOfPrograms = 1;
constexpr int kOnlyProgramIndex = 0;

juce::String gainToText(float valueDb, int)
{
    return juce::String(valueDb, kGainDecimals) + " dB";
}
}

#if PLUGINLAB_TEST_PLUGIN_MONO_ONLY
// like ZamGrains: a plugin with one input and one output channel
static const juce::AudioChannelSet kDefaultChannelSet = juce::AudioChannelSet::mono();
#else
static const juce::AudioChannelSet kDefaultChannelSet = juce::AudioChannelSet::stereo();
#endif

TestPluginProcessor::TestPluginProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", kDefaultChannelSet, true)
                               .withOutput("Output", kDefaultChannelSet, true))
{
#if PLUGINLAB_TEST_PLUGIN_CRASHES
    // a plugin that crashes when it is created: scanning it must not take the host down
    volatile int* nothing = nullptr;
    *nothing = 1;
#endif

    m_gain = new juce::AudioParameterFloat(juce::ParameterID{"gain", kParameterVersion}, "Gain",
                                           juce::NormalisableRange<float>(kMinGainDb, kMaxGainDb, kGainStepDb), kDefaultGainDb,
                                           juce::AudioParameterFloatAttributes().withStringFromValueFunction(gainToText));
    m_frequency = new juce::AudioParameterFloat(juce::ParameterID{"frequency", kParameterVersion}, "Frequency",
                                                juce::NormalisableRange<float>(kMinFrequencyHz, kMaxFrequencyHz, 0.0f, kFrequencySkew),
                                                kDefaultFrequencyHz);
    m_mode = new juce::AudioParameterChoice(juce::ParameterID{"mode", kParameterVersion}, "Mode", juce::StringArray{"A", "B", "C"}, 0);
    m_bypass = new juce::AudioParameterBool(juce::ParameterID{"bypass", kParameterVersion}, "Bypass", false);

    addParameter(m_gain);
    addParameter(m_frequency);
    addParameter(m_mode);
    addParameter(m_bypass);
}

const juce::String TestPluginProcessor::getName() const
{
    return JucePlugin_Name;
}

void TestPluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
    setLatencySamples(PLUGINLAB_TEST_PLUGIN_REPORTED_LATENCY);
    m_delayLines.assign(static_cast<size_t>(getTotalNumOutputChannels()), std::vector<float>(PLUGINLAB_TEST_PLUGIN_DELAY_SAMPLES, 0.0f));
    m_delayPosition = 0;
}

void TestPluginProcessor::releaseResources()
{
}

bool TestPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const juce::AudioChannelSet& outputSet = layouts.getMainOutputChannelSet();
    const bool outputIsMonoOrStereo = outputSet == juce::AudioChannelSet::mono() || outputSet == juce::AudioChannelSet::stereo();
#if PLUGINLAB_TEST_PLUGIN_MONO_ONLY
    return outputSet == juce::AudioChannelSet::mono() && layouts.getMainInputChannelSet() == outputSet;
#else
    return outputIsMonoOrStereo && layouts.getMainInputChannelSet() == outputSet;
#endif
}

void TestPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

#if PLUGINLAB_TEST_PLUGIN_CRASHES_IN_PROCESS
    // a plugin that scans and loads fine, but crashes when it processes audio: only a validation with processing finds it
    volatile int* nothing = nullptr;
    *nothing = 1;
#endif

    if (! m_bypass->get())
    {
        buffer.applyGain(juce::Decibels::decibelsToGain(m_gain->get()));
    }

    constexpr int kDelaySamples = PLUGINLAB_TEST_PLUGIN_DELAY_SAMPLES;
    if (kDelaySamples > 0)
    {
        const int channels = juce::jmin(buffer.getNumChannels(), static_cast<int>(m_delayLines.size()));
        for (int channel = 0; channel < channels; ++channel)
        {
            std::vector<float>& line = m_delayLines[static_cast<size_t>(channel)];
            float* data = buffer.getWritePointer(channel);
            int position = m_delayPosition;
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const float delayed = line[static_cast<size_t>(position)];
                line[static_cast<size_t>(position)] = data[sample];
                data[sample] = delayed;
                position = (position + 1) % kDelaySamples;
            }
        }
        m_delayPosition = (m_delayPosition + buffer.getNumSamples()) % kDelaySamples;
    }
}

juce::AudioProcessorEditor* TestPluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

bool TestPluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorParameter* TestPluginProcessor::getBypassParameter() const
{
    return m_bypass;
}

bool TestPluginProcessor::acceptsMidi() const
{
    return false;
}

bool TestPluginProcessor::producesMidi() const
{
    return false;
}

double TestPluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TestPluginProcessor::getNumPrograms()
{
    return kNumberOfPrograms;
}

int TestPluginProcessor::getCurrentProgram()
{
    return kOnlyProgramIndex;
}

void TestPluginProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String TestPluginProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void TestPluginProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void TestPluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement state(kStateTag);
    state.setAttribute("gain", static_cast<double>(m_gain->get()));
    state.setAttribute("frequency", static_cast<double>(m_frequency->get()));
    state.setAttribute("mode", m_mode->getIndex());
    state.setAttribute("bypass", m_bypass->get());
    copyXmlToBinary(state, destData);
}

void TestPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> state = getXmlFromBinary(data, sizeInBytes);
    if (state == nullptr || ! state->hasTagName(kStateTag))
    {
        return;
    }
    *m_gain = static_cast<float>(state->getDoubleAttribute("gain", kDefaultGainDb));
    *m_frequency = static_cast<float>(state->getDoubleAttribute("frequency", kDefaultFrequencyHz));
    *m_mode = state->getIntAttribute("mode", 0);
    *m_bypass = state->getBoolAttribute("bypass", false);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestPluginProcessor();
}
