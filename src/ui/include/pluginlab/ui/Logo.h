#pragma once

#include <juce_graphics/juce_graphics.h>

namespace pluginlab::ui
{
// The logo of the reviews, "Jade Benchmark" (the author's choice; J. B. as in Joerg Bitzer): a "JB" monogram in light gray with an si impulse
// (two side lobes on each side) on a rounded square in the red of the Jade Hochschule, the name beside it in red and gray; drawn, no asset file
juce::Image makeLogo(int height);

// Writes the logo as PNG into the folder under the name the report heads refer to (engine::kLogoFileName), replacing an older one; true if it is there
bool writeLogo(const juce::File& folder, const juce::String& fileName);
}
