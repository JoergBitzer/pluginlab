#pragma once

#include <juce_core/juce_core.h>

namespace pluginlab::host
{
// What the pages need from the shell they run in (W10.1): the application or the plugin (docs/design/W10-plan.md, 3.1).

// The program that runs the jobs of the pages in a process of its own (--fingerprint, --map, --calibrate, --match, --gui-snapshot): the
// application itself by default; the plugin sets the PluginLabHost it carries in its bundle (a crashing plugin must not take the DAW down)
juce::File getHelperExecutable();
void setHelperExecutable(const juce::File& executable);

// Inside a DAW (the plugin, W10.2): the DAW plays its track through the engine; the pages hide what belongs to the application's own audio
// (files, the audio device, play and render on the page Compare)
bool isInsideDaw();
void setInsideDaw(bool inside);

// While --fingerprint, --map, --calibrate and --match run, they write what they are doing to <report file>.progress: the fraction done (0 ... 1) on the first line, the
// step on the second (the Developer page draws a progress bar from it), and print each new step. The file is removed at the end.
juce::File getProgressFile(const juce::File& reportFile);

struct JobProgress
{
    double fraction = -1.0; // negative: unknown
    juce::String step;
};

// Reads such a file (fraction -1 and an empty step if there is none)
JobProgress readProgressFile(const juce::File& progressFile);
}
