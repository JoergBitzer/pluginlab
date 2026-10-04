#include <juce_gui_basics/juce_gui_basics.h>

#include "TestPluginPaths.h"
#include "pluginlab/ui/PluginBrowserComponent.h"

namespace
{
constexpr int kColumnPlugin = 1;
constexpr int kWaitStepMs = 100;
constexpr int kScanTimeoutMs = 120000;
}

// The list of the plugin browser: scanning the folder of the test plugins and sorting by a column.
class BrowserTests : public juce::UnitTest
{
public:
    BrowserTests()
        : juce::UnitTest("PluginBrowser", "pluginlab")
    {
    }

    void runTest() override
    {
        pluginlab::ui::PluginBrowserComponent browser;
        bool finished = false;
        browser.onScanFinished = [&finished] { finished = true; };

        beginTest("scanning a folder fills the list");
        browser.scanFolder(testpaths::getTestPluginFolder());
        for (int waited = 0; ! finished && waited < kScanTimeoutMs; waited += kWaitStepMs)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil(kWaitStepMs);
        }
        expect(finished, "the scan did not finish");
        expect(browser.getNumEntries() >= 3, "entries: " + juce::String(browser.getNumEntries()));

        beginTest("sorting by the plugin name, forwards and backwards");
        browser.sortBy(kColumnPlugin, true);
        expectSorted(browser, true);
        browser.sortBy(kColumnPlugin, false);
        expectSorted(browser, false);
    }

private:
    static bool isInOrder(int order, bool forwards)
    {
        if (forwards)
        {
            return order <= 0;
        }
        return order >= 0;
    }

    void expectSorted(const pluginlab::ui::PluginBrowserComponent& browser, bool forwards)
    {
        for (int index = 1; index < browser.getNumEntries(); ++index)
        {
            const int order = browser.getEntryName(index - 1).compareNatural(browser.getEntryName(index));
            const bool inOrder = isInOrder(order, forwards);
            expect(inOrder, browser.getEntryName(index - 1) + " / " + browser.getEntryName(index));
        }
    }
};

static BrowserTests browserTests;
