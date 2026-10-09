#include "NativeWindowCapture.h"

#import <Cocoa/Cocoa.h>

namespace pluginlab::host
{
juce::Image captureNativeWindow(juce::Component& component, const juce::File&)
{
    juce::ComponentPeer* peer = component.getPeer();
    if (peer == nullptr)
    {
        return {};
    }
    juce::Image image;
    @autoreleasepool
    {
        NSView* view = static_cast<NSView*>(peer->getNativeHandle());
        const NSRect bounds = [view bounds];
        NSBitmapImageRep* bitmap = [view bitmapImageRepForCachingDisplayInRect:bounds];
        if (bitmap == nil)
        {
            return {};
        }
        [view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
        NSData* png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        if (png == nil)
        {
            return {};
        }
        image = juce::ImageFileFormat::loadFrom([png bytes], static_cast<size_t>([png length]));
    }
    return image;
}

juce::String getNativeCaptureName()
{
    return "native window (NSView cacheDisplayInRect, in the process)";
}
}
