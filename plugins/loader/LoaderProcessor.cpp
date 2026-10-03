#include "LoaderProcessor.h"

#include "pluginlab/PluginLabVersion.h"

namespace
{
constexpr int kNumberOfPrograms = 1;
constexpr int kOnlyProgramIndex = 0;
}

LoaderProcessor::LoaderProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

const juce::String LoaderProcessor::getName() const
{
    return "PluginLabLoader";
}

void LoaderProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void LoaderProcessor::releaseResources()
{
}

bool LoaderProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const juce::AudioChannelSet& outputSet = layouts.getMainOutputChannelSet();
    const bool outputIsMonoOrStereo = outputSet == juce::AudioChannelSet::mono() || outputSet == juce::AudioChannelSet::stereo();
    const bool inputMatchesOutput = layouts.getMainInputChannelSet() == outputSet;
    return outputIsMonoOrStereo && inputMatchesOutput;
}

void LoaderProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(buffer, midiMessages);
    juce::ScopedNoDenormals noDenormals;
    // pass-through: the audio stays in the buffer unchanged
}

juce::AudioProcessorEditor* LoaderProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

bool LoaderProcessor::hasEditor() const
{
    return true;
}

bool LoaderProcessor::acceptsMidi() const
{
    return false;
}

bool LoaderProcessor::producesMidi() const
{
    return false;
}

double LoaderProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LoaderProcessor::getNumPrograms()
{
    return kNumberOfPrograms;
}

int LoaderProcessor::getCurrentProgram()
{
    return kOnlyProgramIndex;
}

void LoaderProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String LoaderProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void LoaderProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void LoaderProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void LoaderProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
}

// the entry point that the plugin wrappers call
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LoaderProcessor();
}
