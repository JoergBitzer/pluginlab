#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "tools/ParameterSpec.h"

// Shared by the three reference plugins: a choice parameter defined once (the template's ParameterSpec.h covers float parameters only),
// and a component that shows the controls of the plugin in a grid (a knob or a choice box per parameter, its name above, the help text
// as tooltip), all connected with the JUCE attachments.
namespace pluginlab::refplugins
{
struct ChoiceSpec
{
    std::string ID;
    std::string name;
    juce::StringArray choices;
    int defaultIndex = 0;
    std::string help;
};

std::unique_ptr<juce::AudioParameterChoice> makeChoiceParameter(const ChoiceSpec& spec);
juce::String choiceHelpText(const ChoiceSpec& spec);

// The index of a choice parameter from its raw value (the APVTS stores the index as a float)
int getChoiceIndex(const std::atomic<float>* rawValue);

class ReferenceControls : public juce::Component
{
public:
    explicit ReferenceControls(juce::AudioProcessorValueTreeState& state);

    template <class Spec>
    void addKnob(const Spec& spec)
    {
        auto slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        slider->setTooltip(jade::helpText(spec));
        m_sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(m_state, juce::String(spec.ID), *slider));
        addCell(juce::String(spec.name), std::move(slider));
    }

    void addChoice(const ChoiceSpec& spec);

    void resized() override;

private:
    void addCell(const juce::String& name, std::unique_ptr<juce::Component> control);

    juce::AudioProcessorValueTreeState& m_state;
    std::vector<std::unique_ptr<juce::Label>> m_labels;
    std::vector<std::unique_ptr<juce::Component>> m_controls;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> m_sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> m_comboAttachments;
};

// The line at the bottom of every reference plugin: name, version, what it is
juce::String getCaption(const juce::String& pluginName);
}
