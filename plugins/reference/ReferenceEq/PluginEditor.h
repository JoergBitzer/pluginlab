#pragma once

#include "PluginProcessor.h"
// #include "JadeLookAndFeel.h"
#include "tools/PresetHandler.h"
#include "tools/MidiModPitchState.h"
#if WITH_DAYNIGHT
    #if ! WITH_PRESETHANDLERGUI
        #error "WITH_DAYNIGHT needs WITH_PRESETHANDLERGUI: the day/night button sits in the preset bar"
    #endif
    #include "tools/DayNightLookAndFeel.h"
#endif


#include "ReferenceEq.h"

//==============================================================================
class ReferenceEqAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                            private juce::AudioProcessorParameter::Listener
{
public:
    
    explicit ReferenceEqAudioProcessorEditor (ReferenceEqAudioProcessor&);
    ~ReferenceEqAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Turns the preset bar's Save button red once the user changes a parameter.
    // Listens to parameter GESTURES, which only user actions produce (slider drags,
    // typed values, double-click resets, buttons, combo boxes via the JUCE attachments)
    // -- not preset loading or host automation, which would otherwise mark a just-loaded
    // preset as changed. Gestures may arrive on any thread, hence the async hop.
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

#if WITH_DAYNIGHT
    // Day/night theme (WITH_DAYNIGHT in CMakeLists.txt). Declared before all components, so the
    // LookAndFeel is destroyed after them. The choice is stored per user (m_userSettings).
    void toggleTheme();
    std::unique_ptr<juce::PropertiesFile> m_userSettings;
    jade::DayNightLookAndFeel m_lookAndFeel;
    jade::ThemeButton m_themeButton;
#endif
    // JadeLookAndFeel m_jadeLAF;
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    ReferenceEqAudioProcessor& m_processorRef;
    PresetComponent m_presetGUI;
#if WITH_MIDIKEYBOARD    
    MidiKeyboardComponent m_keyboard;
    MidiModPitchBendStateComponent m_wheels;    
#endif
    // plugin specific components
    ReferenceEqGUI m_editor;
    // shows the tooltips (setTooltip) of all controls in this window; 700 ms before it appears
    juce::TooltipWindow m_tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceEqAudioProcessorEditor)
};
