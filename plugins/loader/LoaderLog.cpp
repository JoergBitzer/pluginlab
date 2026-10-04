#include "LoaderLog.h"

namespace
{
const juce::String kFolderName = "PluginLab";
const juce::String kFileName = "PluginLabLoader.log";
constexpr juce::int64 kMaximumFileSize = 1024 * 1024;
}

namespace loaderlog
{
juce::File getLogFile()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile(kFolderName).getChildFile(kFileName);
}

void write(const juce::String& message)
{
    static juce::CriticalSection lock;
    const juce::ScopedLock scopedLock(lock);

    const juce::File file = getLogFile();
    file.getParentDirectory().createDirectory();
    if (file.getSize() > kMaximumFileSize)
    {
        file.deleteFile();
    }
    juce::String thread = "other thread";
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        thread = "message thread";
    }
    file.appendText(juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S") + " [" + thread + "] " + message + "\n");
}
}
