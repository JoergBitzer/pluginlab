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
