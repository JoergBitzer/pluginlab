# Lessons learned from the first prototype (pluginlab 0.6.0, 2026-10-02/03)

State at the time of writing: Python + pedalboard prototype with measurements (big 6), parallel runner, CMA-ES matching of one peaking band, a PySide6 listening GUI and 13 free EQs. Everything below was found by doing it; the evidence is in `docs/findings.md`, `docs/plugin_shortlist.md`, `spikes/` and the git history (tag `prototype-0.6.0`).

## 1. What worked and should be kept
- **Python + pedalboard as host** (decision of the planning phase): enough to load 13 VST3 EQs on Linux, render offline, change parameters and stream blocks. No own host was needed.
- **A numpy reference plugin with known ground truth** (`pluginlab.reference`) made every measurement testable against analytic results (frequency response within 0.1 dB above about 30 Hz, THD, noise, crosstalk, null test). Write the reference first, then the measurement.
- **Measure descriptors, not waveforms:** fitting the swept magnitude response converges in 300-500 renders with CMA-ES in normalized knob space (log scale for frequency and Q), and gives readable results (f0, gain, Q).
- **Calibrating the knobs** ("what does this knob really do?") is the most productive teaching tool found so far: the 1.0875 frequency factor of PeakEQ, the doubled gain of Biquads, the non-Q Q knob of 4K-EQ and the gain-dependent centre frequency of WayQ all came out of it.
- **Worker processes for everything that loads a plugin:** pedalboard refuses to load some plugins from non-main threads, a crashing plugin (LSP: bus error) must not take the GUI down, and plugins are not thread-safe. Spawned processes, one per plugin.
- **A device-independent audio engine** (block sources + mixer with crossfade) could be tested without sound hardware.
- **Configuration in YAML** (plugin registry, per-plugin peak-band mapping, tolerances) kept plugin quirks out of the code.

## 2. Plugins do not behave like a parameter-in, audio-out function (the biggest surprise)
Setting a parameter and rendering gave different results in different plugins. Each of these cost hours and produced wrong conclusions first:
- Own PeakEqualizer: `prepareToPlay` re-initialises with hard-coded values; a parameter is only re-read when it changes.
- Dusk 4K-EQ: values set before the first process call are ignored; a value only counts when it changes after it.
- Dusk Multi-Q: values set before the first process call are applied with 44.1 kHz instead of the real sample rate (frequency 8.75 % too high).
- Dusk Multi-Q: output goes silent for good (NaN state) after a large Q jump; recovers only with a new instance.
- Parameter smoothing: the first render after a change can still be ramping (Multi-Q: 7.95 dB instead of 9.0 dB).
- Reported latency is not reliable and pedalboard treats `reset=True` and streaming calls differently, which looked like "block-size dependent plugins" and was a host bug.
**Consequences for a restart:**
1. Design the host layer around an explicit *parameter delivery protocol* (prime, deliver, settle, render, flush) and a *self-test per plugin* from day one. The test that finally worked: target, target again, alternative, target again must give A, A, B != A, A.
2. A self-test that only checks "repeatable" or "reacts to parameters" is not enough: wrong-but-repeatable strategies pass. Try the most conservative strategy first.
3. Never trust the first measurement of a new plugin; compare it with at least a second strategy before drawing conclusions about the plugin. Two of the early "findings" (block-size dependence, 44.1 kHz in Multi-Q) were host artifacts; the 44.1 kHz hypothesis for PeakEQ is still open.
4. Verify claims about a plugin in its source code when the source is available (own plugins, GPL plugins).

## 3. Measurement
- Exponential sweep: start at 10 Hz (not 20) and keep 150 ms before the IR peak, otherwise the low end is off by 0.3 dB. Harmonics fall before the linear IR and are cut by the window.
- Digital silence in gives digital silence out for most plugins: idle noise is -inf, SNR infinite, crosstalk -inf. Report that as such, do not print -240 dB.
- Latency of the offline path and of the streaming path was identical in the end, but it had to be checked.
- A match of the magnitude response is not a null: ZL EQ 2 matched within 0.03 dB reached only -58.6 dB null depth against the minimum-phase master. Report both.
- The comparison band limits what a test can say: the 18 kHz bilinear-transform test compared only up to 19 kHz, where the cramping is not yet largest. Define the band from the question.

## 4. Matching and optimization
- Knob ranges limit what can be matched (PeakEqualizer frequency knob to 15 kHz, 4K-EQ high-mid band to 7 kHz): record the reachable range per plugin before blaming the filter.
- Single fits can fail (local minima, unconverged runs): repeat with several seeds and keep the best, and report the spread.
- Per-plugin mapping files (which parameters make one peaking band, which are fixed) are necessary and fragile: each plugin needed an hour of exploration. Plan a tool that dumps all parameters and proposes a mapping.
- Some plugins cannot be mapped to the peak-band model (BaxEQ shelving tone control, WSTD MSEQ without Q): the model of "one peaking band" is too narrow for a general tool.

## 5. GUI and real time
- PySide6: connecting a lambda to a signal that is emitted from a worker thread ran the lambda in the worker thread. Emit one signal of the window with the callback and the result as arguments.
- Audio callback: queue parameter changes and apply them between blocks; never raise in the callback.
- The sound output was only checked with silence (no underruns); nobody has listened through it yet.

## 6. Process lessons
- Plugin downloads: KVR and Ko-fi sit behind Cloudflare challenges; GitHub releases, vendor download URLs and `.deb` archives worked. Keep a download script (`tools/fetch_plugins.sh`) and note what had to be done by hand.
- Many plugins are VST2 or LV2 only on Linux and cannot be hosted by pedalboard (SAFEEqualiser, XT-EQ).
- Long experiments: save results after every step (an experiment of 25 minutes was lost to a timeout), run them in the background with a log, and estimate the time from the first iteration. 13 plugin variants x 5 frequencies x 500 renders took about 8 minutes per frequency.
- Do not use `pkill -f <pattern>` when the pattern appears in your own command line.
- CI: Python 3.10 on the ubuntu runner crashed on `import pedalboard` (illegal instruction); plugin tests do not run on CI because no plugins are installed there. Cross-platform CI for plugin tests needs the plugins installed per OS.
- Report what the data shows and what it does not: the BLT table contained knob-limit artifacts and optimizer failures that looked like results.

## 7. Suggested starting points for a fresh start
1. Host layer first, with the delivery protocol and self-test (section 2) and a plugin "fingerprint" report (parameters, latency, delivery strategy, determinism, sample-rate behavior at 44.1/48/96 kHz).
2. Reference plugins and analytic tests before any real plugin.
3. A general "describe this plugin" measurement set (big 6 + response at several settings) before matching; matching only after the descriptors are trustworthy.
4. Separate the model of what an EQ band is (type, frequency, gain, Q, slope, phase behavior) from the per-plugin mapping, so that shelves, high/low-pass and multiple bands fit in.
5. Keep from the prototype: the sweep measurement, the reference EQ, the runner with worker processes, the engine/mixer, the YAML configuration, `docs/findings.md` as the first case-study library.
