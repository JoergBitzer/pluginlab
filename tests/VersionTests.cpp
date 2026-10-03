#include <string>

#include <juce_core/juce_core.h>

#include "pluginlab/PluginLabVersion.h"

class VersionTests : public juce::UnitTest
{
public:
    VersionTests()
        : juce::UnitTest("Version", "pluginlab")
    {
    }

    void runTest() override
    {
        beginTest("the numbers are not negative");
        const pluginlab::VersionNumber version = pluginlab::getVersion();
        expect(version.major >= 0);
        expect(version.minor >= 0);
        expect(version.patch >= 0);

        beginTest("the string is major.minor.patch");
        const std::string expected = std::to_string(version.major) + "." + std::to_string(version.minor) + "."
                                   + std::to_string(version.patch);
        expectEquals(juce::String(pluginlab::getVersionString()), juce::String(expected));
    }
};

static VersionTests versionTests;
