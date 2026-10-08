#include "pluginlab/measure/Device.h"

namespace pluginlab::measure
{
Device makeProcessorDevice(std::function<std::unique_ptr<reference::Processor>(double sampleRate)> factory)
{
    return [factory](const juce::AudioBuffer<float>& input, double sampleRate)
    {
        juce::AudioBuffer<float> output;
        output.makeCopyOf(input);
        const std::unique_ptr<reference::Processor> processor = factory(sampleRate);
        processor->process(output);
        return output;
    };
}
}
