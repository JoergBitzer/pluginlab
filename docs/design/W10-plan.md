# W10: the final products (plan)

Status: **plan v2** (v1 2026-10-18 for review; v2 the same day with the author's answers in section 7; W10.1 done; W10.2 code done; W10.3 done; W10.4 done; W10.5 done; W10.6 done; W10.7 done; W10.8 in progress). Milestone of
`planning.md` §8: "**Final products: the stand-alone application and the plugin** (two main targets since 2026-10-12): 4 slots, parallel rendering, real-time transfer function
display, three-level GUI, measurement presets, in both. Done when: persona walkthroughs succeed in the stand-alone application and in the
plugin (musician compares a demo with an owned plugin; developer checks an algorithm)." The questions for the author are in section 7.

## 1. What W10 has to answer
Two products from one code base:
- **The stand-alone application** (the host of W2/W4, grown into a product): plugins, audio files and test signals, measurements, the
  matcher, the reports; no DAW needed (the author: working without a DAW is the more convenient way to measure).
- **The measurement plugin** (the loader of W3, grown into a product): the same inside a DAW, with the DAW's audio as one more source.

Three GUI levels, for three readers:

| Level | Who | What they do |
|---|---|---|
| **Easy** | the musician (persona 1): owns one EQ, tries a demo | load up to 4 plugins, match them to the owned one, listen and switch, see one curve each and "how close" |
| **Expert** | the engineer, the teacher | everything measurable: frequency response, phase, group delay, THD, noise, null test, system class; presets of measurements; the matcher with all options; live transfer function while turning knobs |
| **Developer** | the plugin developer (persona 2) | the Developer page that exists: fingerprint, GUI review, survey and mapping, calibration and interactions, reports |

W10 does not add new measurement units (W11: dynamics, delay, reverb) and no perceptual metrics (later).

## 2. What already exists
| From | What | Used for |
|---|---|---|
| W2/W3 | plugin-hosting core: scanning in a process of its own, catalog, validation (pluginval), loading VST3/VST2, editor windows, parameter table | both products |
| W3 | loader plugin: hosts one effect inside a DAW, crash log | the base of the plugin product |
| W4 | engine: up to 4 slots rendering the same input in parallel, latency compensation, gapless switching, offline files; Compare page | the 4 slots, listening |
| W5/W5c/W5d | fingerprint, real-time behaviour, GUI review; the Developer page | the developer level |
| W6 | reference plugins, test signals | teaching, presets, tests |
| W7 | measurement units (AES17 set, response, phase, group delay, THD, noise, crosstalk, null test) | the expert level |
| W8 | system class, band model, survey, mapping (with confirmation), calibration, interactions | expert and developer levels; the matcher's start point |
| W9 | matcher: general layer, EQ domain, fast render path, plugin matching, attribution; Matcher page (W9.6, being built) | the easy and expert levels |
| W9.3 | fast render path (one live instance, impulse probe, ~40 ms per render in Debug) | the real-time transfer function display |

## 3. Design (proposals)
### 3.1 One code base, two thin shells
- A library of **pages** (`pluginlab_pages`, new): Plugins, Compare, Matcher, Measure (expert), Developer, each a component that gets the
  engine and the settings, and knows nothing about being in an application or a plugin.
- **The application shell** (`PluginLabHost` today): a window with the pages as tabs, the audio device, the file menu, the command line.
- **The plugin shell** (the loader today): an editor with the same pages; the audio comes from the DAW; no audio device of its own.
- What the shells provide to the pages: the engine, the settings folder, "is there a DAW stream", "may I start a child process", a place to
  open windows. Pages ask, they do not assume.
- The command-line modes (`--fingerprint`, `--map`, `--calibrate`, `--match`, ...) stay in the application only; the plugin starts the
  application in a child process for them (crash isolation, as the Developer page does already).

### 3.2 The 4 slots and listening
- Up to 4 plugins in slots, as the engine has them (W4): the same input, latency-compensated, gapless switching (A/B/C/D), per-slot gain
  trim after a match (so that switching compares sound, not level).
- Sources: audio files with loops (W4), test signals (W6), and in the plugin the DAW's input.
- **No audio while the matcher runs** (the author): the Matcher stops playback, matches, then offers "listen". The live display (3.3) does
  not use the audio path either.

### 3.3 The real-time transfer function display
- What: the magnitude (and optionally phase) of every slot, redrawn while the user turns a knob in a plugin's editor.
- How: the fast render path (W9.3) on a copy of each slot's parameter values: one live instance per slot for measuring (not the instance
  that plays), an impulse, the spectrum; about 40 ms per render in Debug, less in Release.
- Rate: proposal 5 updates per second per slot, only for the slots whose parameters changed since the last update (a knob that does not
  move costs nothing).
- Shown: the curves of all slots in their colours on one log axis, the master's curve thicker, the deviation from the master below
  (largest and rms in numbers).
- Limits: valid for LTI plugins only (the system class decides; a non-LTI slot is drawn grey with the reason); ringing longer than the
  window (W9.3) is cut.

### 3.4 The three levels
**Easy page** (the musician's walkthrough, in this order):
1. "My EQ": pick the owned plugin, set it as it sounds good (its editor).
2. "Compare with": up to three plugins.
3. "Make them the same": one button; the matcher with the defaults (the released parameters proposed from the mappings: frequency, gain,
   Q of the bands that act; tolerance medium); a progress bar.
4. "Listen": play a file, switch A/B/C/D; per slot "how close" (largest deviation in dB, in words: "the same", "audibly close", "different")
   and, when not the same, the main cause in words (W9.5).
5. Save the matched settings as presets of the plugins.

**Expert page**: the measurement units on a chosen slot or all slots (response, phase, group delay, THD and THD+N, noise, null test against
the master), the system class first; measurement presets (3.5); the matcher with every option (released parameters and ranges, tolerance,
start point); the live display; Markdown reports like the Developer page's, with tables and an explaining footer.

**Developer page**: as it is (fingerprint, GUI review, survey and mapping with confirmation, calibration and interactions), plus the match
report of W9.7.

### 3.5 Measurement presets
- A preset: which units, their settings (levels, frequency range, sweep length, tolerance), which slots, the source; a name and a note.
- JSON in the user's settings folder (`presets/`), like the mappings; shipped presets for the teaching cases ("Orfanidis against RBJ",
  "Is it LTI?", "Null test of two EQs").
- The same presets in the application and the plugin.

### 3.6 The plugin inside a DAW: what differs
| Topic | Stand-alone application | Plugin |
|---|---|---|
| audio | own device, files, test signals | the DAW's stream (and files, test signals offline) |
| rendering for measurements | offline, any time | offline in the background on copies, never on the audio thread |
| threading | message thread and own render threads | the DAW calls the audio thread; measurements on a worker, results to the message thread |
| crash of a hosted plugin | the child processes protect the jobs; a crash while playing ends the application | a crash ends the DAW (the worst case) |
| sample rate, block size | chosen | the DAW's; the measurements state them |
| state | settings folder, session files | the DAW project (the slots, their states, the page) plus the settings folder |
| child processes | yes | yes for the jobs (the application binary), if the DAW's sandbox allows it (macOS?) |

### 3.7 Crash protection (decision needed)
`planning.md` §9 item 5 is still open and matters most in the plugin. Options:
- (a) in process, with validation before loading (pluginval, W3) and the crash log; simple, a crash ends the host or the DAW;
- (b) every hosted plugin in a process of its own, audio through shared memory; robust, costs latency and a GUI bridge (editors in another
  process are hard, on macOS especially);
- (c) as (a) for listening, (b) for all measurements and jobs (done already for the Developer page's jobs).
Proposal: (c) for W10; (b) only if real use shows crashes during listening.

## 4. Work packages
| Step | Content | Done when |
|---|---|---|
| **W10.1** | **Pages library**: Plugins, Compare, Matcher, Developer moved into `pluginlab_pages`; the application shell uses it; the interface a shell provides | The application works as before (all tests); the pages build without the application |
| **W10.2** | **Plugin shell**: the loader becomes the plugin product with the same pages and up to 4 slots; DAW input as a source; state in the project | The plugin runs in Bitwig (Linux) with 4 slots; the pages appear; pluginval passes; the project restores the slots |
| **W10.3** | **Real-time transfer function display**: a measuring instance per slot, update on parameter change, 5 per second, curves and deviation | Turning a knob of a test EQ updates its curve within 0.3 s; a non-LTI slot is greyed with its reason; no dropouts while playing |
| **W10.4** | **Easy page**: the musician's walkthrough (3.4), matched gain trim, "how close" in words, presets of the plugins | The musician walkthrough succeeds with the reference plugins and two test EQs in both products |
| **W10.5** | **Expert page**: measurement units on slots, system class first, null test against the master, the matcher with all options, reports | Every unit runs from the page on a test plugin and agrees with its CLI result; reports readable in the report window |
| **W10.6** | **Measurement presets**: format, storage, shipped teaching presets, loading in both products | A preset saved in the application loads in the plugin and gives the same results |
| **W10.7** | **Crash protection** as decided (3.7) | A crashing test plugin during a measurement leaves the product running in both shells; the behaviour while listening is documented |
| **W10.8** | **Release preparation**: CI on Linux, Windows, macOS for both products, installers or archives, the manual's first chapters (from `docs/teaching/`), licence texts | CI green on three OSes; both persona walkthroughs succeed on at least Linux and one more OS; the author's sign-off |

Order: W10.1 first (everything else builds on the pages); W10.2 and W10.3 next (the risks); then W10.4 ... W10.6; W10.7 as soon as decided;
W10.8 last. Each step: design note here, code, tests (offscreen in Xvfb), document, version, commit.

## 5. Risks
- **Editors inside a plugin's editor**: hosted plugins' editors in their own windows from inside a DAW (focus, z-order, HiDPI, Wayland);
  W3 showed it works in Bitwig on Linux, other DAWs untested.
- **The plugin in a DAW sandbox** (macOS, AU): child processes and file access may be restricted; the jobs may have to run in process there.
- **CPU**: 4 slots playing plus measuring instances for the live display; the display must back off when the audio thread is busy.
- **Thread safety of hosted plugins**: two instances of one plugin (playing and measuring) at the same time; plugins with global state may
  misbehave (the fingerprint's determinism test catches some).
- **Scope**: W10 is the largest package; the walkthroughs keep it focused.

## 6. Dependencies
W9.6 (Matcher page) and W9.7 (match report, teaching page) first: the easy page builds on them. W8's open items (the author's test objects,
a Nyquist-matched low-pass family) are not needed for W10. W11 (dynamics, delay, reverb) comes after W10 and plugs into the general matcher
and the expert page.

## 7. Questions for the author
1. **One code base, two thin shells** (3.1), the plugin as a shell around the same pages? Proposal: yes.
Answer yes
2. **Pages in the plugin**: all levels including the Developer page, or easy and expert only (the Developer page's jobs start the
   application as a child process)? Proposal: all, the Developer page's jobs through the application binary.
   Answer yes and we should keep the Developer page in the stand-alone application only, to avoid confusion for the musician.
3. **Live display rate**: 5 updates per second per changed slot (proposal), faster, or on release of the knob only?
Answer yes follow your proposal, 5 updates per second per changed slot.
4. **The easy page**: is the walkthrough of 3.4 what the musician should see, and is medium (0.25 dB) the right default tolerance there?
   Proposal: yes, with "the same / audibly close / different" in words.
   Answer yes
5. **Gain trim after a match**: should the product level-match the slots for listening automatically (proposal), or only show the level
   difference?
   Answer level match but with a parameter / slider, so that it is transparent to the user and can be manually adjusted if desired.
6. **Measurement presets**: JSON in the settings folder, shared by both products (proposal)?
   Answer yes, JSON in the settings folder, shared by both products.
7. **Crash protection**: option (c) of 3.7 (in process for listening, child processes for all measurements)? Proposal: yes.
   Answer yes
8. **First release**: which OSes (proposal: Linux and Windows first, macOS when signing and the sandbox questions are solved) and which
   formats of the plugin product (proposal: VST3; VST2 where the FST headers allow; AU later)?
   Answer, we should check MAc and au during development via CI Processing.
9. **Distribution**: GPLv3 source, binaries from CI as archives (proposal), or installers? Under the name "pluginlab" or "Jade Benchmark"?
Jade Benchmark and binaries from CI, as we do it with all plugins
10. **DAWs to test** beyond Bitwig (Linux): Reaper (Linux, Windows) and one on macOS? Proposal: Reaper on Linux and Windows for W10.2.
Yeah, macos I will ask someone to test it 

## Progress
### W10.1 pages library (0.62.1, done)
`src/pages` (library `pluginlab_pages`): the Plugins page (`HostMainComponent`), Compare, Matcher, Developer and the settings folder
(`HostSettings`), moved from `apps/host`; `PageServices` with what a shell provides: the program that runs the jobs in a process of its own
(the application itself by default; the plugin will set the PluginLabHost in its bundle) and the progress file of the jobs. The application
links the library and works as before (its tests); the library builds as a target of its own. The pages keep the namespace `pluginlab::host`
and the start options of the application for now (`StartupOptions`, used by the Plugins page for a scan folder and a plugin to load).


### W10.2 plugin shell (0.63.0, code done; the DAW checks open)
`plugins/benchmark`: the plugin **Jade Benchmark** (VST3, `JadeBenchmark.vst3`, copied to `build/benchmark_plugin`). The processor
(`BenchmarkProcessor`, library `pluginlab_benchmark_core`) owns a `MeasurementEngine` with the dry slot; the DAW's track is the source
(`MeasurementEngine::processExternal`: every slot processes the block, the audible one comes out, the crossfade as in the application; a
mono track feeds both inputs). The latency the DAW is told is the engine's (all slots aligned to the slowest one). Another sample rate or
larger blocks: the engine is prepared again and the slots are loaded again with their states. The state in the project: the slots with
their plugin descriptions and states, and the audible slot. The editor: the pages Plugins, Compare and Matcher as tabs (no Developer page);
Compare inside a DAW hides the file, play, device and render controls (`PageServices::isInsideDaw`). The bundle carries PluginLabScanner
and PluginLabHost next to the plugin binary; the Matcher's jobs run in that PluginLabHost.
Tests (`BenchmarkTests`): dry pass-through, a gain plugin in the audible slot and back to dry, the latency, the state round trip with the
plugin's setting, a foreign state ignored.
Open: the run in Bitwig and Reaper with 4 slots and pluginval (not installed here); the limit of 4 slots is not enforced yet; a match
report opens in the system's viewer (the report window is a Developer-page component); macOS: PluginLabHost as an app bundle inside the
plugin bundle is not handled yet (W10.7, CI).

### W10.3 real-time transfer function display (0.64.0, done)
`engine::LiveResponseMonitor` (`src/engine/LiveResponse.*`): per slot two measuring instances of the plugin (not the playing one), each
with a thread below the audio thread's priority. The page reads the knobs of the playing instances 20 times per second and hands them in;
the render thread renders a changed setting through the fast render path (W9.3: setting, settling, impulse, spectrum; 12 points per
octave, 20 Hz ... min(20 kHz, 0.45 fs)) at most 5 times per second, always the latest setting, nothing for a knob that does not move.
When the knobs rest for 1 s, the second instance checks the system class of that setting (W8.1) on its own thread, so the check never
delays a curve. The dry slot is flat without an instance; a slot whose plugin did not change keeps its instances.
`LiveResponseView` on the page Compare (both products), right of the parameter table: the curves of all slots on one log axis, the chosen
reference slot thicker, the deviation of each slot from the reference below and as numbers (largest and rms, frequencies where both curves
are below -60 dB left out); a slot that is not LTI at its setting is drawn grey with its class and reason; "system class being checked"
while it runs. It measures only while the page is visible; the toggle "Live response" removes the measuring instances.
Measured (Debug, Test EQ): knob to curve 41 ... 51 ms (the render about 41 ms); 20 settings in one second give 6 curves. Tests
(`LiveResponseTests`): the curve within 0.3 s and equal to the exact RBJ curve (0.01 dB), the rate limit, no render without a change, LTI
for the EQ and not LTI for Test EQ Drive with the drive on (with the reason), the instances kept, the grid at another rate, the view with a
knob of the playing instance (picture in the temp folder).
"No dropouts while playing" is by construction (other instances, lower priority, no lock shared with the audio thread) and not measured
by a test; to check by ear in Bitwig. The Matcher page keeps its own live view of one target (it shows the band curve as a master too).

### W10.4 easy page (0.66.0, done)
`EasyPanel` (`src/pages/EasyPanel.*`): the musician's walkthrough of 3.4 on one page, in both products (the plugin shows it first; the
application as its last tab, so that the tabs of the tests keep their places).
1. **My EQ**: a loaded plugin (the first by default), "Show its editor", "Load plugins ..." (the page Plugins).
2. **Compare with**: the other loaded plugins, at most three.
3. **Make them the same**: one button; the matcher with the defaults (the band parameters of the plugin's mapping with the model start; without a
   mapping every continuous, automatable knob that is not a bypass: the musician cannot map a plugin inside a DAW), tolerance medium, 15 s per
   plugin, in a process of its own (PluginLabHost --match; in the plugin the one in its bundle) with a progress bar; the found values are applied
   at once.
4. **Listen**: a row per plugin and one "without EQ"; the number button makes it the audible slot (the crossfade of the engine); how close in words
   from the largest difference: "the same" (<= 0.25 dB, the tolerance), "audibly close" (<= 1 dB, the just noticeable difference the author gave),
   "different", with the dB and the frequency; when not the same the main cause of the attribution (W9.5) in words; a refused plugin with its
   reason. A **level slider** per row (the author: "level match but with a parameter / slider, so that it is transparent to the user"): the match
   sets it to minus the matched plugin's mean level difference against mine; the user can move it. The application's Play button plays the files of
   the page EQ Compare; inside a DAW the DAW plays.
5. **Keep it**: the matched settings as presets of the plugins (settings folder, `presets/<plugin>/matched to <mine> <date>.state` and `.json`).

Shared with the Matcher page (`src/pages/MatchJob.*`): the job file, the child process (`MatchJob`), the result (`readMatchResult`: now also
the refusal reason, the main cause with its evidence, the frequency of the largest difference and the level difference), applying it, the
mapped parameters, the presets. The engine has a level trim per slot (`setSlotTrimDb`, ramped within a block: no click), for listening only
(the offline renders stay untrimmed); the plugin keeps the trims in the project.
Test (`EasyTests`): the walkthrough with my Test EQ (+6 dB peak) against the Reference EQ and the two-band Test EQ through the real child
process: both "the same" (0.02 and 0.00 dB) in 15 s, the values applied, the levels within the slider range, switching, two presets. The plugin's
editor shows Easy first (`BenchmarkTests`).
Open: the walkthrough by a person (the author) in both products; loading a saved preset back (W10.6 brings presets of measurements; plugin
presets are files for now).

### W10.5 expert page (0.67.0, done)
`ExpertPanel` (`src/pages/ExpertPanel.*`), the last tab in both products: the slots to measure (the plugins by default, the dry slot on demand),
the units (`engine::getMeasurementUnitKeys()`: level and gain, frequency response, delay and polarity, phase, THD and THD+N, intermodulation,
noise, crosstalk, maximum input level, gain linearity; the system class always first; the slow linearity off by default), the null test of the
other slots against a chosen master; "Measure" runs `PluginLabHost --expert <job> <report>` in a process of its own (the job runner of the
Matcher, now with a mode) with a progress bar per slot and unit; the results side by side (a column per slot, a row per quantity, the null depth
and its alignment as rows); "View report" opens the Markdown report (the report window in the application, the system's viewer in the plugin);
links to the Matcher (all options) and to EQ Compare (the live curves).
Engine: `MeasurementOptions::units` (the units to run) and `progress`; the summary as JSON (`createMeasurementJson`, `parseMeasurementJson`);
`--measure` writes `<report>.json` too.
Test (`ExpertTests`): every unit on the gain plugin from the page gives the same text as `--measure` (all rows equal), in 27 s; the gain plugin
against the dry slot nulls bit-identically ("identical"). The measurement presets (W10.6) will store the choices of this page.

### W10.6 measurement presets (0.68.0, done)
`MeasurementPresets` (`src/pages/MeasurementPresets.*`): a preset is the choice of the page Expert under a name with a note: the units, whether
the dry slot is measured, the null-test master **by role** ("none", "dry", "first plugin": a preset must work in another session with other
plugins in the slots). JSON (`"format": "pluginlab measurement preset"`, version 1) in the settings folder, `measurement_presets/`, shared by the
application and the plugin (the same settings folder). An unknown unit key (a newer version) is left out when reading.
Shipped teaching presets, written when missing (a changed one is never overwritten): "Is it LTI?" (only the system class), "Full AES17 set"
(every unit, the linearity included), "Null test of two EQs" (response, phase, the second against the first), "Orfanidis against RBJ"
(the Reference EQ twice, response, phase, null test; the note says how to set it up).
The page Expert: a preset menu (choosing applies it; the note is shown), "Save as preset ..." (name and note).
Test (`PresetTests`): the shipped presets are written; a preset survives its JSON; a preset saved on the page in the application loads on the
page in the plugin (the processor's engine, the same plugins) and gives the same results, row by row.
Plugin presets (the matched settings of the Easy page and the Matcher, `presets/<plugin>/`) stay files; loading them back is for later.

### W10.7 crash protection (0.69.0, done)
Option (c) of 3.7, decided by the author: **in process for listening, a process of its own for everything that measures.**

| What | Where it runs | A crashing plugin |
|---|---|---|
| scanning (both products) | PluginLabScanner, one process per file (W2) | the file is marked as crashing, the program goes on |
| Developer page jobs (fingerprint, mapping, calibration, GUI review) | PluginLabHost in a child process (W5, W8) | the job reports "no report (a crash?)" |
| Matcher, Easy ("make them the same"), Expert, measurement presets | PluginLabHost --match / --expert in a child process (`MatchJob`) | the job fails with a message, the page goes on |
| **live transfer functions** (EQ Compare, the Matcher's live view) | **new: the live worker**, PluginLabHost started by `juce::ChildProcessCoordinator` (`RemoteLiveResponse`, `src/pages/LiveWorker.*`) | the worker ends; the page marks the curves ("the measuring process ended (a plugin crashed?)"), starts the worker once more with the slots and the last settings; after a second crash the live curves stay off with the reason |
| listening (the slots of the engine: EQ Compare, Easy, the DAW's track in the plugin) | in process (the engine's slots) | **the application or the DAW ends**: a plugin that crashes while it plays cannot be caught; the start marker of the application tells what happened (at the next start loading the last session's plugins did not finish); the crash log of the loader plugin (W3) is not yet in Jade Benchmark (open) |

The live worker: the page sends JSON messages (setup with the rate and the options, the slots as plugin descriptions, the settings of the
playing instances when they change); the worker runs `engine::LiveResponseMonitor` (the measuring copies, the 5 renders per second, the
system class) and sends each new curve back (`engine::toJson(LiveSlotResponse)`); the page computes the frequency grid itself
(`getLiveResponseFrequencies`); the dry slot is flat without the worker. In the plugin the worker is the PluginLabHost in its bundle (as the jobs).
Tests (`CrashProtectionTests`): the Test EQ's curve from the worker equals the exact curve; the crash-in-process plugin ends the worker, it is
started once more and then stays off with the reason; an `--expert` and a `--match` job with that plugin fail with a message; the test program
(the application, the DAW) runs on. The live view's test now runs through the worker too.
Option (b) (listening in another process, audio through shared memory) stays for later, if real use shows crashes while listening.

### W10.8 release preparation (0.70.0, in progress)
Done here:
- **pluginval** 2.0.0-beta3 (strictness 10) on Jade Benchmark on Linux: passes, no JUCE assertions (the open point of W10.2).
- **CI** (`.github/workflows/ci.yml`): Linux (new; tests in Xvfb), Windows and macOS; pluginval for the loader and for Jade Benchmark on all
  three; **AU** of Jade Benchmark on macOS (`FORMATS VST3 AU`) validated with `auval -v aufx Jdbm IHAJ`; the archives of both products as
  artifacts; for a version tag a **draft** GitHub release with them (the author publishes it after the walkthroughs).
- **macOS bundle**: PluginLabHost is an application bundle there; Jade Benchmark carries it whole in `Contents/Helpers/PluginLabHost.app`
  (VST3 and AU), `getBundledHelper` finds it; the scanner sits next to the plugin binary in `Contents/MacOS` (not tested here: CI).
- **Archives** (`tools/package.sh`): `JadeBenchmark-<version>-<platform>.zip` (the bundles) and `PluginLabHost-<version>-<platform>.zip` (the
  application with its scanner), each with LICENSE, THIRD_PARTY.md and INSTALL.md (`docs/manual/01-installation.md`). Tested on Linux.
- **Licences**: `THIRD_PARTY.md` (JUCE AGPLv3, the VST3 SDK MIT, the AU SDK Apache 2.0, FST GPLv3, pluginval as a separate program).
- **Manual**, first chapters (`docs/manual/`): installation, first steps (the Easy walkthrough), the pages.
Open: the first CI run (needs a tag from the author); the code-review pass of the code conventions (CLAUDE.md: no ternary operators, which
the code of W10 and some older code use); splitting PluginLabTests further (about 580 s in Debug); the persona walkthroughs by the author on
Linux and one more system; the author's sign-off.
