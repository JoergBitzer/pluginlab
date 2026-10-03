# W1 design: project skeleton

Goal (planning.md, W1): an empty stand-alone app, an empty loader plugin and a test target build and run on Linux, Windows and
macOS in CI; pluginval runs in CI; `CLAUDE.md` is in place; the version lives in CMake.

## Layout
```
CMakeLists.txt            top level: project(pluginlab VERSION x.y.z), JUCE (git submodule JUCE/), options, subdirectories
JUCE/                     JUCE 9.0.3 (submodule, never edited)
src/core/                 static library pluginlab_core: no GUI code; the later plugin-hosting core grows here (W2/W3)
apps/host/                stand-alone host application PluginLabHost (JUCE GUI app); W1: empty window with the version
plugins/loader/           loader plugin PluginLabLoader (VST3); W1: pass-through effect, the base for W3
tests/                    PluginLabTests: console app that runs all juce::UnitTest classes, registered with CTest
tools/                    run_pluginval.sh / .ps1 (pluginval, Linux/macOS and Windows)
docs/                     planning, design notes, prototype evidence
.github/workflows/ci.yml  build, test, pluginval on the three systems
```

## Decisions
- **Test framework: JUCE `juce::UnitTest`** in a console app (`juce_add_console_app`). No extra dependency, same toolchain on
  all systems, JUCE types usable in tests. CTest runs the console app; its exit code is the result.
- **Version:** `project(pluginlab VERSION 0.1.0)` in the top-level CMakeLists.txt is the only place; `src/core/Versioning.h.in`
  generates `Versioning.h` (numbers); `pluginlab::getVersionString()` returns "major.minor.patch" for the GUI and for
  `PluginLabHost --version`.
- **Smoke test without a display:** `PluginLabHost --version` prints the version and quits, so CI can run the app on a runner
  without GUI session.
- **Loader plugin in W1:** a pass-through VST3 effect (stereo and mono), `hasEditor` with a generic editor, so pluginval and the
  CI plugin steps work from the start. It is *not* the AAT2 template (decision: reuse of AAT2 is discussed later); it contains
  only what a plugin needs.
- **pluginval:** CI runs `tools/run_pluginval.*` on the built loader VST3 at strictness level 10 on every system (a CI check; the
  "run pluginval before every commit" rule of the AAT2 CLAUDE.md stays inactive, see CLAUDE.md).
- **Core library without GUI modules:** `pluginlab_core` links `juce_core` only for now; modules are added when a work package
  needs them. The app and the plugin link the GUI modules themselves.
- **Code style:** the rules of planning.md section 7 (no magic numbers/strings, no ternaries, `constexpr`, minimal includes, ...).

## Test plan
- `tests/VersionTests.cpp`: the version string equals the numbers of `getVersion()`, all numbers non-negative.
- CTest runs `PluginLabTests`; CI runs it on all three systems.
- `PluginLabHost --version` output equals the version string of CMake (checked in CI).
- pluginval on `PluginLabLoader.vst3`.
