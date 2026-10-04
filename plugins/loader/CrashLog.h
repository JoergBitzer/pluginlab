#pragma once

namespace loaderlog
{
// Writes a stack backtrace into the log file when the process receives a crash signal (Linux and macOS), then passes the signal on to
// the handler that was installed before (the DAW's own). Installed once; later calls do nothing.
void installCrashHandler();
}
