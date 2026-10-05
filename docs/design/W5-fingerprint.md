# W5: parameter delivery protocol and plugin fingerprint

Status: started 2026-10-05. Definition of done (planning §8): fingerprint of all test plugins; behavior at 44.1/48/96 kHz and different block sizes recorded; quirks documented;
the PeakEQ question answered (with source code).

## The PeakEQ question (answered from the source, to be confirmed by the fingerprint)
Prototype finding: the own PeakEQ (`AAT_EQ1`) has a measured centre frequency 1.0875 x the knob at every frequency (500 -> 543 Hz, 8 kHz -> 8640 Hz), also in the streaming
host. Hypothesis: filter designed for 44.1 kHz, running at 48 kHz (48000/44100 = 1.0884).
Source (`AAT_EQ1/PeakEQ.h`, `PeakEQ.cpp`): `float m_fs = 44100.f` is a member of `PeakEQAudio`; `designCoeffs()` uses `w0 = 2*pi*m_f0/m_fs`; `prepareToPlay(double sampleRate, ...)`
computes the block size from `sampleRate` and resets smoothers, but **never assigns `m_fs = sampleRate`** (the processor's own `m_fs` is another variable). The filter is
therefore always designed for 44.1 kHz. At 44.1 kHz the knob is right, at 48 kHz the centre frequency is 1.0884 x too high, at 96 kHz 2.177 x. One line fixes it
(`m_fs = sampleRate;` in `prepareToPlay`). The fingerprint must show exactly this pattern (the factor follows the sample rate); that is the test of the sample-rate measurement.

## What the fingerprint is (generic, for any plugin)
| Item | How |
|---|---|
| Parameters | names, range texts at 0 / default / 1, number of steps (as in the host parameter list) |
| Channel layouts | which of mono / stereo the plugin accepts |
| Latency | reported and measured (impulse peak), at 44.1, 48 and 96 kHz |
| Delivery behavior | the A, A, B, A test (LESSONS_LEARNED §2) for three strategies: parameters set after prepare (stream), set before prepare, and a fresh instance per render; a strategy passes if A = A and A = A again and B differs from A |
| Which parameters change the audio | every parameter moved to 0.25 and 0.75 with noise through the plugin: RMS change in dB (the "reachable" parameters; the others are display only, bypass-like, or smoothed away) |
| Sample-rate dependence | at a setting that changes the response: impulse response at 44.1, 48 and 96 kHz, magnitude on a common frequency axis; the largest difference between the rates and the frequency of the largest deviation from 0 dB at every rate (centre frequency of a bell); the ratio of these frequencies to the ratio of the rates |
| Block-size independence | the same noise through the plugin at block sizes 32 ... 2048 (and a prime size): largest difference to the 512 run |
| Determinism | two fresh instances, same input: identical output? |
| Robustness | every parameter jumped to 0 and 1 and back with noise running: any NaN or infinity, and does the plugin return to its first output (A again) |
| Silence | digital silence in: digital silence out? (idle noise level) |

Output: a text report (Markdown) per plugin, and the same as a data structure for tests and for the later "developer page".

## Test plugins with known faults (ground truth for the fingerprint)
`PluginLabTestEq` is an RBJ peaking EQ; three builds: correct; `FsBug` (designed for 44.1 kHz whatever the host rate: the PeakEQ fault); `PrepareBug` (`prepareToPlay` resets the filter to
hard-coded values and a parameter is read again only when its value changes: the PeakEqualizer template fault). The tests check that the fingerprint finds exactly these faults and
none in the correct build and in the gain plugin.

## Order of work
W5.1 test EQ plugins; W5.2 measurement code (`pluginlab_engine`, `Fingerprint`); W5.3 report and command line (`PluginLabHost --fingerprint <plugin file> <report file>`) and tests;
W5.4 run on the author's plugins (PeakEQ, PeakEqualizer, the free EQs) and document; W5.5 the page "Developer" in the host (later, with W10).

## Results (2026-10-05, version 0.11.0)
Code: `src/engine/Fingerprint.{h,cpp}` (measurements and report), `PluginLabHost --fingerprint <plugin file> <report>` (a process of its own: a crashing plugin gives no report), tests in
`tests/FingerprintTests.cpp` with the plugins of known behavior (`PluginLabTestEq` correct, `...Fs` designed for 44.1 kHz, `...Prepare` resets in prepareToPlay, the gain plugin and the two
latency plugins). The reports of the author's plugins and the free EQs of the prototype are in `docs/fingerprints/`.

### The PeakEQ question is answered
`AAT_EQ1/PeakEQ.h` has `float m_fs = 44100.f` and `PeakEQ.cpp` never assigns the sample rate to it (the processor's own `m_fs` is another variable): `designCoeffs()` is always designed for
44.1 kHz. The fingerprint finds exactly that, from the outside, with the same setting at three rates: the bell is at **3304 Hz at 44.1 kHz, 3591 Hz at 48 kHz (factor 1.087, the factor
of the prototype), 7289 Hz at 96 kHz (factor 2.206; the rates differ by 2.177)**. The test EQ with the same fault gives the same pattern, the correct test EQ does not. The fix is one line
(`m_fs = sampleRate;` in `PeakEQAudio::prepareToPlay`). Second finding for the same plugin: it reports a latency of 0 samples but delays by one synchronous block (88 / 96 / 192 samples at
44.1 / 48 / 96 kHz, measured); the template calls `setLatencySamples(m_algo.getLatency())` once in the constructor (value 0), and `PeakEQAudio::prepareToPlay` adds the block size to `m_Latency` later (and adds it again at every prepare) without telling the host.
The fix was not made in `AAT_EQ1` (not part of this repository); the author decides.

### What the measurement had to learn on the way (each cost a wrong result first)
- **Setting time is a time, not a number of samples.** A fixed 4096 samples of silence before the signal was enough at 48 kHz but not at 96 kHz for a plugin that smooths its parameters
  over 50 ms in blocks of 2 ms: the bell had not arrived, and the feature looked like a factor 1.18 instead of 2.2. The settle time is now 0.25 s whatever the rate.
- **A switch undoes the others.** Moving every parameter that "changes the audio" to 0.75 also moved the bypass, and the response of the setting B was flat. Switches (two steps, or "bypass" in
  the name) are not moved together with the others; the second scan of "which parameters matter" runs with the first ones moved (the frequency of an EQ does nothing at 0 dB gain).
- **Set the parameters the careful way.** A plugin that reads a parameter only when it changes (the PeakEqualizer template) loses a setting that equals what it saw in `prepareToPlay`. Every
  measurement therefore sets every parameter to another value first, runs one block, then sets the target ("poked" delivery). The A, A, B, A test is run for four ways of delivery and
  also compares each with the output that the same setting gives when reached by a change.
- A plugin that was never prepared does not process (JUCE-hosted VST3, no error); the hosted VST3 delivers parameter changes with the next process call.

### What the first runs on real plugins show (to be read with the caution below)
| Plugin | Findings |
|---|---|
| PeakEQ (own) | designed for 44.1 kHz (above); latency 0 reported, 88/96/192 measured; everything else clean |
| PeakEqualizer (own) | latency 0 reported, 88/96/192 measured; bell at 3591 Hz at all three rates (correct design); delivery ok with the poked way |
| EQoder (own) | latency 0 reported, 87/95/191 measured; the "feature" is at 13.6 kHz / 7.3 kHz / 212 Hz at the three rates: with the setting B the frequency parameters seem to depend on the rate (needs a look at the source) |
| FreeEQ8, ZeroEQ | large differences between the rates at the same normalised setting (14.8 dB, 43 dB): the knob ranges are probably tied to the sample rate or a parameter hits an extreme (ZeroEQ: 335 dB feature gain); needs per-plugin mapping (W8) before a conclusion |
| Multi-Q (Dusk) | output depends on the block size; does not come back after jumps of parameters to both ends; idle noise of -149 dBFS; the poked delivery does NOT reproduce the output of a change (only "stream" passes): matches the prototype findings (state poisoning, stale 44.1 kHz) |
| WayQ | no delivery way passes (the output depends on the history of the parameters: A again 4.6 dB); latency 1 sample at 96 kHz only |
| WSTD MSEQ | latency 0 reported, 2 measured at all rates |
| ShapeIt (free, Soundly) | nothing unusual |
**Caution:** the setting B moves the reacting parameters to 0.75 of their *normalised* range. If a knob range depends on the sample rate, or a parameter reaches an extreme, "the response differs between
the rates" is a statement about the knob, not necessarily about the filter. To separate the two the response must be compared at the same frequency in Hz, which needs the per-plugin mapping of
W8. The finding about the PeakEQ stands because the correct test EQ and PeakEqualizer, with the same procedure, show the same bell at all rates.

### Not done / next
- The "developer page" in the host (W10), a Hz-based comparison once the mapping exists (W8), the fingerprint of all free EQs of the final test set on all three systems in CI (needs the plugins there),
  the fingerprint of plugins in a separate process by the GUI (today: command line).

## 0.12.0: the page "Developer" in the host (author's request)
- Third page next to Plugins and Compare. It lists the plugins that are loaded on the Plugins page (they follow loading and unloading). Per plugin two buttons: **Generate report** and, when
  a report exists, **View report**. The status column says "no report yet / waiting / measuring / ready / ready, but the plugin is newer than the report / failed". Several reports can be queued; one
  runs at a time.
- The measurement runs in a **process of its own** (`PluginLabHost --fingerprint <plugin file> <report> <plugin identifier>`): a plugin that crashes during the tests leaves no report and the row says
  "failed: the plugin crashed or could not be measured"; the host stays up. The identifier selects one plugin of a file that holds many (a bundle of 190 plugins is not measured as a whole). The
  report is written to a temporary file and put in place when it is complete.
- Reports are kept in `~/.config/pluginlab/fingerprints/` (one file per plugin, named from the name and the identifier) and shown again after a restart; if the plugin is newer than its report the row says so.
- The view is a window with the report as text in a monospaced font (tables aligned by `alignMarkdownTables`, headings underlined; a text editor does not reliably switch fonts line by line, so one
  font only). The tests are technical and independent of the kind of algorithm (they run noise, impulses and parameter changes through the plugin); the "feature" of the response (bell frequency) is a
  generic detector of the largest deviation from 0 dB, nothing that needs to know that the plugin is an EQ.
- Manual test options: `--developer <plugin file> [--view]` (loads the plugin, opens the page, makes the report, shows it). Tests: `alignMarkdownTables`, CTest `PluginLabHostFingerprint` (the command line
  mode end to end, also the error for an identifier that is not in the file).
