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
constexpr int kNumberOfPrograms = 1;
constexpr int kOnlyProgramIndex = 0;

juce::String gainToText(float valueDb, int)
{
    return juce::String(valueDb, kGainDecimals) + " dB";
}
}

TestPluginProcessor::TestPluginProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
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
}

void TestPluginProcessor::releaseResources()
{
}

bool TestPluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const juce::AudioChannelSet& outputSet = layouts.getMainOutputChannelSet();
    const bool outputIsMonoOrStereo = outputSet == juce::AudioChannelSet::mono() || outputSet == juce::AudioChannelSet::stereo();
    return outputIsMonoOrStereo && layouts.getMainInputChannelSet() == outputSet;
}

void TestPluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    if (m_bypass->get())
    {
        return;
    }
    buffer.applyGain(juce::Decibels::decibelsToGain(m_gain->get()));
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
    juce::ignoreUnused(destData);
}

void TestPluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TestPluginProcessor();
}
