#pragma once

#include <juce_graphics/juce_graphics.h>

namespace pluginlab::ui
{
// The logo of the reviews, "Jade Benchmark" (the author's choice; J. B. as in Joerg Bitzer): a "JB" monogram with a sine on a rounded square
// in the red of the Jade Hochschule, the name beside it in red and gray; drawn, no asset file
juce::Image makeLogo(int height);

// Writes the logo as PNG into the folder under the name the report heads refer to (engine::kLogoFileName); true if it is there
bool writeLogo(const juce::File& folder, const juce::String& fileName);
}
