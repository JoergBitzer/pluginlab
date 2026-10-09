#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::host
{
// The CPU time (user + system, seconds) this process has used so far, and its resident memory (bytes); -1 where the platform gives none.
// For the GUI review's load and robustness (W5d.4, W5d.5).
double getProcessCpuSeconds();
juce::int64 getResidentBytes();
}
