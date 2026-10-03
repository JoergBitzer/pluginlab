# pluginlab - planning (fresh start)

Status: draft v3 (2026-10-03). Second attempt, **product only (JUCE/C++)**. The first prototype (repo `measurement_tool`,
tag `prototype-0.6.0`) was a learning vehicle; its results are in [docs/prototype/](docs/prototype/). The vision, personas,
feature list, development path and rules come from the author's document
[docs/reference/Measurement Tool Development plan.md](docs/reference/Measurement%20Tool%20Development%20plan.md),
the **reference for this plan**. Nothing is decided until marked **DECIDED**. No code has been written yet.

## 1. Mission and final product
**Mission: divide the myth from reality** (what plugins really do, versus what their marketing and their knobs say).

**Final product (from the reference):** a measurement VST plugin that loads up to 4 other plugins and has measurement units for
different plugin types (EQ, dynamics, delay, reverb). It has a pool of test signals, can also render audio from the DAW through all
plugins in parallel, and switches between the algorithms (delay, gain etc. equalized inside). It has an automatic parameter matcher;
the user can open the GUI and choose which parameters to compare or use as starting points for the optimization.

**Personas**
- *Simple musician with some technical interest*: reads about a new plugin, downloads the demo, wants to compare it with plugins already owned. => easy to use.
- *Plugin developer*: develops an algorithm, wants to check everything and compare with competitors. => "something like pluginval, but for the processing, not just the specification."

**Teaching is a main goal:** show that different tools do the same thing when adjusted correctly, and teach a method for finding out *why* they differ (the "audio detective"). Real-time listening so that students hear the differences.

## 2. Feature list of the final product (reference, condensed)
- Load up to 4 plugins in parallel, show their GUIs in separate windows.
- Render audio from the DAW, from files or from test signals through all plugins in parallel; output can be saved or listened to; gapless switching between plugins in real time.
- Measurements, easy to add new ones: frequency response, phase, group delay, THD, THD+N, noise, SNR, crosstalk, null test with alignment (and the big 6 and extensions of section 6).
- Real-time display of the transfer function (continuously updated with a click or a short sweep) while the user adjusts parameters.
- Automatic parameter matcher (e.g. free EQ to a commercial EQ): user or tool chooses the start point; user chooses which parameters to match or ignore, the frequency range and the tolerance; real-time display of the deviation.
- GUI with easy page, expert page, developer page; user chooses which measurements to show; presets for the measurements.
(The reference leaves the last list item empty.)

## 3. Scope and the role of Python (**DECIDED**)
This repository is **the product only: JUCE/C++**. The Python work stays where it is:
- The measurement algorithms were prototyped in Python in the repo `measurement_tool` (sweep response, phase, group delay, THD, THD+N, noise/SNR, crosstalk, null test, matching, host experiments). That repo continues to exist; **how far the Python path goes there is open** and is not planned here.
- Python is used here only as an **oracle**: a C++ measurement is accepted when it agrees with the validated Python implementation (and with analytic ground truth) within a stated tolerance. The reference (RBJ, Orfanidis, Zölzer) designs are the ground truth for EQ tests; the C++ test suite carries its own copies of the reference processors.
- Teaching notebooks are not part of this repository's scope for now.

## 4. Development path (reference)
The project is huge and is divided into parts that can be developed and optimized independently.

**A. Stand-alone host (JUCE, C++, Windows/Mac/Linux, VST3 only; VST2 later via the project the reference names).** Pre-test to learn how to load plugins and render audio. Required abilities (reference list): scan for plugins and list them; load one plugin and show its parameters; show its GUI in a window; load several plugins with separate GUI windows; load audio from disk and render through the plugins (save or listen); render through multiple plugins in parallel; switch between plugins gaplessly without stopping the audio engine; measure latency and gain of each plugin and compensate; save each plugin's output to a separate file; simple GUI. It can become a stand-alone product without a DAW; audio files in an easily switchable list, each with loop positions that can be set in the GUI.

**B. Simple VST plugin that loads other plugins:** scan the standard installation paths, list the plugins, load one, show its parameters and GUI.

**C. Measurement units** (C++, tested against the oracle and ground truth).

**D. The final measurement plugin** that combines A-C, the matcher and the three-level GUI.

## 5. What the prototype taught and what changes
Details and evidence: [docs/prototype/LESSONS_LEARNED.md](docs/prototype/LESSONS_LEARNED.md), [docs/prototype/findings.md](docs/prototype/findings.md). The prototype hosted plugins through pedalboard, so some lessons concern pedalboard; the plugin behavior itself carries over to a JUCE host.
1. **Plugins are not functions "parameters in, audio out".** Observed in the prototype: parameters set before the first process call were lost (PeakEqualizer, 4K-EQ), applied with a stale 44.1 kHz sample rate (Multi-Q), delivered only after a later change, smoothed (first render still ramping), or poisoned the filter state (NaN, silence after a large Q jump). Block-size and latency effects came from the host layer. => A real host calls `prepareToPlay` and delivers parameters in the order a DAW does; the host and the measurement units must be tested against these plugins, and the host needs an explicit *parameter delivery protocol* (prepare, deliver, settle, process, flush) and a *self-test per plugin* (target, target, alternative, target must give A, A, B != A, A). A repeatable result is not a correct result. Whether a JUCE host shows the same quirks as pedalboard (also JUCE-based) has to be measured, not assumed.
2. **Plugin fingerprint** as the first deliverable per plugin: parameters and ranges, reported and measured latency, delivery behavior, determinism, behavior at 44.1/48/96 kHz, block-size independence, reachable knob ranges. In the product this is the "developer page".
3. **Ground truth before real plugins:** reference processors (RBJ, Orfanidis, Zölzer) and analytic tests for every measurement.
4. **A general EQ band model** (type, frequency, gain, Q, slope, phase behavior) separated from per-plugin mapping data; a tool that lists all parameters of a plugin and proposes a mapping for the matcher. The prototype's "one peaking band" model was too narrow (shelves, high/low-pass, multiple bands, fixed-Q bands).
5. **Crash isolation:** some plugins crash on load (LSP: bus error), are not thread-safe, or refuse loading off the main thread. A plugin that loads other plugins in-process (product B, D) is taken down by a crashing plugin: to be designed (e.g. out-of-process scanning/loading as DAWs do). Open question 4.
6. **Experiments and tests:** save results incrementally, several optimizer seeds, state the frequency band a test covers, separate knob-range limits from filter behavior, report what the data does not show, do not conclude from the first measurement of a new plugin.

## 6. Decisions
| Topic | Status |
|---|---|
| **Scope: product only (JUCE/C++); Python only as oracle, its further development is outside this repo** | **DECIDED** |
| Big 6: level/gain, frequency response, THD+N, noise/SNR, phase, crosstalk (+ IMD, aliasing, latency, dynamics, delay, null test, linearity as extensions) | **DECIDED** |
| Linux main development platform; Windows and macOS must be supported; CI on all three | **DECIDED** |
| Test objects: free VST3 EQs first (open source not required), own plugins from `~/AudioDev` included; VST2 only if needed; commercial plugins optional for case studies | **DECIDED** |
| Real-time listening required | **DECIDED** |
| Code GPLv3; teaching material CC BY-SA 4.0 | **DECIDED** |
| EQ first, then compressor, delay, reverb; mono/stereo first | **DECIDED** |
| "Equal": technical (frequency response within 0.1 dB, null depth below -60 dB, in a settings file/preset), perceptual models later | **DECIDED** |
| Test audio: free sample packs (MusicRadar SampleRadar) via download scripts, not stored in the repo | **DECIDED** |
| Plugin format: VST3 first | **DECIDED** (reference) |

## 7. Development rules (reference, binding for all code in this project)
- Step by step. For each new feature: design, implementation, function testing; then code review and documentation.
- Readable code: no one-liners, no magic numbers, no magic constants, no magic strings.
- Readability is more important than performance (but not too much).
- Versioning (author's global rule): `project(... VERSION X.Y.Z)` in CMake; a new feature raises the second number and sets the third to zero, a fix or other change raises the third; bumped once per logical change.

## 8. Work packages and milestones (proposal, each with a definition of done)
| # | Work package | Done when |
|---|---|---|
| W1 | **Project skeleton**: CMake + JUCE (submodule), CI on Linux/Windows/macOS, pluginval, test framework | Builds and runs an empty stand-alone app and a test target on all three OSes in CI. |
| W2 | **Stand-alone host, part 1**: plugin scan and list, load one plugin, parameter list, plugin GUI window, several plugins with separate windows | Works with the test plugin set; failures (crash on load) are reported, not fatal. |
| W3 | **Stand-alone host, part 2**: audio file list with loops, render through several plugins in parallel, gapless switching, latency measurement and compensation, per-plugin output files | Gapless switching without xruns; latency of each test plugin measured and compensated; files written. |
| W4 | **Delivery protocol + plugin fingerprint report** (section 5, items 1 and 2) | Fingerprint of all test plugins; behavior at 44.1/48/96 kHz and different block sizes recorded; quirks documented. |
| W5 | **Reference processors (RBJ, Orfanidis, Zölzer) + test signals** in C++ | Analytic ground truth available for the measurement tests. |
| W6 | **Measurement units** (frequency response, phase, group delay, latency, THD, THD+N, noise, SNR, crosstalk, null test with alignment), easy to add new ones | Each agrees with analytic ground truth and with the Python oracle within a stated tolerance; band of validity documented. |
| W7 | **Loader plugin** (scan standard paths, list, load one plugin, parameters, GUI) | Runs inside a DAW; pluginval passes. |
| W8 | **EQ band model + parameter mapping**, calibration of knobs by measurement | Mapping data validated for the test EQs. |
| W9 | **Matcher** (user chooses parameters, start point, frequency range, tolerance; real-time deviation display) | Master reproduced on each test plugin or the residual quantified and attributed. |
| W10 | **Final plugin**: 4 slots, parallel rendering, real-time transfer function display, three-level GUI, measurement presets | Persona walkthroughs succeed (musician compares a demo with an owned plugin; developer checks an algorithm). |
| W11 | **Further measurement units** (dynamics, delay, reverb) | Each with ground-truth tests. |
Later: perceptual metrics, multichannel/sidechain, VST2 support.

## 9. Open questions
1. Repository/package and CMake project name (proposal: `pluginlab`).
2. Reuse of the author's existing JUCE material: the JUCE checkout, `AdvancedAudioTemplate`, and the CMake/CI/pluginval setup of `stereo_widening` (not yet looked at for this plan).
3. Should the open PeakEQ question (frequency knob 1.0875 too high; 44.1 kHz design?) be a first test case for the fingerprint (W4)? Source code is available.
4. Crash isolation for the loader/final plugin (out-of-process loading?) and what the first version promises.
5. Which plugins form the fixed test set (13 free/own EQs in the prototype, `docs/prototype/plugin_shortlist.md`)? Air-G EaseQ still needs a manual download.
6. How to get the free plugins onto Windows/macOS CI runners.
7. VST2 support: the reference names an external project; suitability and licence are not checked.
8. How the C++ measurements are compared with the Python oracle in practice (shared test signals and reference result files?).
9. The empty last item of the feature list in the reference: what was intended?
