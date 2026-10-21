#pragma once

#include <cstdlib>

#include <juce_core/juce_core.h>

namespace testenvironment
{
// Sets an environment variable of this process (and of the processes it starts): setenv on Linux and macOS, _putenv_s on Windows
inline void set(const juce::String& name, const juce::String& value)
{
#if JUCE_WINDOWS
    _putenv_s(name.toRawUTF8(), value.toRawUTF8());
#else
    setenv(name.toRawUTF8(), value.toRawUTF8(), 1);
#endif
}
}
