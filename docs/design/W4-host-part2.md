# W4: stand-alone host, part 2 (audio files, parallel rendering, gapless switching, latency, output files)

Status: started 2026-10-04 (version 0.8.0 and following). Definition of done (planning §8): gapless switching without xruns; latency of each test plugin measured and
compensated; files written.

## What it is
The host gets an audio engine: a list of audio files (with loop regions) is played through up to N plugin "slots" **at the same time**; the listener switches between the
slots (A/B/C/...) without a gap; every slot can also render the file list offline into its own output file. This is the base of the comparison tool (W5 onwards).

## Decisions (author may overrule)
1. **All slots always run on the same input.** Switching does not start or stop a plugin: it only moves a short crossfade (default 10 ms) between the outputs of the slots.
   The plugins never see a gap, so there are no clicks from plugin state, and a plugin's own smoothing/tail is not cut. The reference ("dry", no plugin) is a slot, too.
2. **Latency is measured, not read** (LESSONS_LEARNED §2: reported latency is not reliable). Each slot is measured with an impulse in a scratch run of its own instance
   (`LatencyMeasurer`); the engine delays every slot output to the largest measured latency, so all outputs are time aligned and a switch does not jump in time. The
   latency a plugin reports is kept next to the measured one and a difference is shown (a finding for the teaching material).
3. **Parameter delivery protocol** (LESSONS_LEARNED §2) in a minimal form: a slot is prepared, its parameters are set, then it runs a number of warm-up blocks before it
   counts as ready; changing a parameter later also lets the slot settle for a number of blocks. The full fingerprint test is W5.
4. **In-process first, process isolation later.** The slots run in the process of the host, each in the thread that calls it (sequential in the audio callback in the first
   version, one worker thread per slot in the second). A plugin that crashes takes the host down; the scanner, the validator and the quick check keep known-bad
   plugins out. The `RenderSlot` interface keeps a process-based slot possible (planning §9, item 5) without changing the engine. (Decision to be confirmed by the author
   before the final plugin; for W4 the in-process path is the simple one to test.)
5. **Fixed internal block size** (the largest block the audio device announces); a device block of any size is cut into pieces. Files are read into memory (float, the
   sample rate of the device); loop points are in samples of the file.
6. **Channels:** stereo engine; mono files are copied to both channels; a mono plugin gets the mean of the channels (same adapter as the loader, `ChannelAdapter`).

## Parts (library `pluginlab_engine`, no GUI)
| Part | Job |
|---|---|
| `AudioFileSource` | an audio file in memory at the engine's sample rate, loop region, `read(buffer)` advancing and wrapping |
| `ChannelAdapter` | mono plugin in a stereo engine and back (extracted from the loader) |
| `LatencyMeasurer` | impulse through a plugin instance: measured latency in samples, compared with the reported one |
| `RenderSlot` | one hosted plugin (or none: the dry reference) with its adapter, delay for the compensation, warm-up/settle counter |
| `MeasurementEngine` | file list, slots, active slot, crossfade, `processBlock`, latency compensation, real-time safe parameter changes |
| `OfflineRenderer` | file list through every slot, one WAV per slot (latency compensated, loops unrolled to a chosen number of passes) |

## Order of work
W4.1 engine core with tests (no device): file source, adapter, latency measurement, slots, switching and compensation. W4.2 offline rendering to files (command line
`--render`). W4.3 real-time playback (audio device) with a window: file list, slots, A/B buttons, latency display. W4.4 worker thread per slot.

## Tests (ground truth)
- Test plugins with a known delay: 64 samples reported 64, and 100 samples reported 0 (the liar): the measurement must give 64 and 100.
- Gain plugins at different settings as slots: after the compensation all slot outputs are aligned (null of two identical slots is exact zero), switching with a
  sine input shows no jump larger than the crossfade allows and no silent block.
- File source: loop wrap without a gap (sample exact), mono to stereo, sample rate conversion keeps the pitch.

## Progress
- **W4.1 + offline renderer: done (0.8.0, 2026-10-04).** Library `pluginlab_engine` (`AudioFileSource`, `ChannelAdapter` (the loader uses it now), `LatencyMeasurer`, `RenderSlot`,
  `MeasurementEngine`, `OfflineRenderer`); tests in `tests/EngineTests.cpp` pass: a file's passes follow each other sample exactly, mono becomes stereo, a 44.1 kHz file played at
  48 kHz keeps its pitch (440 zero crossings in one second), the latency is measured for the plugin that reports it right (64), the one that lies (100, reports 0) and the
  one without latency (0), all slots are aligned (the impulse of every slot comes out at sample 110 = 10 + the slowest latency), switching a sine between a dry slot and a +12 dB slot
  every 50 blocks gives no silent block and a largest step of 0.115 between samples (the analytic limit of the signal is 0.115; a hard switch would jump by about 1.5), and the offline
  render writes one aligned 24 bit file per slot that equals the input to 1e-5.
- Not yet: command line `--render` and the window with the audio device (W4.3), worker threads per slot (W4.4), xrun measurement with a real device.
- **W4.3 window and real-time playback: done on Linux (0.9.0, 2026-10-04).** The host has two pages (`HostShell`): "Plugins" (as before) and "Compare" (`ComparePanel`): audio files with
  passes and loop region, slots (dry slot, a plugin selected on the Plugins page), the audible slot by row or key 1 ... 9, measured and reported latency and the delay for
  the alignment per slot, play through the audio device (the engine asks the device for its rate and refuses a mismatch), crossfade time, engine rate, "Render to files...".
  Command line for manual tests: `--compare <audio file> [<plugin file>]` and `--play <seconds> <report file>`. Checked on this machine (PipeWire through ALSA): 6 s of playback with a
  dry slot and the latency test plugin: 0 xruns, latency 64 samples measured. The window was checked in a screenshot.
- Still open in W4: **W4.4 worker thread per slot** (today the slots run one after the other inside the audio callback; with the test plugins the load is negligible, with several
  heavy plugins it will matter), a longer xrun test with real plugins and several slots, a command line `--render` (the button does it; the library is tested), loops of
  the whole list are always on in listening mode. Decision to confirm: in-process slots (see decision 4).

## 0.10.0: the author's review of the host (2026-10-04/05)
- **Type column** in the plugin list: "effect|Delay|Mono", "instrument|Synth|Drum" (`getTypeText`, from the category the plugin reports); sortable like the other columns.
- **Audio files:** the dialog selects several files (Ctrl, Shift) and adds all after the confirmation; it opens in the folder of the last use (`host_settings.xml`).
- **Plugins page and slots are one list.** The loaded plugins ARE the slots of the engine: Load on the Plugins page puts the plugin into the comparison, Unload removes it
  (no second selection on the Compare page). The list of the browser selects several plugins and Load loads all of them one after the other (a validation or quick check of a
  plugin in the queue runs before the next one is loaded; the status line gives "Loaded n of m, not loaded: ..."). The dry slot is always slot 0. The editor windows belong
  to the Plugins page; the Compare page asks for them.
- **Saving and loading:** `*.audiolist` (files with passes and loop region) and `*.pluginset` (the plugins with their states, in the order of the slots, and the audible slot), written
  and read by `SessionFiles` (tests: round trip, a missing audio file and a missing plugin are reported and skipped, the others load). Buttons on the Compare page (audio list)
  and the Plugins page (plugin set); the dialogs open in the folder of the last list.
- **Last session:** the two lists are written to `~/.config/pluginlab/last_session.*` whenever they change. At the next start the host asks, separately for the audio list and
  the plugin set, whether to load them (buttons Load / Skip); the plugin question warns that a plugin can crash the program. While the plugins are loaded a marker file exists; if
  it is still there at the next start the question says that the last attempt did not finish (probably a crash). Nothing is written before the questions are answered,
  and a start with a command line test option neither asks nor writes the session.
- Release build: the host in `build-release/` has no JUCE debug assertions (the parameter id checks of JUCE complain about the parameter tables of third-party plugins).
- **0.10.1:** a shorter loop region was not kept in the saved audio list (the file said "up to the full length"). The region fields were applied only on Return and only if a file
  row was selected (after loading a list none was), so a typed value was silently dropped. Now the fields are applied while typing, on Return and when left, the first file is
  selected after a load, the fields are disabled without a selected file, and a region with a start and no end runs to the end of the file. The load and save code itself was
  correct (round trip test) and the saved file showed the full length because the engine never got the typed value.
