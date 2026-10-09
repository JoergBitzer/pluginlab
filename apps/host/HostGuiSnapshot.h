#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::host
{
// Command line mode --gui-snapshot <plugin file> <output folder> [<plugin identifier>] (W5d, docs/design/W5d-gui-review.md): opens the editor of the
// plugin in a window of its own, captures it (JUCE's component snapshot and the native window: NativeWindowCapture.h), at the scale factors 1, 1.5 and 2 and, if the
// editor is resizable, at its smallest and largest size; writes the images, the vision variants (grayscale, colour-vision simulations), a
// low-contrast map, a contact sheet and a Markdown summary (gui_review.md) into the folder. Meant for a virtual display (xvfb-run): it opens windows.
bool writeGuiSnapshots(const juce::File& pluginFile, const juce::File& folder, const juce::String& pluginIdentifier = {});
}
