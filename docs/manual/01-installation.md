# Jade Benchmark and PluginLabHost: installation

Two products from one code base (docs/design/W10-plan.md): **Jade Benchmark**, a plugin for your DAW, and **PluginLabHost**, a stand-alone
application that needs no DAW. Both have the same pages; the application also has the Developer page.

## The plugin: Jade Benchmark
Copy the plugin bundle into your plugin folder and let the DAW rescan its plugins:

| System | Format | Folder |
|---|---|---|
| Linux | `JadeBenchmark.vst3` | `~/.vst3/` |
| Windows | `JadeBenchmark.vst3` | `C:\Program Files\Common Files\VST3\` |
| macOS | `JadeBenchmark.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS | `JadeBenchmark.component` (AU) | `~/Library/Audio/Plug-Ins/Components/` |

Keep the bundle as it is: it carries two helper programs, `PluginLabScanner` (scans plugins one file at a time, so that a broken plugin
cannot take your DAW down) and `PluginLabHost` (measures and matches in a process of its own, for the same reason). On macOS
`PluginLabHost.app` is inside the bundle in `Contents/Helpers`.

Put Jade Benchmark as an effect on a track. Its first page, **Easy**, walks you through comparing your EQ with others (see
`02-first-steps.md`).

## The application: PluginLabHost
Unpack the archive anywhere and start `PluginLabHost` (Windows: `PluginLabHost.exe`, macOS: `PluginLabHost.app`). Keep `PluginLabScanner`
next to it (on macOS it is inside the app). The application plays audio files through the plugins with your audio device and has every page.

## Where the settings are kept
Both products share one settings folder (the application data folder of your user, `pluginlab/`): the mappings of plugins, the presets of
plugins and of measurements, the reports. A preset saved in the application is there for the plugin, too.

| System | Settings folder |
|---|---|
| Linux | `~/.config/pluginlab/` |
| Windows | `%APPDATA%\pluginlab\` |
| macOS | `~/Library/pluginlab/` |

## Unsigned programs (macOS, Windows)
The archives from the project's CI are not signed. macOS may refuse to open them ("cannot be verified"): open the app once with right-click,
Open, or remove the quarantine flag (`xattr -dr com.apple.quarantine <folder>`); Windows SmartScreen may ask once.

## Licence
Free software: the source code under the GNU GPL v3, the binaries under the AGPL v3 (JUCE); third-party parts in `THIRD_PARTY.md`.
