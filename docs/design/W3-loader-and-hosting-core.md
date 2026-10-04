# W3 design: loader plugin and the plugin-hosting core (VST3 + VST2, validation)

Goal (planning.md, W3): the plugin-hosting core is consolidated and shared by the stand-alone host and the loader plugin; VST2
loading; pluginval validation of every plugin before it is loaded. Done when: the loader plugin runs inside a DAW and passes
pluginval; VST3 and VST2 test plugins load in both the stand-alone host and the loader plugin; a crashing plugin is caught by the
validation step.

Decision of the author (2026-10-03): the AdvancedAudioTemplate (AAT) is not used for the loader plugin; reusable pieces are
taken over as copies, nothing depends on it.

## Steps (each with tests and a commit)
- **W3a VST2.** The FST headers (GPL v3 or later, submodule `external/FST`) with the adapter and the three patches found with
  SimplePeakEQ (see planning.md section 10) become `cmake/Vst2Sdk.cmake` + `vst2_shim/`. With them JUCE's VST2 *hosting*
  (`JUCE_PLUGINHOST_VST=1`, `VSTPluginFormat(Headless)`) and the VST2 *plugin wrapper* (test plugins as VST2) build. The scanner
  and `HostedPlugin` handle both formats. Risk: the host side of VST2 uses more opcodes than the plugin side; FST does not know
  all of them (placeholders), so some VST2 plugins may behave incompletely. Tested with the in-tree VST2 test plugin and with
  SimplePeakEQ's VST2 on the development machine.
- **W3b Validation.** `PluginValidator` (core) runs pluginval on a plugin file in a child process and returns
  `Passed / Failed / TimedOut / NotAvailable / Crashed` with the output; results are cached per file (path, size, modification
  time, strictness) in a user cache file. Findings so far: pluginval 1.0.4 validates VST3 *and* VST2 on Linux. Policy in the
  UIs: only plugins that passed validation can be loaded, unless the user allows unvalidated plugins. The pluginval executable is
  searched in: environment variable `PLUGINLAB_PLUGINVAL`, next to the host executable, `PATH`, `~/.cache/pluginval`. CI installs
  it. A third test plugin crashes when it processes audio (it scans and loads, pluginval finds it): "a crashing plugin is caught".
- **W3c Shared UI and the loader plugin.** The plugin browser (list, status, validation, load) and the parameter table move from
  the host into a shared GUI library `pluginlab_hosting_ui` (juce_gui_basics, juce_audio_processors) that the host window and the
  loader plugin's editor both use. The loader plugin hosts one plugin: the editor shows the browser, the parameters and the
  hosted plugin's own editor; audio passes through the hosted plugin (simple: prepared with the host's sample rate and block
  size; no latency compensation, no state saving yet: W10). Checked by pluginval (CI) and by loading the test plugins in the
  loader's editor on this machine.

## Interfaces (new)
- `pluginlab::hosting::createFormatManager(bool withGui)`: VST3 and VST2 formats (headless or with GUI support).
- `ScanStatus`, `PluginScanResult`: unchanged; scanning covers both formats (`PluginScanner::findPluginFiles` over all formats).
- `ValidationResult validateWithPluginval(file, strictness, timeout)` and `ValidationCache`.
- `pluginlab::ui::PluginBrowserComponent` (list + status + validation + callbacks `onPluginChosen`), `ParameterTableComponent`.

## Test plan
- W3a: ScannerTests and HostedPluginTests run for VST3 and VST2 versions of the Gain plugin; the report test lists both; the
  crash plugin stays crashing in both formats. SimplePeakEQ VST2 loads (manual, Linux).
- W3b: validator: Gain passes, the crash-on-process plugin fails (not a crash of the host), a non-plugin file fails; cache hit
  returns without running pluginval (counted); missing pluginval -> `NotAvailable`.
- W3c: pluginval on the loader (CI); the loader's editor is opened with the test plugin loaded (screenshot, by hand on Linux).

## Findings of W3a (VST2 with FST)
- JUCE's VST2 *hosting* code needs two more patches/additions than the plugin wrapper: the structure tag `AEffect` (JUCE forward-declares
  `struct AEffect`; patch 4 in `cmake/Vst2Sdk.cmake`) and two names (`VstSmpteFrameRate`, `kPlugCategMaxCount`, in `vst2_shim/aeffectx.h`).
- Real-world check on Linux (host `--report` over the author's `~/.vst`, with a temporary home folder): 7 VST2 files with 12 plugins
  (u-he Diva, Zebra2, Zebralette, ZRev, Zebrify, Repro-1, Repro-5, ACE, ZebraHZ, OB-Xd, ToneZ, SimplePeakEQ) are scanned and
  loaded; parameter names and display texts are correct (e.g. `Tune = 0.0 Cents`). ToneZ_V2 reports 0 parameters. Not tried: audio
  processing and editors of these plugins (W4), Windows and macOS (CI only covers the test plugins).
- VST2 differs from VST3 in what the host sees: the name is the file name (`name`), the plugin's own name is in `descriptiveName`
  (`hosting::getDisplayName`); parameter display text is only available through `getCurrentValueAsText()` (`getText(value)` gives
  the normalised number); there is no step information (every parameter looks continuous: a switch is not `isBoolean`).
- On POSIX a child process killed by a signal reports exit code 0; "no result file" is the reliable sign of a crash.
- The VST2 format also "finds" the shared libraries inside VST3 bundles (`Contents/x86_64-linux/X.so`); the scanner ignores files
  inside `.vst3` and `.component` folders when searching for VST2 plugins.

## Findings of W3a, second part: why the VST2 hosting crashed on Windows and macOS (found with the debug workflow)
- First CI run: on Windows (exit code 0xc0000409) and macOS (SIGABRT) the host crashed when it created a VST2 instance; Linux was fine.
  A debug workflow (`.github/workflows/vst2-debug.yml`: scanner under lldb/cdb) gave the stack on macOS:
  `__stack_chk_fail` in `juce::VSTPluginInstanceHeadless::queryBusIO` (JUCE's VST2 host code), i.e. a stack buffer overrun that
  the stack protector detected.
- Cause: JUCE's VST2 plugin wrapper writes one byte more than the label arrays hold into the `VstPinProperties` structure that the
  host passes (`copyToUTF8(properties.shortLabel, kVstMaxShortLabelLen + 1)`), relying on the 48 bytes of padding that the official
  structure has behind `shortLabel`. FST's structure has no padding. **Only a host built with FST's structure is affected** (our
  host: it allocates the structure on its stack); hosts built with the official SDK have the padding, so a plugin built with FST
  (SimplePeakEQ) is not endangered in them. (An earlier remark that Cubase would be hit by this was wrong.) Linux survived by luck of
  the stack layout. Patch 5 in `cmake/Vst2Sdk.cmake` adds the padding.
- The Windows run under the debugger (cdb) and the plain RelWithDebInfo run did not crash: stack-protector failures depend on the
  build configuration, so "works under a debugger" is no proof.

## Findings and decisions of W3b and W3c
- **Validation** (`PluginValidator`): pluginval runs in a child process (`--strictness-level 5 --timeout-ms ... --validate <file>`); on Linux and
  macOS with a temporary `HOME` (`env HOME=...`), like `tools/run_pluginval.sh`. "Passed" needs exit code 0 *and* the line `SUCCESS` in the output
  (a child killed by a signal reports exit code 0 on POSIX). pluginval 1.0.4 validates VST3 and VST2 plugins; level 5 takes about 2 s for the test
  plugin. The result is remembered per plugin file (path + size + modification time of every file in a bundle, strictness level, pluginval file),
  in `<application data>/pluginlab/validation_cache.xml`.
- **Policy in the UIs** (`PluginBrowserComponent`): a plugin is loaded only after it passed; validation runs when the user presses Load (not for
  the whole list: it can take a minute per plugin); a plugin that failed or timed out is never loaded; if pluginval is missing or "Load without
  validation" is ticked, a not yet validated plugin can be loaded. Test plugins: the one that crashes while processing fails the validation
  without taking the host down, a file that is not a plugin fails.
- **Loader plugin** (`plugins/loader`): `LoaderProcessor` hosts one plugin and passes the audio through it (the audio thread only try-locks: while the
  plugin is replaced the audio passes through unchanged); its state is the description of the hosted plugin plus the hosted plugin's state
  (`hosting/LoaderState.h`); the editor is the browser, the parameter table and the editor of the hosted plugin (a generic one if there is none).
  The processor and the editor are the static library `pluginlab_loader_core`: the plugin links it, and the tests use `LoaderProcessor` directly.
  Reason: a JUCE host hands a hosted VST3 plugin only states in the plugin wrapper's own format, so the loader's state cannot be injected through a
  hosted wrapper in a test. The wrapper itself is covered by pluginval.
- **Shared UI** (`src/ui`, `pluginlab_hosting_ui`): `PluginBrowserComponent`, `ParameterTableComponent`, `TextTableModel`, `createEditorFor`,
  `addGuiFormats`; the host window (`apps/host`) is built from them.
- **Not done / limits:** the loader runs effects only, no latency compensation, no sample-accurate parameter handling, no MIDI; restoring a session state
  loads the stored plugin without a new validation; the loader and the host were tried with the test plugins and on Linux with a screenshot, not in a
  real DAW (Bitwig Studio 5.2.7 is installed on the development machine, not used yet); Cubase and Windows/macOS DAWs not tried; VST2 on Windows and
  macOS only with the test plugins (CI), on Linux also with the author's third-party VST2 plugins (scan and load).

## Bug found by the author in Bitwig (2026-10-04): "scanner failed, something is missing" (fixed in 0.3.1)
- Symptom: the loader (VST3 copied to `~/.vst3`) loaded in Bitwig (its log shows "Engine loaded plug-in"), but every entry in the loader's own plugin list
  was "ScannerFailed".
- Cause 1: the scanner is looked for next to the module that contains the code; inside a plugin that is the plugin binary in the bundle, and the build never
  put `PluginLabScanner` there. Cause 2: the file name was built with the extension of the running module (`.so`, `.vst3`) instead of the extension of an
  executable (none, or `.exe`); the same mistake was in the pluginval lookup.
- Fix: the build copies the scanner into the loader bundle (`PluginLabLoaderBundle`, JUCE's own copy step is off) and then copies the finished bundle to
  `<build>/loader_plugin/PluginLabLoader.vst3` (the place to install it from); `PluginScanner::getScannerFileName()` gives the right name; the environment
  variable `PLUGINLAB_SCANNER` overrides the path (development). Tests: file name of the scanner, CTest `PluginLabLoaderBundleHasScanner`.
- Checked end to end on Linux: the loader running inside the pluginlab host scans with its own bundled scanner (developer variable
  `PLUGINLAB_LOADER_SCAN=<folder>` makes the loader's editor scan a folder when it opens). Why the tests did not catch it: they run `LoaderProcessor` inside the
  test program, where the scanner sits next to the program; only a real host runs the code from inside the bundle.

## Second Bitwig test (2026-10-04): crash, refused plugin, editor in a corner (changes in 0.4.0)
- **TOMiC "Tonic" (drum synth) crashed Bitwig's engine** (exit code 139). The plugin loads and renders in the pluginlab host, so the cause is the combination
  with the loader: an instrument (no audio input) was forced into a 2-in/2-out layout, the parameter table asked all parameters of a 2.7 MB-state plugin five
  times per second, and `getStateInformation` ran without the lock. Changes: instruments and plugins that do not accept the loader's channel layout are refused
  with a message (effects only for now); `getStateInformation` takes the lock; the parameter table asks only the visible rows (`HostedPlugin::getNumParameters`,
  `getParameter(index)`). The crash itself is not proven fixed: TOMiC stays on the list of plugins for the robustness harness (see below).
- **u-he Repro-1 (VST3) "not loaded"**: the validation gate refused it without a reason. pluginval passes level 1 and dies (segmentation fault) from level 2
  on. The validator now reports where pluginval died ("crashed or hung during '<test>'"), the browser has a strictness level (1-10, default 5), and
  "Load without validation" now also overrides a failed validation (the plugin is trusted by the user, the risk is theirs).
- **Editor in the lower-right corner**: most editors do not fit into a part of the loader window. The editor of the hosted plugin now opens in a window of its own
  (`ui::PluginEditorWindow`, shared with the host; resizable if the plugin's editor is); buttons "Show plugin editor" and "Unload plugin" in the loader.
- Lesson: only a real DAW shows these problems; the user's own plugin collection is the best robustness test set. A local harness driven by the environment
  variable `PLUGINLAB_REALWORLD_PLUGINS` is still to be written (not committed with third-party plugins).
