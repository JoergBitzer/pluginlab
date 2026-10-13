#include "pluginlab/engine/LivePluginDevice.h"

#include <algorithm>
#include <cmath>

#include "PluginRig.h"
#include "pluginlab/engine/PluginDevice.h"

namespace pluginlab::engine
{
struct LivePluginTarget::State
{
    FingerprintSettings settings;   // the rig keeps a reference to it
    std::unique_ptr<detail::Rig> rig;
    LiveDeviceOptions options;
    double sampleRate = 48000.0;
    int renders = 0;
};

LivePluginTarget::LivePluginTarget(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, double sampleRate,
                                   const LiveDeviceOptions& options)
    : m_state(std::make_shared<State>())
{
    m_state->options = options;
    m_state->sampleRate = sampleRate;
    m_state->rig = std::make_unique<detail::Rig>(formatManager, description, sampleRate, options.blockSize, m_state->settings);
    if (m_state->rig->isValid())
    {
        m_state->rig->prepare();
    }
}

LivePluginTarget::~LivePluginTarget() = default;

bool LivePluginTarget::isValid() const
{
    return m_state->rig != nullptr && m_state->rig->isValid();
}

juce::String LivePluginTarget::getError() const
{
    return m_state->rig != nullptr ? m_state->rig->getError() : juce::String("no instance");
}

double LivePluginTarget::getSampleRate() const
{
    return m_state->sampleRate;
}

int LivePluginTarget::getRenders() const
{
    return m_state->renders;
}

measure::Device LivePluginTarget::makeDevice(const std::vector<float>& setting) const
{
    const std::shared_ptr<State> state = m_state;
    return [state, setting](const juce::AudioBuffer<float>& input, double sampleRate) {
        if (state->rig == nullptr || ! state->rig->isValid() || std::abs(sampleRate - state->sampleRate) > 1.0e-6)
        {
            return juce::AudioBuffer<float>(); // the live instance runs at one rate only
        }
        detail::Rig& rig = *state->rig;
        if (state->options.poke)
        {
            rig.apply(detail::makePokeValues(setting, state->settings));
            rig.settleFor(static_cast<double>(state->options.blockSize) / sampleRate);
        }
        rig.apply(setting);
        rig.settleFor(state->options.settleSeconds);
        detail::Signal signal;
        for (int channel = 0; channel < input.getNumChannels(); ++channel)
        {
            const float* data = input.getReadPointer(channel);
            signal.emplace_back(data, data + input.getNumSamples());
        }
        const detail::Signal output = rig.process(signal);
        ++state->renders;
        const int length = static_cast<int>(detail::getLength(output));
        juce::AudioBuffer<float> buffer(static_cast<int>(output.size()), length);
        for (size_t channel = 0; channel < output.size(); ++channel)
        {
            buffer.copyFrom(static_cast<int>(channel), 0, output[channel].data(), length);
        }
        return buffer;
    };
}

LiveTargetCheck checkLiveTarget(juce::AudioPluginFormatManager& formatManager, const juce::PluginDescription& description, const LivePluginTarget& target,
                                const std::vector<float>& setting, const std::function<std::vector<double>(const measure::Device&)>& features,
                                double repeatTolerance)
{
    LiveTargetCheck check;
    if (! target.isValid())
    {
        check.reason = "the plugin cannot be loaded: " + target.getError();
        return check;
    }
    measure::SystemClassSettings classification;
    classification.sampleRate = target.getSampleRate();
    const measure::SystemClassResult result = measure::classifySystem(makePluginDevice(formatManager, description, setting), classification);
    check.systemClass = result.systemClass;
    if (result.systemClass != measure::SystemClass::Lti)
    {
        check.reason = "not LTI (" + measure::getSystemClassName(result.systemClass) + ": " + result.reason
                       + "): matching assumes a transfer function; refused";
        return check;
    }
    const std::vector<double> first = features(target.makeDevice(setting));
    const std::vector<double> second = features(target.makeDevice(setting));
    for (size_t index = 0; index < first.size() && index < second.size(); ++index)
    {
        check.repeatDifference = std::max(check.repeatDifference, std::abs(first[index] - second[index]));
    }
    if (first.empty() || first.size() != second.size() || check.repeatDifference > repeatTolerance)
    {
        check.reason = "two renders of the same setting differ by " + juce::String(check.repeatDifference, 4)
                       + ": the plugin keeps state between renders or is not deterministic; the fast path cannot be used";
        return check;
    }
    check.usable = true;
    return check;
}
}
