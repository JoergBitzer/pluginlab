#include "NativeWindowCapture.h"

#include <vector>

#include <windows.h>

namespace pluginlab::host
{
namespace
{
constexpr UINT kRenderFullContent = 0x00000002; // PW_RENDERFULLCONTENT (not in every SDK's headers)
constexpr UINT kClientOnly = 0x00000001;        // PW_CLIENTONLY
}

juce::Image captureNativeWindow(juce::Component& component, const juce::File&)
{
    juce::ComponentPeer* peer = component.getPeer();
    if (peer == nullptr)
    {
        return {};
    }
    const auto window = static_cast<HWND>(peer->getNativeHandle());
    RECT client{};
    if (! GetClientRect(window, &client))
    {
        return {};
    }
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 0 || height <= 0)
    {
        return {};
    }
    HDC windowContext = GetDC(window);
    HDC memoryContext = CreateCompatibleDC(windowContext);
    HBITMAP bitmap = CreateCompatibleBitmap(windowContext, width, height);
    HGDIOBJ previous = SelectObject(memoryContext, bitmap);
    const BOOL printed = PrintWindow(window, memoryContext, kClientOnly | kRenderFullContent);

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; // top-down rows
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    std::vector<juce::uint32> pixels(static_cast<size_t>(width) * static_cast<size_t>(height));
    const int rows = GetDIBits(memoryContext, bitmap, 0, static_cast<UINT>(height), pixels.data(), &info, DIB_RGB_COLORS);

    SelectObject(memoryContext, previous);
    DeleteObject(bitmap);
    DeleteDC(memoryContext);
    ReleaseDC(window, windowContext);
    if (! printed || rows != height)
    {
        return {};
    }
    juce::Image image(juce::Image::ARGB, width, height, false);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            // BGRX in memory: 0x00RRGGBB as a 32-bit value
            const juce::uint32 value = pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
            image.setPixelAt(x, y, juce::Colour(static_cast<juce::uint8>((value >> 16) & 0xff), static_cast<juce::uint8>((value >> 8) & 0xff),
                                                static_cast<juce::uint8>(value & 0xff)));
        }
    }
    return image;
}

juce::String getNativeCaptureName()
{
    return "native window (PrintWindow, PW_RENDERFULLCONTENT)";
}
}
