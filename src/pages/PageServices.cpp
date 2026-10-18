#include "PageServices.h"

namespace pluginlab::host
{
namespace
{
juce::File& getHelperSetting()
{
    static juce::File executable;
    return executable;
}

bool& getInsideDawSetting()
{
    static bool inside = false;
    return inside;
}
}

juce::File getHelperExecutable()
{
    const juce::File& set = getHelperSetting();
    return set != juce::File() ? set : juce::File::getSpecialLocation(juce::File::currentExecutableFile);
}

void setHelperExecutable(const juce::File& executable)
{
    getHelperSetting() = executable;
}

bool isInsideDaw()
{
    return getInsideDawSetting();
}

void setInsideDaw(bool inside)
{
    getInsideDawSetting() = inside;
}

juce::File getProgressFile(const juce::File& reportFile)
{
    return juce::File(reportFile.getFullPathName() + ".progress");
}

JobProgress readProgressFile(const juce::File& progressFile)
{
    JobProgress progress;
    const juce::StringArray lines = juce::StringArray::fromLines(progressFile.loadFileAsString());
    if (lines.size() >= 2 && lines[0].containsOnly("0123456789."))
    {
        progress.fraction = lines[0].getDoubleValue();
        progress.step = lines[1];
    }
    return progress;
}
}
