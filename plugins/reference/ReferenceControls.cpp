#include "ReferenceControls.h"

#include "PluginSettings.h"

namespace pluginlab::refplugins
{
namespace
{
constexpr int kColumns = 5;
constexpr int kLabelHeight = 18;
constexpr int kCellHeight = 110;
constexpr int kComboHeight = 26;
constexpr int kMargin = 6;
}

std::unique_ptr<juce::AudioParameterChoice> makeChoiceParameter(const ChoiceSpec& spec)
{
    jade::parameterHelpTexts()[juce::String(spec.ID)] = juce::String(spec.help);
    return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(juce::String(spec.ID), 1), juce::String(spec.name), spec.choices, spec.defaultIndex);
}

juce::String choiceHelpText(const ChoiceSpec& spec)
{
    return juce::String(spec.name) + " (" + spec.choices.joinIntoString(", ") + "; default " + spec.choices[spec.defaultIndex] + "): " + juce::String(spec.help);
}

int getChoiceIndex(const std::atomic<float>* rawValue)
{
    return juce::roundToInt(rawValue->load());
}

ReferenceControls::ReferenceControls(juce::AudioProcessorValueTreeState& state)
    : m_state(state)
{
}

void ReferenceControls::addChoice(const ChoiceSpec& spec)
{
    auto box = std::make_unique<juce::ComboBox>();
    box->addItemList(spec.choices, 1);
    box->setTooltip(choiceHelpText(spec));
    m_comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(m_state, juce::String(spec.ID), *box));
    addCell(juce::String(spec.name), std::move(box));
}

void ReferenceControls::addCell(const juce::String& name, std::unique_ptr<juce::Component> control)
{
    auto label = std::make_unique<juce::Label>(juce::String(), name);
    label->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(*label);
    addAndMakeVisible(*control);
    m_labels.push_back(std::move(label));
    m_controls.push_back(std::move(control));
}

void ReferenceControls::resized()
{
    const int cellWidth = getWidth() / kColumns;
    for (size_t index = 0; index < m_controls.size(); ++index)
    {
        const int column = static_cast<int>(index) % kColumns;
        const int row = static_cast<int>(index) / kColumns;
        juce::Rectangle<int> cell(column * cellWidth, row * kCellHeight, cellWidth, kCellHeight);
        cell = cell.reduced(kMargin);
        m_labels[index]->setBounds(cell.removeFromTop(kLabelHeight));
        if (dynamic_cast<juce::ComboBox*>(m_controls[index].get()) != nullptr)
        {
            cell = cell.withSizeKeepingCentre(cell.getWidth(), kComboHeight);
        }
        m_controls[index]->setBounds(cell);
    }
}

juce::String getCaption(const juce::String& pluginName)
{
    return pluginName + " V " + juce::String(PLUGIN_VERSION_MAJOR) + "." + juce::String(PLUGIN_VERSION_MINOR) + "." + juce::String(PLUGIN_VERSION_PATCH)
           + "  -  pluginlab reference: the exact algorithm of pluginlab_reference, no smoothing, latency 0";
}
}
