#include "pluginlab/hosting/HostedPlugin.h"

namespace pluginlab::hosting
{
namespace
{
constexpr int kMaxTextLength = 64;
// VST3 has no boolean parameters: a switch is a discrete parameter with two values (two steps)
constexpr int kBooleanLikeStepCount = 2;
}

HostedPlugin::HostedPlugin(juce::PluginDescription description, std::unique_ptr<juce::AudioPluginInstance> instance)
    : m_description(std::move(description)), m_instance(std::move(instance))
{
}

std::unique_ptr<HostedPlugin> HostedPlugin::load(juce::AudioPluginFormatManager& formatManager,
                                                 const juce::PluginDescription& description,
                                                 double sampleRate,
                                                 int blockSize,
                                                 juce::String& errorMessage)
{
    std::unique_ptr<juce::AudioPluginInstance> instance =
        formatManager.createPluginInstance(description, sampleRate, blockSize, errorMessage);
    if (instance == nullptr)
    {
        if (errorMessage.isEmpty())
        {
            errorMessage = "The plugin " + description.name + " could not be loaded";
        }
        return nullptr;
    }
    return std::unique_ptr<HostedPlugin>(new HostedPlugin(description, std::move(instance)));
}

const juce::PluginDescription& HostedPlugin::getDescription() const
{
    return m_description;
}

juce::AudioPluginInstance& HostedPlugin::getInstance()
{
    return *m_instance;
}

ParameterInfo HostedPlugin::describe(const juce::AudioProcessorParameter& parameter)
{
    ParameterInfo info;
    info.index = parameter.getParameterIndex();
    info.name = parameter.getName(kMaxTextLength);
    info.label = parameter.getLabel();
    info.valueText = parameter.getText(parameter.getValue(), kMaxTextLength);
    info.normalisedValue = parameter.getValue();
    info.defaultNormalisedValue = parameter.getDefaultValue();
    info.numSteps = parameter.getNumSteps();
    info.isDiscrete = parameter.isDiscrete();
    info.isBoolean = parameter.isBoolean() || (parameter.isDiscrete() && parameter.getNumSteps() == kBooleanLikeStepCount);
    info.isAutomatable = parameter.isAutomatable();
    return info;
}

std::vector<ParameterInfo> HostedPlugin::getParameters() const
{
    std::vector<ParameterInfo> infos;
    for (const juce::AudioProcessorParameter* parameter : m_instance->getParameters())
    {
        infos.push_back(describe(*parameter));
    }
    return infos;
}

ParameterInfo HostedPlugin::getParameter(int index) const
{
    return describe(*m_instance->getParameters()[index]);
}

void HostedPlugin::setParameterNormalised(int index, float normalisedValue)
{
    const juce::Array<juce::AudioProcessorParameter*>& parameters = m_instance->getParameters();
    if (index < 0 || index >= parameters.size())
    {
        return;
    }
    parameters[index]->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, normalisedValue));
}
}
