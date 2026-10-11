# Mappings of our own plugins

One JSON file per plugin (W8.5, `docs/eqmodel/mapping.md`): which parameter is which band quantity, its knob law as the plugin shows it and as
measured, what it needs to act, and the steps of its switches. Proposed by the parameter survey (`PLUGINLAB_WRITE_MAPPINGS=<folder>` with the survey
tests), checked against the plugins' code by the tests: the survey tests compare every new proposal with these files, `tests/MappingTests.cpp` drives
the plugins from them. Mappings of other plugins live in the user's settings folder (`PluginLabHost --map`), not here.
