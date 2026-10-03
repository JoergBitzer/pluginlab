# W2 design: stand-alone host, part 1 (plugin-hosting core, first version)

Goal (planning.md, W2): scan and list plugins, load one plugin (VST3), parameter list, plugin GUI window, several plugins with
separate windows. Done when it works with the test plugin set and a plugin that fails to load is reported, not fatal.

## What W2 does and does not do
In: VST3 scanning of the standard folders and extra folders; list with status; loading; parameter list with live values;
editor windows (several plugins at once). Out (later): VST2 and pluginval validation (W3), audio processing and gapless
switching (W4), the delivery protocol and fingerprint (W5). Plugins are loaded and their editors work, but no audio flows.

## Crash isolation of the scan (decision for W2)
Scanning a plugin loads its binary, and some plugins crash on load (LSP Plugins: bus error, found in the prototype). So **every
file is scanned in its own child process**: `PluginLabScanner <plugin file> <result file>` (a small console program that uses
the JUCE VST3 format to read the plugin descriptions and writes them as XML). The host waits at most 30 s, then kills the
scanner. A scan result is `Ok`, `NoPluginInFile`, `Crashed` (the scanner died or wrote no result), `TimedOut` or
`ScannerFailed` (scanner could not be started). The host stays alive in all cases.
Loading a plugin for use is done in-process (editor windows need it); plugins that were scanned `Ok` can still crash while
being used: that is the open question 4 of planning.md (pluginval check, out-of-process hosting), not solved in W2.

## Modules
```
src/core (static library pluginlab_core, juce_audio_processors_headless, no GUI code)
  hosting/PluginScanResult.h     ScanStatus enum + PluginScanResult (file, status, message, descriptions)
  hosting/PluginScanner.h/.cpp   findPluginFiles(search path), scanFile() in a child process, scanFileInProcess() (used by the scanner)
  hosting/PluginScanXml.h/.cpp   write/read a PluginScanResult as XML (the scanner writes, the host reads)
  hosting/HostedPlugin.h/.cpp    one loaded plugin: load() with error text, parameter list, set parameter
apps/scanner                     PluginLabScanner (console app)
apps/host                        PluginLabHost (GUI): plugin list, load/unload, parameter table, editor windows;
                                 command line modes for CI: --write-version, --report <folder> <file>
tests/plugins                    in-tree test plugins (VST3): "PluginLab Test Gain" (4 parameters, known behavior) and
                                 "PluginLab Test Crash" (crashes when it is created); built into <build>/test_plugins
tests                            ScannerTests, HostedPluginTests (juce::UnitTest); CTest PluginLabHostReport (end to end)
```
The format objects are passed in (`juce::AudioPluginFormat&`, `juce::AudioPluginFormatManager&`): the app registers the VST3
format with GUI support (`juce::VST3PluginFormat`, needed for editors), the scanner and the tests the headless one.

## Interfaces
- `PluginScanResult scanFile(file)` (class `PluginScanner`, constructed with the path of the scanner executable and a timeout).
- `std::unique_ptr<HostedPlugin> HostedPlugin::load(formatManager, description, sampleRate, blockSize, errorMessage)`;
  `getParameters()` returns `ParameterInfo` (index, name, label, value text, normalised value, default, number of steps,
  discrete/boolean/automatable); `setParameterNormalised(index, value)`.
- A failed load returns `nullptr` and a non-empty error text.

## GUI (apps/host)
Main window: a table of the scan results (name, manufacturer, format, status, file), buttons *Scan standard folders*,
*Add folder...*, *Load*, *Unload*; a table of the loaded plugins; a table of the parameters of the selected loaded plugin (name,
value text, slider). Scanning runs in a background thread (the GUI stays responsive); results arrive on the message thread.
Each loaded plugin gets its own window with its editor (a generic editor if the plugin has none); closing the window does not
unload the plugin. The status line reports failures (not found, crashed, load error).

## Test plan
- ScannerTests (needs the test plugins and the scanner executable, passed by CMake): good plugin -> Ok with the description
  "PluginLab Test Gain"; crash plugin -> Crashed; a file that is not a plugin -> NoPluginInFile; missing scanner executable ->
  ScannerFailed; a folder with all of them: the good plugin is still found after the crash.
- HostedPluginTests: load the good plugin -> 4 parameters with the expected names; set Gain to 0.75 normalised and read
  "12.0 dB"; a description of a file that does not exist -> nullptr and an error text.
- CTest PluginLabHostReport: `PluginLabHost --report <test_plugins> <file>` scans, loads and lists; the file contains the good
  plugin with 4 parameters, the crash plugin as Crashed and the garbage file as NoPluginInFile.
- pluginval on the loader plugin (CI, unchanged). Manual: start the GUI, scan, load Test Gain and a plugin of the machine,
  open two windows.

## Findings during the implementation (2026-10-03)
- **The scanner must be next to the host.** The host looks for `PluginLabScanner` in the folder of its own executable (on macOS
  inside the app bundle). Both are built into different folders, so a post-build step copies the scanner next to the host.
  A missing scanner was first reported as `Crashed`: on POSIX a program that does not exist "starts" (the forked child fails
  with exit code 255), so the scanner is checked for existence before it is started (`ScannerFailed`).
- **A plugin with a manifest is not scanned by running it.** JUCE 9 reads the description of a VST3 from `moduleinfo.json` and
  never runs the plugin code. To get a plugin that crashes during the scan, the crash test plugin is built without manifest
  (`VST3_AUTO_MANIFEST FALSE`). In practice this means: the scan only finds plugins that crash in their *factory*, a plugin
  with a manifest can still crash when it is loaded for use.
- **The VST3 wrapper adds a bypass parameter** (in the plugin and in the host's parameter list) unless the plugin names its own
  bypass parameter (`getBypassParameter()`); the test plugin does, so it has exactly four parameters.
- **VST3 has no boolean parameters:** a switch arrives in the host as a discrete parameter with two steps, `isBoolean()` is
  false. `ParameterInfo::isBoolean` is therefore also true for a discrete parameter with two values.
- **Developer options of the GUI:** `PluginLabHost --scan <folder> --load <plugin name>` scans a folder at startup and loads the
  plugin when the scan has finished (used for the screenshot of the working window; the window itself is not tested
  automatically).
- Not tested automatically: clicking in the window (sliders, buttons, editor windows). Tested by hand on Linux: scan list with
  Ok/Crashed/NoPluginInFile, load, 4 parameters with values and sliders, editor window. Windows and macOS: the report test
  (end to end without a window) runs in CI.
