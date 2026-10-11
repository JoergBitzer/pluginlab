#pragma once

#include <map>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "tools/ParameterSpec.h"

// Shared by the three reference plugins: a choice parameter defined once (the template's ParameterSpec.h covers float parameters only),
// and a component that shows the controls of the plugin (a knob or a choice box per parameter, its name above, the help text as tooltip),
// all connected with the JUCE attachments: in a grid, or each at a place the plugin chooses (placeCell); a control that has no effect in
// the current setting is greyed out (setControlEnabled). The plugin draws its group boxes and signal-flow arrows behind them (drawGroup,
// drawArrow): the component itself paints nothing.
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
        addCell(juce::String(spec.ID), juce::String(spec.name), std::move(slider));
    }

    void addChoice(const ChoiceSpec& spec);

    // The choice box of a parameter (nullptr if there is none), e.g. to disable entries
    juce::ComboBox* getComboBox(const juce::String& parameterId) const;

    // A control (and its name) greyed out and not operable: the parameter has no effect in the current setting
    void setControlEnabled(const juce::String& parameterId, bool enabled);

    // The place of a control (with its name above it), in the component's coordinates; once any control is placed, the grid is not used
    void placeCell(const juce::String& parameterId, juce::Rectangle<int> bounds);

    void resized() override;

private:
    void addCell(const juce::String& parameterId, const juce::String& name, std::unique_ptr<juce::Component> control);
    void layoutCell(size_t index, juce::Rectangle<int> cell);

    juce::AudioProcessorValueTreeState& m_state;
    std::vector<juce::String> m_ids;
    std::vector<std::unique_ptr<juce::Label>> m_labels;
    std::vector<std::unique_ptr<juce::Component>> m_controls;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> m_sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> m_comboAttachments;
    std::map<juce::String, juce::ComboBox*> m_comboBoxes;
    std::map<juce::String, juce::Rectangle<int>> m_places;
};

// A block of the signal flow: a group of controls in a box with a title, in a row; the blocks follow each other in the order of the
// processing (left to right, then the next row), joined by arrows, "in" before the first and "out" after the last
struct FlowGroup
{
    juce::String title;
    juce::StringArray parameterIds;
    int row = 0;
    juce::Rectangle<float> box; // set by layoutFlow
};

// Places the groups and their controls (cells of equal width, as many as the widest row needs) in the component's coordinates
void layoutFlow(ReferenceControls& controls, std::vector<FlowGroup>& groups, juce::Rectangle<float> area, float scale);

// The boxes of the groups and the arrows between them
void drawFlow(juce::Graphics& g, const std::vector<FlowGroup>& groups, float scale, juce::Colour background, juce::Colour text);

// A group of controls: a box slightly brighter than the panel's background, its title at the top left
void drawGroup(juce::Graphics& g, juce::Rectangle<float> box, const juce::String& title, float titleHeight, juce::Colour background, juce::Colour text);

// A signal-flow arrow from one point to another (straight, or with a right-angle bend when the points are not on one line)
void drawArrow(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float thickness, juce::Colour colour);

// The line at the bottom of every reference plugin: name, version, what it is
juce::String getCaption(const juce::String& pluginName);
}
