#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pluginlab::host
{
// The content of a component's native window as the system draws it, including native child windows of a hosted plugin editor that JUCE's
// component snapshot does not see (W5d, docs/design/W5d-gui-review.md). An invalid image if the platform method fails.
//   Linux:   the X window by xwd and ImageMagick convert (temporary: a file for the conversion)
//   Windows: PrintWindow with PW_RENDERFULLCONTENT (also DirectX/OpenGL child windows since Windows 8.1)
//   macOS:   the NSView cached into a bitmap in the process (no permission needed; Metal/OpenGL layers may be missing)
juce::Image captureNativeWindow(juce::Component& component, const juce::File& temporary);

// The name of the method of this platform, for the report
juce::String getNativeCaptureName();
}
