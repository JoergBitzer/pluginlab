#include "BenchmarkProcessor.h"

// the plugin's entry point; only the plugin target compiles it (the tests link the core library together with the loader's core)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BenchmarkProcessor();
}
