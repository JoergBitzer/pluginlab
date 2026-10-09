#include "NativeWindowCapture.h"

namespace pluginlab::host
{
namespace
{
constexpr int kCaptureTimeoutMs = 20000;
}

juce::Image captureNativeWindow(juce::Component& component, const juce::File& temporary)
{
    juce::ComponentPeer* peer = component.getPeer();
    if (peer == nullptr)
    {
        return {};
    }
    const auto windowId = static_cast<juce::uint64>(reinterpret_cast<juce::pointer_sized_uint>(peer->getNativeHandle()));
    temporary.deleteFile();
    juce::ChildProcess process;
    const juce::String command = "xwd -silent -id " + juce::String(windowId) + " | convert xwd:- png:" + temporary.getFullPathName().quoted();
    if (! process.start(juce::StringArray{"sh", "-c", command}) || ! process.waitForProcessToFinish(kCaptureTimeoutMs) || ! temporary.existsAsFile())
    {
        return {};
    }
    const juce::Image image = juce::ImageFileFormat::loadFrom(temporary);
    temporary.deleteFile();
    return image;
}

juce::String getNativeCaptureName()
{
    return "X window (xwd)";
}
}
