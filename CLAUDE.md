# CLAUDE.md -- instructions for Claude Code in the pluginlab project

Based on the CLAUDE.md of the AdvancedAudioTemplate (branch AAT2, `~/AudioDev/AdvancedAudioTemplate`), adapted to this project.
The plan, the decisions and the work packages are in `planning.md`; the lessons of the first prototype are in
`docs/prototype/LESSONS_LEARNED.md`. Read `planning.md` first.

## The project
pluginlab (mission: divide the myth from reality) measures, compares and matches audio plugins; the product is JUCE/C++
(Python is only the oracle for the measurements, in the separate repository `measurement_tool`).
- `src/core/`: static library `pluginlab_core` (no GUI code): plugin scanning (`PluginScanner`, one scanner process per file), `HostedPlugin`, `PluginValidator`
  (pluginval in a child process, cached), `LoaderState`, VST2/VST3 formats. `src/ui/`: `pluginlab_hosting_ui` (browser, parameter table, editor factory), used by host and loader.
- `src/signals/`: `pluginlab_signals` (test signals: sine, two-tone, multitone, noise, bursts, the synchronized swept sine after Novak et al. with its
  analytic inverse, the stepped sine; WAV export). `src/reference/`: `pluginlab_reference` (linear reference filters with their exact H(e^jw):
  RBJ, Orfanidis, Zoelzer, state-variable, Butterworth/LR, linear-phase FIR, delays; analog prototypes; W6.3: waveshapers and quantizer with known
  harmonics/SNR, gain, channel matrix, DC, hum, noise adder, tremolo; all derive from `Processor`). `src/engine/`: audio engine and the fingerprint.
- `src/measure/`: `pluginlab_measure`, the measurement units of W7 after AES17-2015 (`Device`, analyzer, units); one document per unit in
  `docs/measurements/` (purpose, standard, routine, analysis, validity, results for known test signals). Plan `docs/design/W7-plan.md`.
- `cmake/Vst2Sdk.cmake` + `vst2_shim/` + `external/FST`: the free VST2 headers (GPL) with five patches; read the comments before touching them.
- `apps/host/`: stand-alone host application `PluginLabHost` (GUI; command line: `--write-version <file>`, `--report <folder> <file>`,
  `--scan <folder> --load <plugin name>`). `apps/scanner/`: `PluginLabScanner`, scans one plugin file in its own process
  (crash isolation); it is copied next to the host after the build.
- `apps/signals/`: `PluginLabSignals <folder> [--seconds 5,10] [--rate 48000]` writes the standard test signals (stereo, 32-bit float WAV)
  with `signals.txt` (what every file is) and the step lists of the stepped sines (CSV). The author keeps a set in `~/Music/TestSignals/`.
- `apps/teaching/`: `PluginLabTeachingFigures <folder>` writes the SVG figures of the teaching pages (`docs/teaching/references/`, CC BY-SA)
  from `pluginlab_reference`; regenerate them after a change of a reference: `PluginLabTeachingFigures docs/teaching/references/figures`.
- `tests/oracle/`: oracle files of the Python prototype (`measurement_tool/tools/export_oracle.py`), compared by `tests/OracleTests.cpp`.
- `plugins/loader/`: `pluginlab_loader_core` (LoaderProcessor + LoaderEditor, also used by the tests) and the plugin `PluginLabLoader` (VST3).
- `plugins/reference/`: the three reference plugins (PluginLab Reference EQ / Nonlinear / Utility), AdvancedAudioTemplate instances in the
  simple form around `pluginlab_reference`; see `plugins/reference/README.md` (template origin, changes to the template code, controls).
  Built into `<build>/reference_plugins`; a Release build installs them into `~/.vst3`.
- `tests/`: `PluginLabTests` (`juce::UnitTest` console app, registered with CTest), `CheckHostVersion.cmake`, `CheckHostReport.cmake`;
  `tests/plugins/`: test plugins with known behavior (Gain: 4 parameters, state, VST3 + VST2; Crash: crashes when created; Crash Process: crashes while processing), built into `<build>/test_plugins`.
- `tools/`: `run_pluginval.sh` / `.ps1`.
- `docs/`: `design/` (one design note per work package), `prototype/` (evidence from the prototype), `reference/`.
- `CMakeLists.txt`: the version of the whole project (`project(pluginlab VERSION x.y.z)`).

## Build and test
```console
git submodule update --init                       # JUCE (pinned submodule), once
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
tools/ctest_offscreen.sh                          # CTest in Xvfb + openbox (no windows on the desktop)
tools/run_pluginval.sh build/plugins/loader/PluginLabLoader_artefacts/Debug/VST3/PluginLabLoader.vst3 1
```
Develop with the Debug build (JUCE assertions are on). **Run the tests offscreen** (the author's rule: no test windows on the desktop):
`tools/ctest_offscreen.sh` runs CTest in Xvfb with openbox (hosted VST2 editors need a window manager; without one they die with an X "BadAtom"
error). Compared on 2026-10-08: offscreen and desktop give the same results. `PluginLabTests --only <part of a test name>` runs single tests.
CI (`.github/workflows/ci.yml`) builds, tests and runs pluginval on Windows and macOS, only for a version tag (see "CI and tags" below).

## Workflow rules (from the author)
- Development rules of `planning.md` section 7 apply: step by step; for each new feature design (a note in `docs/design/`),
  implementation, function testing, then code review and documentation.
- Versioning (`project(pluginlab VERSION X.Y.Z)` in the top-level CMakeLists.txt), once per logical change that gets committed:
  a new feature raises the second number and sets the third to 0 (1.0.3 -> 1.1.0); a fix or other change raises the third
  (1.1.0 -> 1.1.1). Documentation-only changes need no new version.
- Release tags (`vX.Y.Z`) only on the user's explicit request.
- **CI and tags** (as in JadeSpectrogram2, decided by the author 2026-10-06): pushing is fine at any time and starts no CI. CI (Windows and
  macOS) starts only when a tag `vX.Y.Z` is pushed whose version equals `project(pluginlab VERSION X.Y.Z)` in the top-level CMakeLists.txt;
  a tag that does not match stops the run in the first job, before anything is built. The "Run workflow" button on the Actions page starts it
  by hand (no tag check). To test a version on Windows/macOS: bump the version, commit, `git tag vX.Y.Z`, `git push origin main vX.Y.Z`
  (the tag only when the user asks for it), then watch the run with `gh run list` / `gh run watch`.
- Update the documentation in the same change as the code.
- Report results honestly: failed tests, skipped steps, things not tested (e.g. Windows/macOS).
- The user works directly on `main` for now; commit per logical step, push when asked or when a work package is finished.

### Not active yet (the author: for feature extensions of a working product; switch on later)
- A new git branch per request (descriptive name, e.g. `feature/...`, `fix/...`); merge into main only when the user says so.
- Run pluginval before every commit (`tools/run_pluginval.*`, all runs SUCCESS, zero JUCE assertions).
(CI runs pluginval for every version tag regardless.)

## Conventions (code style of the author, binding)
- Readable code: no one-liners, no magic numbers, no magic constants, no magic strings. Readability is more important than
  performance (but not too much).
- No ternary operators; no `if`/`else` inside expressions; no `goto`; no `using namespace` in headers.
- Constants with `constexpr`, not `#define`; no `#define` macros (inline functions or templates); JUCE's own macros are the exception.
- Headers include as little as possible: forward declare, include only what is used, no includes in headers just to pass them on.
- Member variables `m_...`, constants `kName`; algorithm and core code free of GUI code (no `juce_gui_*` includes in `src/core`).
- GUI text: only Latin-1 characters (Windows fonts lack more).

- CI installs pluginval before the tests and sets `PLUGINLAB_REQUIRE_PLUGINVAL=1` (the validator tests must not be skipped). Locally they are skipped without pluginval.
- `.github/workflows/vst2-debug.yml` runs the scanner under a debugger (lldb/cdb/gdb) on a runner: use it to get a stack of a crash that only happens on Windows or macOS.

## Traps
- Never change anything in `JUCE/` (submodule).
- Plugins are not functions "parameters in, audio out": parameters can be lost, applied with a stale sample rate or delayed (see
  `docs/prototype/LESSONS_LEARNED.md`). Do not draw conclusions from the first measurement of a plugin.
- Keep audio buffers alive for as long as a loaded plugin might use their pointers (VST2 plugins may keep them).
- Tests that start the app or a plugin must not touch the user's real settings: use a temporary home folder
  (`HOME=$(mktemp -d) ...`; `run_pluginval.sh` does this itself).
- `Versioning.h` is generated into the build folder; do not commit it.
