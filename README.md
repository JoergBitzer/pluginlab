# pluginlab

Measure, compare and match audio plugins, and learn why they differ. Mission: divide the myth from reality.

Status: W1 (project skeleton) in progress; product only: JUCE/C++. Read [planning.md](planning.md). The vision comes from
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
Targets: `PluginLabHost` (stand-alone application), `PluginLabLoader` (VST3), `PluginLabTests` (`juce::UnitTest`).
Details for developers (and for Claude Code): [CLAUDE.md](CLAUDE.md); design notes: [docs/design/](docs/design/).
JUCE is used under the AGPLv3: binaries are under the AGPLv3, the source code of this project under the GPLv3.
