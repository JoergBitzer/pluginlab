# CLAUDE.md -- instructions for Claude Code in the pluginlab project

Based on the CLAUDE.md of the AdvancedAudioTemplate (branch AAT2, `~/AudioDev/AdvancedAudioTemplate`), adapted to this project.
The plan, the decisions and the work packages are in `planning.md`; the lessons of the first prototype are in
`docs/prototype/LESSONS_LEARNED.md`. Read `planning.md` first.

## The project
pluginlab (mission: divide the myth from reality) measures, compares and matches audio plugins; the product is JUCE/C++
(Python is only the oracle for the measurements, in the separate repository `measurement_tool`).
- `src/core/`: static library `pluginlab_core` (no GUI code); the plugin-hosting core grows here.
- `apps/host/`: stand-alone host application `PluginLabHost` (GUI; command line: `--write-version <file>`, `--report <folder> <file>`,
  `--scan <folder> --load <plugin name>`). `apps/scanner/`: `PluginLabScanner`, scans one plugin file in its own process
  (crash isolation); it is copied next to the host after the build.
- `plugins/loader/`: the loader plugin `PluginLabLoader` (VST3).
- `tests/`: `PluginLabTests` (`juce::UnitTest` console app, registered with CTest), `CheckHostVersion.cmake`, `CheckHostReport.cmake`;
  `tests/plugins/`: test plugins with known behavior (Gain: 4 parameters; Crash: crashes when created), built into `<build>/test_plugins`.
- `tools/`: `run_pluginval.sh` / `.ps1`.
- `docs/`: `design/` (one design note per work package), `prototype/` (evidence from the prototype), `reference/`.
- `CMakeLists.txt`: the version of the whole project (`project(pluginlab VERSION x.y.z)`).

## Build and test
```console
git submodule update --init                       # JUCE (pinned submodule), once
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build -C Debug --output-on-failure
tools/run_pluginval.sh build/plugins/loader/PluginLabLoader_artefacts/Debug/VST3/PluginLabLoader.vst3 1
```
Develop with the Debug build (JUCE assertions are on). On Linux the tests that start the GUI app need a display (`xvfb-run -a`
on a machine without one). CI (`.github/workflows/ci.yml`) builds, tests and runs pluginval on Linux, Windows and macOS.

## Workflow rules (from the author)
- Development rules of `planning.md` section 7 apply: step by step; for each new feature design (a note in `docs/design/`),
  implementation, function testing, then code review and documentation.
- Versioning (`project(pluginlab VERSION X.Y.Z)` in the top-level CMakeLists.txt), once per logical change that gets committed:
  a new feature raises the second number and sets the third to 0 (1.0.3 -> 1.1.0); a fix or other change raises the third
  (1.1.0 -> 1.1.1). Documentation-only changes need no new version.
- Release tags (`vX.Y.Z`) only on the user's explicit request.
- Update the documentation in the same change as the code.
- Report results honestly: failed tests, skipped steps, things not tested (e.g. Windows/macOS).
- The user works directly on `main` for now; commit per logical step, push when asked or when a work package is finished.

### Not active yet (the author: for feature extensions of a working product; switch on later)
- A new git branch per request (descriptive name, e.g. `feature/...`, `fix/...`); merge into main only when the user says so.
- Run pluginval before every commit (`tools/run_pluginval.*`, all runs SUCCESS, zero JUCE assertions).
(CI runs pluginval on every push regardless.)

## Conventions (code style of the author, binding)
- Readable code: no one-liners, no magic numbers, no magic constants, no magic strings. Readability is more important than
  performance (but not too much).
- No ternary operators; no `if`/`else` inside expressions; no `goto`; no `using namespace` in headers.
- Constants with `constexpr`, not `#define`; no `#define` macros (inline functions or templates); JUCE's own macros are the exception.
- Headers include as little as possible: forward declare, include only what is used, no includes in headers just to pass them on.
- Member variables `m_...`, constants `kName`; algorithm and core code free of GUI code (no `juce_gui_*` includes in `src/core`).
- GUI text: only Latin-1 characters (Windows fonts lack more).

## Traps
- Never change anything in `JUCE/` (submodule).
- Plugins are not functions "parameters in, audio out": parameters can be lost, applied with a stale sample rate or delayed (see
  `docs/prototype/LESSONS_LEARNED.md`). Do not draw conclusions from the first measurement of a plugin.
- Keep audio buffers alive for as long as a loaded plugin might use their pointers (VST2 plugins may keep them).
- Tests that start the app or a plugin must not touch the user's real settings: use a temporary home folder
  (`HOME=$(mktemp -d) ...`; `run_pluginval.sh` does this itself).
- `Versioning.h` is generated into the build folder; do not commit it.
