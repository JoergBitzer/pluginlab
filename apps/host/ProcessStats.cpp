#include "ProcessStats.h"

#if JUCE_WINDOWS
 #include <windows.h>
 #include <psapi.h>
#else
 #include <sys/resource.h>
 #include <unistd.h>
#endif
#if JUCE_MAC
 #include <mach/mach.h>
#endif

namespace pluginlab::host
{
double getProcessCpuSeconds()
{
#if JUCE_WINDOWS
    FILETIME creation{}, exitTime{}, kernel{}, user{};
    if (! GetProcessTimes(GetCurrentProcess(), &creation, &exitTime, &kernel, &user))
    {
        return -1.0;
    }
    const auto toSeconds = [](const FILETIME& time)
    {
        ULARGE_INTEGER value{};
        value.LowPart = time.dwLowDateTime;
        value.HighPart = time.dwHighDateTime;
        return static_cast<double>(value.QuadPart) * 1.0e-7; // 100 ns units
    };
    return toSeconds(kernel) + toSeconds(user);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
    {
        return -1.0;
    }
    return static_cast<double>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) + 1.0e-6 * static_cast<double>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec);
#endif
}

juce::int64 getResidentBytes()
{
#if JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS counters{};
    if (! GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
    {
        return -1;
    }
    return static_cast<juce::int64>(counters.WorkingSetSize);
#elif JUCE_MAC
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
    {
        return -1;
    }
    return static_cast<juce::int64>(info.resident_size);
#else
    // /proc/self/statm: size resident shared ... in pages
    const juce::StringArray fields = juce::StringArray::fromTokens(juce::File("/proc/self/statm").loadFileAsString(), " ", "");
    if (fields.size() < 2)
    {
        return -1;
    }
    return fields[1].getLargeIntValue() * static_cast<juce::int64>(sysconf(_SC_PAGESIZE));
#endif
}
}
