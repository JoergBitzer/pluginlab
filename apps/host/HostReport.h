#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::host
{
// Headless mode of the host (command line: --report <folder> <file>): scans the plugins in the folder with scanner
// processes, loads the plugins that were found and writes what happened into a text file, one line per fact:
//   FOLDER <path>
//   FILE <file name> | <scan status>            (and "  <message>" for problems)
//   PLUGIN <name> | parameters <n>              (after loading)
//   PARAMETER <index> <name> = <value text>
//   LOADERROR <name> | <message>
// Returns false if the report cannot be written.
bool writeReport(const juce::File& pluginFolder, const juce::File& reportFile);
}
