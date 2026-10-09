#include "HostGuiSnapshot.h"

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "pluginlab/hosting/PluginScanner.h"
#include "pluginlab/ui/GuiFormats.h"
#include "pluginlab/ui/GuiReview.h"
#include "pluginlab/ui/PluginEditorFactory.h"

namespace pluginlab::host
{
namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;
constexpr int kSettleMs = 1500;              // the editor paints (and a plugin's timers run) before a capture
constexpr int kCaptureTimeoutMs = 20000;
constexpr double kLowContrastRatio = 1.5;    // edges that only colour carries: luminance contrast below this
constexpr double kEmptyShare = 0.001;        // a capture with fewer differing pixels counts as empty
constexpr int kNoLimit = 100000;             // a maximum size above this is "no limit"

void runMessageLoop(int milliseconds)
{
#if JUCE_MODAL_LOOPS_PERMITTED
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
#else
    juce::Thread::sleep(milliseconds);
#endif
}

bool writePng(const juce::Image& image, const juce::File& file)
{
    if (! image.isValid())
    {
        return false;
    }
    file.deleteFile();
    juce::FileOutputStream stream(file);
    juce::PNGImageFormat png;
    return stream.openedOk() && png.writeImageToStream(image, stream);
}

// The X window of a component on the desktop, read by xwd and converted by ImageMagick (Linux; an invalid image elsewhere or if the tools are missing)
juce::Image captureNativeWindow(juce::Component& component, const juce::File& temporary)
{
#if JUCE_LINUX
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
    return juce::ImageFileFormat::loadFrom(temporary);
#else
    juce::ignoreUnused(component, temporary);
    return {};
#endif
}

struct Capture
{
    juce::String name;
    int width = 0;
    int height = 0;
    juce::Image component;
    juce::Image native;
    bool fitsScreen = true;                  // a window larger than the screen is captured only in part

    // the capture that has content: the X window if JUCE's snapshot is empty (a native child window of a hosted editor)
    const juce::Image& best() const
    {
        if (getContentShare(component) < kEmptyShare && native.isValid())
        {
            return native;
        }
        return component;
    }

    static double getContentShare(const juce::Image& image)
    {
        return pluginlab::ui::getContentShare(image);
    }
};

Capture capture(juce::AudioProcessorEditor& editor, const juce::String& name, const juce::File& folder)
{
    runMessageLoop(kSettleMs);
    Capture result;
    result.name = name;
    result.width = editor.getWidth();
    result.height = editor.getHeight();
    if (const juce::Displays::Display* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        result.fitsScreen = display->totalArea.contains(editor.getScreenBounds());
    }
    result.component = editor.createComponentSnapshot(editor.getLocalBounds());
    result.native = captureNativeWindow(editor, folder.getChildFile("capture_temporary.png"));
    folder.getChildFile("capture_temporary.png").deleteFile();
    writePng(result.component, folder.getChildFile(name + "_component.png"));
    writePng(result.native, folder.getChildFile(name + "_window.png"));
    return result;
}

juce::String percent(double share)
{
    return juce::String(100.0 * share, 1) + " %";
}

juce::String reviewPlugin(juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description, const juce::File& folder)
{
    juce::String text;
    text << "# GUI review: " << description.name << " " << description.version << "\n\n";
    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> instance = formats.createPluginInstance(description, kSampleRate, kBlockSize, error);
    if (instance == nullptr)
    {
        return text + "The plugin could not be created: " + error + "\n";
    }
    std::unique_ptr<juce::AudioProcessorEditor> editor(pluginlab::ui::createEditorFor(*instance));
    const bool generic = ! instance->hasEditor();
    editor->setTopLeftPosition(0, 0);
    editor->addToDesktop(juce::ComponentPeer::windowIsTemporary);
    editor->setVisible(true);

    std::vector<Capture> captures;
    captures.push_back(capture(*editor, "scale_1", folder));
    const int baseWidth = captures.front().width;
    const int baseHeight = captures.front().height;
    for (const double scale : {1.5, 2.0})
    {
        editor->setScaleFactor(static_cast<float>(scale));
        captures.push_back(capture(*editor, "scale_" + juce::String(scale), folder));
    }
    editor->setScaleFactor(1.0f);
    runMessageLoop(kSettleMs);

    const bool resizable = editor->isResizable();
    juce::String sizeText = "not resizable";
    if (resizable)
    {
        sizeText = "resizable";
        if (juce::ComponentBoundsConstrainer* constrainer = editor->getConstrainer())
        {
            // a hosted editor's wrapper may report no real limits (0 ... 2^30): then half and twice the default size are tried
            const bool limited = constrainer->getMinimumWidth() > 0 && constrainer->getMaximumWidth() < kNoLimit;
            if (limited)
            {
                sizeText << ", " << constrainer->getMinimumWidth() << " x " << constrainer->getMinimumHeight() << " ... " << constrainer->getMaximumWidth() << " x "
                         << constrainer->getMaximumHeight();
            }
            else
            {
                sizeText << ", no size limits reported (half and twice the default size tried)";
            }
            editor->setSize(std::max(constrainer->getMinimumWidth(), baseWidth / 2), std::max(constrainer->getMinimumHeight(), baseHeight / 2));
            captures.push_back(capture(*editor, "smallest", folder));
            editor->setSize(std::min(constrainer->getMaximumWidth(), 2 * baseWidth), std::min(constrainer->getMaximumHeight(), 2 * baseHeight));
            captures.push_back(capture(*editor, "largest", folder));
        }
    }

    // the vision variants of the scale-1 capture, a low-contrast map and the contact sheets
    const juce::Image base = captures.front().best();
    std::vector<std::pair<juce::String, juce::Image>> variants;
    for (const pluginlab::ui::VisionVariant variant : pluginlab::ui::getAllVariants())
    {
        const juce::Image image = pluginlab::ui::makeVariant(base, variant);
        writePng(image, folder.getChildFile("variant_" + pluginlab::ui::getVariantName(variant) + ".png"));
        variants.emplace_back(pluginlab::ui::getVariantName(variant), image);
    }
    double lowContrastShare = 0.0;
    const juce::Image map = pluginlab::ui::makeLowContrastMap(base, kLowContrastRatio, lowContrastShare);
    writePng(map, folder.getChildFile("low_contrast_map.png"));
    variants.emplace_back("low contrast (red)", map);
    writePng(pluginlab::ui::makeContactSheet(variants, 3), folder.getChildFile("contact_sheet_vision.png"));
    std::vector<std::pair<juce::String, juce::Image>> sizes;
    for (const Capture& item : captures)
    {
        sizes.emplace_back(item.name + " (" + juce::String(item.width) + " x " + juce::String(item.height) + ")", item.best());
    }
    writePng(pluginlab::ui::makeContactSheet(sizes, 3), folder.getChildFile("contact_sheet_sizes.png"));

    juce::String editorText = "the plugin's own";
    if (generic)
    {
        editorText = "none (the generic editor of the host is shown)";
    }
    text << "Editor: " << editorText << "; size " << baseWidth
         << " x " << baseHeight << "; " << sizeText << ".\n\n";
    text << "| Capture | Size | JUCE snapshot has content | X window capture has content | Used |\n|---|---|---|---|---|\n";
    for (const Capture& item : captures)
    {
        const double componentShare = Capture::getContentShare(item.component);
        const double nativeShare = Capture::getContentShare(item.native);
        juce::String used = "JUCE snapshot";
        if (&item.best() == &item.native)
        {
            used = "X window";
        }
        juce::String nativeText = "not available";
        if (item.native.isValid())
        {
            nativeText = percent(nativeShare);
        }
        text << "| " << item.name << " | " << item.width << " x " << item.height << " | " << percent(componentShare) << " | " << nativeText << " | " << used << " |\n";
    }
    // W5d.3: the scale factor (size and content) and resizing
    text << "\n**Scale factor** (the host asks for 1.5 and 2; content: the capture reduced to the default size against the default capture, luminance "
            "correlation):\n\n| Factor | Size | Size ratio | Content similarity | Behaviour |\n|---|---|---|---|---|\n";
    pluginlab::ui::ScaleBehaviour behaviourAtTwo = pluginlab::ui::ScaleBehaviour::Ignores;
    double similarityAtTwo = 0.0;
    for (size_t index = 1; index <= 2; ++index)
    {
        const double factor = 1.0 + 0.5 * static_cast<double>(index);
        const double ratio = static_cast<double>(captures[index].width) / baseWidth;
        const double similarity = pluginlab::ui::getImageSimilarity(base, captures[index].best());
        pluginlab::ui::ScaleBehaviour behaviour = pluginlab::ui::judgeScaling(factor, ratio, similarity);
        if (! captures[index].fitsScreen)
        {
            behaviour = pluginlab::ui::ScaleBehaviour::NotJudged;
        }
        if (index == 2)
        {
            behaviourAtTwo = behaviour;
            similarityAtTwo = similarity;
        }
        text << "| " << factor << " | " << captures[index].width << " x " << captures[index].height << " | " << juce::String(ratio, 3) << " | "
             << juce::String(similarity, 3) << " | " << pluginlab::ui::describe(behaviour) << " |\n";
    }
    juce::String resizeText = "not resizable";
    if (captures.size() > 4)
    {
        const Capture& largest = captures.back();
        const double similarity = pluginlab::ui::getImageSimilarity(base, largest.best());
        juce::String layout = "the layout changes (re-laid out or stretched: compare contact_sheet_sizes.png)";
        if (similarity >= pluginlab::ui::kSameContentSimilarity)
        {
            layout = "the content zooms with the window";
        }
        resizeText = "resized to " + juce::String(largest.width) + " x " + juce::String(largest.height) + ": content similarity " + juce::String(similarity, 3) + ", "
                     + layout + "; the smallest size reached is " + juce::String(captures[3].width) + " x " + juce::String(captures[3].height);
    }
    text << "\n**Resizing**: " << resizeText << ".\n";

    // the single results for programs (the Developer page, W5d.7)
    auto json = std::make_unique<juce::DynamicObject>();
    json->setProperty("plugin", description.name);
    json->setProperty("width", baseWidth);
    json->setProperty("height", baseHeight);
    json->setProperty("resizable", resizable);
    json->setProperty("scaleBehaviour", pluginlab::ui::describe(behaviourAtTwo));
    json->setProperty("scaleSimilarity", similarityAtTwo);
    json->setProperty("lowContrastShare", lowContrastShare);
    json->setProperty("componentSnapshotEmpty", Capture::getContentShare(captures.front().component) < kEmptyShare);
    folder.getChildFile("gui_review.json").replaceWithText(juce::JSON::toString(juce::var(json.release())));
    text << "Low-contrast edges (colour changes with a luminance contrast below " << kLowContrastRatio << ":1): " << percent(lowContrastShare)
         << " of the colour edges (a hint, not a verdict; see low_contrast_map.png).\n";
    text << "\nImages: contact_sheet_vision.png (original, grayscale, protanopia, deuteranopia, tritanopia, low-contrast map), contact_sheet_sizes.png, and the "
            "single captures.\n";

    // leave the editor as it was opened: many plugins store their editor size, the review must not change it
    editor->setScaleFactor(1.0f);
    editor->setSize(baseWidth, baseHeight);
    runMessageLoop(kSettleMs);
    editor->setVisible(false);
    editor->removeFromDesktop();
    editor.reset();
    return text;
}
}

bool writeGuiSnapshots(const juce::File& pluginFile, const juce::File& folder, const juce::String& pluginIdentifier)
{
    juce::AudioPluginFormatManager formats;
    pluginlab::ui::addGuiFormats(formats);
    const hosting::PluginScanResult scan = hosting::PluginScanner::scanFileInProcess(formats, pluginFile);
    if (scan.descriptions.isEmpty() || ! folder.createDirectory())
    {
        return false;
    }
    juce::String report;
    for (const juce::PluginDescription& description : scan.descriptions)
    {
        if (pluginIdentifier.isNotEmpty() && description.createIdentifierString() != pluginIdentifier)
        {
            continue;
        }
        report += reviewPlugin(formats, description, folder) + "\n";
    }
    if (report.isEmpty())
    {
        return false;
    }
    return folder.getChildFile("gui_review.md").replaceWithText(report);
}
}
