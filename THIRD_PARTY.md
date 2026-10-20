# Third-party software in pluginlab and Jade Benchmark

The source code of pluginlab is under the GNU General Public License v3 (LICENSE); the teaching material in `docs/teaching/` under
CC BY-SA 4.0. The programs and plugins built from it contain the following software of others. Their licence texts are in the source tree
at the paths given.

| Software | Used for | Licence | Licence text |
|---|---|---|---|
| JUCE 9 (Raw Material Software) | the framework of all programs and plugins | AGPLv3 (open-source option of JUCE's licence): the binaries are under the AGPLv3 | `JUCE/LICENSE.md` |
| VST3 SDK (Steinberg Media Technologies), as bundled with JUCE | hosting VST3 plugins, the VST3 format of Jade Benchmark and the loader | MIT | `JUCE/modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt` |
| Audio Unit SDK (Apple), as bundled with JUCE | the AU format of Jade Benchmark (macOS) | Apache 2.0 | `JUCE/modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt` |
| FST - Free Studio Technology (IOhannes m zmölnig and contributors; fork pierreguillot/FTS) | hosting VST2 plugins (reverse-engineered headers, five patches: `cmake/Vst2Sdk.cmake`) | GPLv3 or later | `external/FST/LICENSE` |
| zlib, libpng and other libraries inside JUCE's modules | file formats, compression | their licences (zlib, libpng and others) | in the folders of `JUCE/modules/` |
| pluginval (Tracktion) | validation of plugins; not built into the programs, downloaded by `tools/run_pluginval.*` and started as a separate program | GPLv3 | https://github.com/Tracktion/pluginval |

"VST" is a trademark of Steinberg Media Technologies GmbH; "Audio Unit" and "AU" are trademarks of Apple Inc. Jade Benchmark and pluginlab
are not affiliated with them.
