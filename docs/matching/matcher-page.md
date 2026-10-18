# The matcher: the Matcher page

Unit of W9.6. Code: `apps/host/MatcherPanel.cpp`; the tab "Matcher" of the
stand-alone application (the first of the two main targets; the plugin gets
the matcher with W10). Plan: `docs/design/W9-plan.md`, 4.5 and 4.6.

## The page
| Part | What |
|---|---|
| Master | a loaded plugin as it is set (the user's own EQ), or a band curve typed in (type, frequency, gain, Q; for the teaching) |
| Tolerance, range, time | the level (exact 0.01, strict 0.1 default, medium 0.25, loose 0.5 dB), the frequency range (20 Hz ... 20 kHz by default), the seconds per target (15 s), the model start (needs the target's mapping) |
| Targets | the other loaded plugins; per target "match" on or off and the released parameters: the band parameters (frequency, gain, Q) of its mapping in the user's mapping folder by default, otherwise none; "choose ..." lists all parameters with their values to tick (only the user decides what is optimized; switches stay as the user set them) |
| Live deviation | the curves of the master and the selected target and their difference (target - master, +-2 dB), largest and rms; copies of the two plugins (live instances of the fast path, W9.3) render the knob values of the loaded plugins, read three times a second on the message thread; no audio plays, the loaded plugins are not touched |
| Match | writes a job and runs `PluginLabHost --match` in a process of its own (a crashing plugin does not take the host down); the progress bar with the step and the elapsed time; the live view pauses meanwhile |
| View report | the match report (result, deviation, found settings, why not closer) in a report window |
| Apply to targets | sets the found values of the released parameters on the loaded targets (from `<report>.json`) |
| Save as presets | the state of each target (`getStateInformation`) and a JSON with its values, in `presets/<plugin>/` of the settings folder ("matched to <master> <date>") |
| Listen (Compare) | the page Compare: the user's audio through master and targets, gapless switching with latency compensation (W4) |

Files: jobs and reports in `matcher/` of the settings folder.

`PluginLabHost --matcher <master plugin file | curve> <target plugin file> [--run]`
loads them and opens the page; with `--run` it matches, applies the result and
shows the report (the check in the virtual display).

## Checked
In the virtual display (2026-10-18, 0.61.0): a band curve (peak 1 kHz +6 dB
Q 1) as master, the Test EQ as target with gain, frequency and Q released
from its mapping; before the match the live view shows the -6 dB difference at
1 kHz; the progress bar ("Match: 0 %, PluginLab Test EQ: checks, 0:03"); the
report "matched, 0.067 dB: Gain 5.93, Frequency 1000, Q 1" (5 renders, 4.8 s);
after "Apply" the difference is flat.

## Limits
- Not tried by hand yet (the author): "choose ...", saving presets, listening.
- Presets are saved, not yet loaded by the host (the state can be set again
  with the plugin's own preset handling only if it reads JUCE's state data).
- The released parameters move over their whole range; narrowing a range is
  not on the page yet.
- The live view needs a copy of each plugin; a plugin that cannot run twice
  in one process would fail there.
