#include "CrashLog.h"

#include <juce_core/juce_core.h>

#include "LoaderLog.h"

#if JUCE_LINUX || JUCE_MAC
#include <execinfo.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace
{
constexpr int kMaximumFrames = 64;
constexpr int kCrashSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL};
constexpr int kNumberOfCrashSignals = sizeof(kCrashSignals) / sizeof(kCrashSignals[0]);
constexpr char kCrashHeader[] = "\n*** CRASH of the process, backtrace:\n";

int g_logDescriptor = -1;
struct sigaction g_previousActions[kNumberOfCrashSignals];

void crashHandler(int signalNumber, siginfo_t* info, void* context)
{
    void* frames[kMaximumFrames];
    const int count = backtrace(frames, kMaximumFrames);
    if (g_logDescriptor >= 0)
    {
        write(g_logDescriptor, kCrashHeader, sizeof(kCrashHeader) - 1);
        backtrace_symbols_fd(frames, count, g_logDescriptor);
    }

    for (int index = 0; index < kNumberOfCrashSignals; ++index)
    {
        if (kCrashSignals[index] != signalNumber)
        {
            continue;
        }
        sigaction(signalNumber, &g_previousActions[index], nullptr);
        const bool previousWantsInfo = (g_previousActions[index].sa_flags & SA_SIGINFO) != 0;
        if (previousWantsInfo && g_previousActions[index].sa_sigaction != nullptr)
        {
            g_previousActions[index].sa_sigaction(signalNumber, info, context);
        }
        else if (! previousWantsInfo && g_previousActions[index].sa_handler != SIG_IGN && g_previousActions[index].sa_handler != SIG_DFL)
        {
            g_previousActions[index].sa_handler(signalNumber);
        }
    }
    raise(signalNumber); // the old handler returned, or there was none: crash as before
}
}

namespace loaderlog
{
void installCrashHandler()
{
    static bool installed = false;
    if (installed)
    {
        return;
    }
    installed = true;

    const juce::File file = getLogFile();
    file.getParentDirectory().createDirectory();
    g_logDescriptor = open(file.getFullPathName().toRawUTF8(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);

    for (int index = 0; index < kNumberOfCrashSignals; ++index)
    {
        struct sigaction action;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = crashHandler;
        sigaction(kCrashSignals[index], &action, &g_previousActions[index]);
    }
    loaderlog::write("crash handler installed");
}
}
#else
namespace loaderlog
{
void installCrashHandler()
{
}
}
#endif
