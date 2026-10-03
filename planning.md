# pluginlab - planning (fresh start)

Status: draft v2 (2026-10-03). Second attempt. The first prototype (repo `measurement_tool`, tag `prototype-0.6.0`) was a
learning vehicle; its results are in [docs/prototype/](docs/prototype/). The vision, personas, feature list, development path
and rules come from the author's own document [docs/reference/Measurement Tool Development plan.md](docs/reference/Measurement%20Tool%20Development%20plan.md),
which is the **reference for this plan**. Nothing is decided until marked **DECIDED**. No code has been written yet.

## 1. Mission and final product
**Mission: divide the myth from reality** (what plugins really do, versus what their marketing and their knobs say).

**Final product (from the reference):** a measurement VST plugin that loads up to 4 other plugins and has measurement units for
different plugin types (EQ, dynamics, delay, reverb). It has a pool of test signals, can also render audio from the DAW through all
plugins in parallel, and switches between the algorithms (delay, gain etc. equalized inside). It has an automatic parameter matcher;
the user can open the GUI and choose which parameters to compare or use as starting points for the optimization.

**Personas**
- *Simple musician with some technical interest*: reads about a new plugin, downloads the demo, wants to compare it with plugins already owned. => easy to use.
- *Plugin developer*: develops an algorithm, wants to check everything and compare with competitors. => "something like pluginval, but for the processing, not just the specification."

**Teaching is a main goal** (unchanged): show that different tools do the same thing when adjusted correctly, and teach a method for finding out *why* they differ (the "audio detective"). Real-time listening so that students hear the differences.

## 2. Feature list of the final product (reference, condensed)
- Load up to 4 plugins in parallel, show their GUIs in separate windows.
- Render audio from the DAW, from files or from test signals through all plugins in parallel; output can be saved or listened to; gapless switching between plugins in real time.
- Measurements, easy to add new ones: frequency response, phase, group delay, THD, THD+N, noise, SNR, crosstalk, null test with alignment (and the big 6 and extensions of section 5).
- Real-time display of the transfer function (continuously updated with a click or a short sweep) while the user adjusts parameters.
- Automatic parameter matcher (e.g. free EQ to a commercial EQ): user or tool chooses the start point; user chooses which parameters to match or ignore, the frequency range and the tolerance; real-time display of the deviation.
- GUI with easy page, expert page, developer page; user chooses which measurements to show; presets for the measurements.
(The reference leaves the last list item empty.)

## 3. Development path (reference, with the parts that depend on the framework question)
The project is huge and is divided into parts that can be developed and optimized independently.

**A. Measurement part (Python first).** Python implementations of standard algorithms (RBJ, Orfanidis, Zölzer) for a Python-only test of the measurement methods: frequency/phase response, group delay (and latency for linear phase), THD, THD+N, noise, SNR, crosstalk, null test with alignment.

**B. Stand-alone host (JUCE, C++, Windows/Mac/Linux, VST3 only; VST2 later via the project the reference names).** Pre-test to learn how to load plugins and render audio. Required abilities (reference list): scan for plugins and list them; load one plugin and show its parameters; show its GUI in a window; load several plugins with separate GUI windows; load audio from disk and render through the plugins (save or listen); render through multiple plugins in parallel; switch between plugins gaplessly without stopping the audio engine; measure latency and gain of each plugin and compensate; save each plugin's output to a separate file; simple GUI. It can become a stand-alone product without a DAW; audio files in an easily switchable list, each with loop positions that can be set in the GUI.

**C. Simple VST plugin that loads other plugins:** scan the standard installation paths, list the plugins, load one, show its parameters and GUI.

**D. The final measurement plugin** that combines A-C, the matcher and the three-level GUI.

## 4. Framework question (revisited; needs your decision)
The earlier planning decided "Python + pedalboard, no own host". The reference asks for a JUCE host and a JUCE plugin as the product. These are compatible as **two tracks**:
- **Track Lab (Python + pedalboard):** where measurements, algorithms, the matching and the teaching case studies are developed and validated quickly (reference part A). Also the quickest way to explore real plugins (fingerprints, quirks).
- **Track Product (JUCE/C++):** host (B), loader plugin (C), final plugin (D). Measurements are ported from the validated Python versions; the Python reference stays as the test oracle.
Alternative: JUCE only (no Python lab); or Python only (no product). **Open question 1.**

## 5. What the prototype taught and what changes
Details and evidence: [docs/prototype/LESSONS_LEARNED.md](docs/prototype/LESSONS_LEARNED.md), [docs/prototype/findings.md](docs/prototype/findings.md).
1. **Plugins are not functions "parameters in, audio out".** Parameters set before the first process call can be lost, applied with a stale sample rate (44.1 kHz), delivered only after a later change, smoothed, or poison the filter state (NaN). Block-size and latency effects came from the host. => The host layer has an explicit *parameter delivery protocol* (prime, deliver, settle, render, flush) and a *self-test per plugin* (target, target, alternative, target must give A, A, B != A, A; most conservative strategy first). A repeatable result is not a correct result. This applies to the JUCE host as well: a real host calls `prepareToPlay` and delivers parameters in the order a DAW does; the host must be tested against these plugins.
2. **Plugin fingerprint** as the first deliverable per plugin: parameters and ranges, reported and measured latency, delivery strategy, determinism, behavior at 44.1/48/96 kHz, block-size independence, reachable knob ranges.
3. **Ground truth before real plugins:** reference processors (RBJ, Orfanidis, Zölzer designs) and analytic tests for every measurement.
4. **A general EQ band model** (type, frequency, gain, Q, slope, phase behavior) separated from per-plugin mapping files; a tool that dumps all parameters and proposes a mapping. The "one peaking band" model of the prototype was too narrow.
5. **Architecture rules that worked:** everything that loads a plugin runs in a worker process in the Lab track (some plugins crash, are not thread-safe, or refuse non-main-thread loading); device-independent audio engine; configuration in YAML. A host plugin that loads other plugins in-process (Track Product) has to survive crashing plugins: to be designed.
6. **Experiments:** save results incrementally, several fit seeds, state the band a test covers, separate knob-range limits from filter behavior, report what the data does not show. Do not conclude from the first measurement of a new plugin.

## 6. Decisions carried over from the prototype planning
| Topic | Status |
|---|---|
| Big 6: level/gain, frequency response, THD+N, noise/SNR, phase, crosstalk (+ IMD, aliasing, latency, dynamics, delay, null test, linearity as extensions) | **DECIDED** |
| Linux main development platform; Windows and macOS for students; CI on all three | **DECIDED** |
| Free VST3 EQs first (open source not required), own plugins from `~/AudioDev` included; VST2 only if needed; commercial plugins optional for case studies | **DECIDED** |
| Real-time listening required; offline rendering + small GUI first | **DECIDED** |
| Code GPLv3; teaching material CC BY-SA 4.0 | **DECIDED** |
| EQ first, then compressor, then delay; mono/stereo first | **DECIDED** |
| "Equal": technical (frequency response within 0.1 dB, null depth below -60 dB, in a settings file), perceptual models later | **DECIDED** |
| Test audio: MusicRadar SampleRadar free samples via download scripts | **DECIDED** |
| GUI toolkit: PySide6 for the Python lab | **DECIDED** for Track Lab; the product GUI is JUCE (follows from the reference) |
| Framework: Python + pedalboard only | **superseded by section 4, to be confirmed** |

## 7. Development rules (reference, binding for all code in this project)
- Step by step. For each new feature: design, implementation, function testing; then code review and documentation.
- Readable code: no one-liners, no magic numbers, no magic constants, no magic strings.
- Readability is more important than performance (but not too much).
(Versioning follows the author's global rule: new feature raises the second number, fix the third; bumped once per logical change.)

## 8. Work packages and milestones (proposal, each with a definition of done)
| # | Track | Work package | Done when |
|---|---|---|---|
| W1 | Lab | **Reference processors + stimuli + measurement set** (RBJ/Orfanidis/Zölzer EQ designs; sweep response, phase, group delay, level, THD, THD+N, noise, SNR, crosstalk, null test with alignment) | Every measurement agrees with analytic ground truth within a stated tolerance; band of validity documented. |
| W2 | Lab | **Pedalboard host + fingerprint** (delivery protocol, self-test, fingerprint report for the test plugins) | Fingerprint of all test plugins; delivery strategy per plugin reproducible; documented failure cases. |
| W3 | Lab | **EQ band model + mapping tool**, calibration | Mapping files generated/validated; shelves, filters, multi-band covered. |
| W4 | Lab | **Matching** (descriptors, optimizer, seeds, residual explanation; user-chosen parameters, range, tolerance as in the reference) | Master reproduced on each test plugin or residual quantified and attributed. |
| W5 | Lab | **Compare + detective** (alignment, null test, linear/nonlinear separation, checklist of causes) and case-study library | Documented diagnosis paths (cramping, wrong sample rate, doubled gain, ...). |
| W6 | Product | **JUCE stand-alone host** (list in section 3B) | All abilities of the reference list work on Linux first; gapless switching without xruns; latency measured and compensated. |
| W7 | Product | **JUCE loader plugin** (section 3C) | Scans, lists, loads one plugin, shows parameters and GUI inside a DAW. |
| W8 | Product | **Measurement units in C++** (ported from W1, tested against the Python oracle) | Same results as the Python oracle within tolerance. |
| W9 | Product | **Final plugin**: 4 slots, parallel rendering, real-time transfer-function display, matcher, easy/expert/developer pages | Persona walkthroughs succeed (musician compares a demo against an owned plugin; developer checks an algorithm). |
| W10 | Both | **Teaching material** (notebooks, exercises) | Notebooks run on all three OSes with the free plugin set. |
| W11 | Both | **CI per OS with plugins installed** | Plugin tests run on Linux, Windows, macOS. |
Later: compressor, delay, reverb measurement units; perceptual metrics; multichannel/sidechain; VST2 support in the host.

## 9. Open questions
1. **Framework:** two tracks (section 4), JUCE only, or Python only? Order of W1-W5 (Lab) versus W6-W7 (Product): start with both in parallel or Lab first?
2. Repository/package name (proposal: `pluginlab`).
3. Should the open PeakEQ question (frequency knob 1.0875 too high; 44.1 kHz design?) be the first test case for the fingerprint? (Source code is available.)
4. Which plugins form the fixed test set (13 free/own EQs in the prototype, `docs/prototype/plugin_shortlist.md`)? Air-G EaseQ still needs a manual download.
5. How to get the free plugins onto Windows/macOS CI runners and onto students' machines.
6. VST2 support: the reference names an external project for it; its suitability and licence are not checked.
7. The empty last item of the feature list in the reference: what was intended?
