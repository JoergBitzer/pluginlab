# pluginlab

Measure, compare and match audio plugins, and learn why they differ. Mission: divide the myth from reality.

Status: W3 (loader plugin, VST2, validation) done; product only: JUCE/C++. Read [planning.md](planning.md). The vision comes from
[docs/reference/Measurement Tool Development plan.md](docs/reference/Measurement%20Tool%20Development%20plan.md).

The first prototype (Python + pedalboard, 13 free EQs, GUI) is kept as the learning vehicle:
repository [JoergBitzer/measurement_tool](https://github.com/JoergBitzer/measurement_tool), tag `prototype-0.6.0`.
Its results are copied as documents into [docs/prototype/](docs/prototype/).

Code: GPLv3. Teaching material: CC BY-SA 4.0.

## Build
```console
git clone --recursive <this repository>           # JUCE is a submodule (git submodule update --init)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
Targets: `PluginLabHost` (stand-alone application: scan, validate with pluginval, load, parameters, editor windows), `PluginLabScanner`
(scans one plugin file in its own process), `PluginLabLoader` (VST3 plugin that hosts one VST3 or VST2 plugin), `PluginLabTests`
(`juce::UnitTest`), test plugins in `tests/plugins`. VST2 uses the free FST headers (submodule `external/FST`, GPL): `git submodule update --init`.
pluginval is found via `PLUGINLAB_PLUGINVAL`, next to the program, in the PATH or in `~/.cache/pluginval` (`tools/run_pluginval.sh` downloads it).
Details for developers (and for Claude Code): [CLAUDE.md](CLAUDE.md); design notes: [docs/design/](docs/design/).
JUCE is used under the AGPLv3: binaries are under the AGPLv3, the source code of this project under the GPLv3.
