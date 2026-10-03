#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

namespace pluginlab::hosting
{
struct ParameterInfo
{
    int index = 0;
    juce::String name;
    juce::String label;     // the unit
    juce::String valueText; // the current value as the plugin displays it
    float normalisedValue = 0.0f;
    float defaultNormalisedValue = 0.0f;
    int numSteps = 0;
    bool isDiscrete = false;
    bool isBoolean = false; // also true for a discrete parameter with two values (VST3 has no boolean flag)
    bool isAutomatable = false;
};

// One loaded plugin. W2: loading and parameters; audio processing comes with W4.
class HostedPlugin
{
public:
    // Loads the plugin; on failure returns nullptr and sets errorMessage. Call on the message thread.
    static std::unique_ptr<HostedPlugin> load(juce::AudioPluginFormatManager& formatManager,
                                              const juce::PluginDescription& description,
                                              double sampleRate,
                                              int blockSize,
                                              juce::String& errorMessage);

    const juce::PluginDescription& getDescription() const;
    juce::AudioPluginInstance& getInstance();

    std::vector<ParameterInfo> getParameters() const;
    ParameterInfo getParameter(int index) const;

    // value between 0 and 1; out-of-range indices are ignored
    void setParameterNormalised(int index, float normalisedValue);

private:
    HostedPlugin(juce::PluginDescription description, std::unique_ptr<juce::AudioPluginInstance> instance);

    static ParameterInfo describe(const juce::AudioProcessorParameter& parameter);

    juce::PluginDescription m_description;
    std::unique_ptr<juce::AudioPluginInstance> m_instance;
};
}
