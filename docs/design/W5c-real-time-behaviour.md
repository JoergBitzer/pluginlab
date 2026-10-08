# W5c: real-time behaviour in the fingerprint

## The question (author, 2026-10-08)
The fingerprint renders as fast as possible. A plugin that depends on time or on a timer may behave differently when it is listened to,
where one second really is 48000 samples at 48 kHz. Should the fingerprint check for such behaviour?

## What can differ between a fast render and real time
1. **Parameter changes over the message thread.** Plugins that apply a change in a listener, a `juce::Timer` or an `AsyncUpdater`: during a
   fast render the message thread hardly runs, the change arrives thousands of samples late or not at all; in real time a few ms late.
2. **Work on background threads** (coefficients, oversampling filters, impulse responses): finished after a few blocks in real time; in a
   fast render many blocks pass first.
3. **The offline flag** (`AudioProcessor::setNonRealtime`, VST3 `processMode = kOffline`, VST2 process level "offline"): some plugins switch
   to a better (or different) algorithm when rendering offline. Then the measurement shows another algorithm than the one that is heard.
4. **Wall-clock time:** demo noise every N seconds of real time, quality that adapts to the CPU load, LFOs on the system clock.
5. (Related, not part of W5c: tempo and transport. The host gives no playhead; tempo-synced plugins see none.)

## What the host did before W5c (checked 2026-10-08)
- The offline flag was never set: every measurement ran in real-time mode (as in listening), offline mode was never compared.
- The fingerprint runs on the message thread (`PluginLabHost --fingerprint` measures inside `initialise()`, the tests on the main
  thread) and never lets the message loop run: timers, async updates and message-thread listeners of a plugin never ran during a
  fingerprint. Case 1 in its strongest form; it may explain some results (slow settling, lost parameter changes).

## Design
`Rig` (one plugin instance in the fingerprint) gets two switches:
- **offline**: `setNonRealtime(true)` before `prepareToPlay`;
- **paced**: after every block the message loop runs (`MessageManager::runDispatchLoopUntil`) until the wall-clock time equals the audio
  time of the blocks processed so far (one block per block duration: timers and async updates fire as in real time). Needs
  `JUCE_MODAL_LOOPS_PERMITTED=1`, set PUBLIC on `pluginlab_engine` so that every target compiles JUCE the same way.

New measurements (all with the poked delivery of the other measurements, setting B, noise L != R):
- **C, offline mode** (cheap, always): the same render with the offline flag on and off. Different: the plugin switches algorithm offline.
- **A, real-time pace** (`realTimeSeconds`, default 2 s of real time): the same render fast and paced. Different: the plugin depends on time
  or on the message thread.
- **B, parameter change in real time** (`realTimeSeconds`): noise with the setting A, after half of it all parameters change to B; the time
  until the output reaches the output of B (block by block, below `differentAboveDb` relative to a render with B from the start), once fast and
  once paced. "Not reached" in the fast render but reached when paced: the plugin applies parameter changes on the message thread.
- **D, long run** (`longRealTimeSeconds`, default 0 = off; for demo noise and the like): setting A, noise, paced against fast, the 1 s segments
  that differ are listed.
Settings: `realTimeTests` (true), `realTimeSeconds` (2.0), `longRealTimeSeconds` (0). Cost with the defaults: about 5 s of real time per fingerprint.
The pacing is approximate (a normal thread waiting, no audio device): good enough to detect time dependence, not to measure exact timing.
Not changed: the other measurements stay fast and without the message loop (comparable with the earlier reports); C, A and B show what that hides.

Test plugins with known behaviour (TIME_MODE of `pluginlab_add_test_plugin`): **Timer Gain** (the gain parameter reaches the audio through a
100 ms `juce::Timer`), **Offline Switch** (+6 dB when rendering offline), **Wall Clock** (a 2 Hz tremolo on the system clock).

## Progress
0.23.0 (2026-10-08), done on Linux:
- `Rig::setOffline`, `Rig::startPacing` (the message loop runs with `runDispatchLoopUntil` until the wall clock has caught up), `RenderMode` of `Bench::render`,
  `measureRealTime`; results in `PluginFingerprint::realTime`, summary items `offline`, `realTimePace`, `changeTiming` (`longRun` when on), report section
  "Real-time behaviour"; settings `realTimeTests`, `realTimeSeconds`, `longRealTimeSeconds`. `JUCE_MODAL_LOOPS_PERMITTED=1` PUBLIC on `pluginlab_engine`.
- Two corrections on the way: (1) on Linux a JUCE plugin runs its own message thread, so its timers fire by the wall clock whether or not the host's message
  loop runs; the 20 ms timer of the test plugin fired during the slower fast scan, so the scan found no reacting parameter and B was A. Now the real-time test
  makes its own B when the scan found nothing, and the test plugin uses 100 ms. (2) The change is timed against B **rendered at real-time pace** (what is heard),
  not against the fast render (for a timer plugin the fast render of B never contains the change). The margin above the noise floor of two fast renders is
  10 dB (the wall-clock plugin: two fast renders -28.7 dB, paced -5.2 dB).
- Tests (FingerprintTests, three runs, no failure): Gain the same in all three; Timer Gain: the fast render never reaches B, paced after 43 ms; Offline Switch:
  found (0 dB re reference); Wall Clock: found. About 5 s more per fingerprint.
- All reports in `docs/fingerprints/` regenerated (except FreeEQ and FreeGain of Venn Audio: the files were in a temporary folder and are gone). **No real plugin
  differs offline or at real-time pace**, none has a different change timing on Linux. The change timing is a useful number in itself (how long a plugin needs
  until a change is complete, its own transient included): EQoder and PeakEqualizer 11 ms, FreeEQ8, Multi-Q, WayQ, WSTD MSEQ 21 ms, ZeroEQ 32 ms, ShapeIt and
  the Reference EQ 53 ms (no smoothing: the ring-out of the filter at Q 15), PeakEQ 64 ms, Pult EQ 96 ms, BL-Gain12/24 139/149 ms, 4K EQ 149 ms,
  BL-StereoWidth 299 ms; ZL Equalizer 2 and the Reference Nonlinear/Utility "not within the render" both ways (noise and tremolo restart at the change, so the
  output never equals a render of B from the start: alike, no finding).
- Not tested: Windows and macOS, where a plugin shares the host's message thread; there the message loop of the paced render matters most.
