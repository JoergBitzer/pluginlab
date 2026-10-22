#pragma once
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include "tools/MidiModPitchState.h"
#include "tools/PresetHandler.h"
#include "ReferenceCompressor.h"

//==============================================================================
class ReferenceCompressorAudioProcessor  : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    friend class ReferenceCompressorAudioProcessorEditor;
    //==============================================================================
    ReferenceCompressorAudioProcessor();
    ~ReferenceCompressorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // bypassed: the input delayed by the latency (the look-ahead), so that a host's latency compensation keeps it aligned
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getScaleFactor(){return m_pluginScaleFactor;};
    void setScaleFactor(float newscalefactor){m_pluginScaleFactor = newscalefactor;};
    
    // Algo component and ValueTreeState must be public to be accessed by the editor
    ReferenceCompressorAudio m_algo;
    std::unique_ptr<AudioProcessorValueTreeState> m_parameterVTS;
private:
    // the look-ahead is the latency: reported again when it changes (on the thread that sets the parameter, not the audio thread)
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    CriticalSection m_protect;
    juce::AudioBuffer<float> m_bypassDelay;
    int m_bypassPosition = 0;
    float m_fs; // sampling rate is always needed

    //Parameterhandling
    std::vector <std::unique_ptr<RangedAudioParameter>> m_paramVector;
	PresetHandler m_presets;
    float m_pluginScaleFactor = 1.0;    
#if WITH_MIDIKEYBOARD    
    MidiKeyboardState m_keyboardState;
    MidiModPitchBendState m_wheelState;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceCompressorAudioProcessor)
};
