# W10: the final products (plan for review)

Status: **draft v1, for the author's review** (2026-10-18). Nothing here is decided. Milestone of `planning.md` §8: "**Final products: the
stand-alone application and the plugin** (two main targets since 2026-10-12): 4 slots, parallel rendering, real-time transfer function
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
2. **Pages in the plugin**: all levels including the Developer page, or easy and expert only (the Developer page's jobs start the
   application as a child process)? Proposal: all, the Developer page's jobs through the application binary.
3. **Live display rate**: 5 updates per second per changed slot (proposal), faster, or on release of the knob only?
4. **The easy page**: is the walkthrough of 3.4 what the musician should see, and is medium (0.25 dB) the right default tolerance there?
   Proposal: yes, with "the same / audibly close / different" in words.
5. **Gain trim after a match**: should the product level-match the slots for listening automatically (proposal), or only show the level
   difference?
6. **Measurement presets**: JSON in the settings folder, shared by both products (proposal)?
7. **Crash protection**: option (c) of 3.7 (in process for listening, child processes for all measurements)? Proposal: yes.
8. **First release**: which OSes (proposal: Linux and Windows first, macOS when signing and the sandbox questions are solved) and which
   formats of the plugin product (proposal: VST3; VST2 where the FST headers allow; AU later)?
9. **Distribution**: GPLv3 source, binaries from CI as archives (proposal), or installers? Under the name "pluginlab" or "Jade Benchmark"?
10. **DAWs to test** beyond Bitwig (Linux): Reaper (Linux, Windows) and one on macOS? Proposal: Reaper on Linux and Windows for W10.2.
