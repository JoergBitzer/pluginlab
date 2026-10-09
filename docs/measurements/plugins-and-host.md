# Plugins and host: the units on hosted plugins

Unit of W7.11. Code: `src/engine/PluginDevice.cpp` (`makePluginDevice`, `getPluginChannels`, `getSetting`, `getDefaultSetting`),
`src/engine/PluginMeasurement.cpp` (`measureDevice`, `measurePlugin`, `createMeasurementReport`), the shared render `src/engine/PluginRig.h`
(moved out of the fingerprint), the host `apps/host/HostFingerprint.cpp` (`--measure`, the section in the fingerprint report); tests
`tests/PluginMeasurementTests.cpp`, CTest `PluginLabHostMeasure` and `PluginLabHostFingerprint`. Conventions: [README](README.md).

## Purpose
The units of W7.1 ... W7.10 measure a `Device`: "render this buffer at this sample rate from a fresh state". A hosted plugin is not such a function
by nature (planning §5 item 1: parameters lost before prepare, stale sample rates, smoothing, state). W7.11 turns a plugin and a setting into a device
that behaves like one, by the same careful delivery protocol that the fingerprint (W5) found reliable, and runs all units on it: in the test suite
(the reference plugins, answers known), from the command line and on the host's Developer page.

## The plugin device
Every render (every call of the device) does, for a plugin, a sample rate and a setting (the normalised values of all parameters):
1. a **fresh instance** (`createPluginInstance`), the main-bus layout chosen for two channels (`ChannelAdapter`, as the fingerprint);
2. `prepareToPlay` at the requested sample rate, block size 512;
3. every parameter first set to **another value** (0.4 away, "poke"), one block of noise processed, then the **setting** (a plugin that reacts only to
   changes cannot miss it);
4. **settling** in silence (0.25 s, `PluginDeviceOptions::settleSeconds`);
5. the input in blocks of 512; the output of all main-bus channels.

This is `Bench::render` of the fingerprint ("the careful way that all measurements use"), now in `PluginRig.h` and shared, so the fingerprint and the
units cannot drift apart. The output has the plugin's channels (`getPluginChannels`): a mono plugin gives one channel, and the units are run with that
many channels (planning §9 item 8: the channel a test signal goes through is chosen explicitly, never mixed silently).

A setting comes from an instance (`getSetting`: the values of its parameters, e.g. after setting them by their texts) or is the plugin's default
(`getDefaultSetting`).

## The measurement summary
`measureDevice` (any device, e.g. a reference processor) and `measurePlugin` (a plugin and a setting) run the units with their AES17 defaults at 48 kHz
on 1 or 2 channels and give one row per result (unit, quantity, value, text, clause):

| Unit (document) | Rows |
|---|---|
| gain ([level-and-gain.md](level-and-gain.md)) | gain at 997 Hz, -20 dBFS; gain matching (2 channels) |
| frequency response ([frequency-response.md](frequency-response.md)) | range of the response at the standard third octaves re 997 Hz (sweep) |
| delay and polarity ([delay-and-polarity.md](delay-and-polarity.md)) | impulse peak, phase delay at 100 Hz, polarity |
| phase ([phase-and-group-delay.md](phase-and-group-delay.md)) | deviation from linear phase in the passband |
| THD and THD+N ([thd-and-thdn.md](thd-and-thdn.md)) | THD+N and THD at -1 and -20 dBFS |
| intermodulation ([intermodulation.md](intermodulation.md)) | difference-frequency and modulation distortion at 0 dBFS |
| noise ([noise.md](noise.md)) | idle channel noise (CCIR-RMS and A), dynamic range (CCIR-RMS and A), mains products |
| crosstalk ([crosstalk.md](crosstalk.md)) | worst selective crosstalk (2 channels) |
| maximum level and linearity ([maximum-level-and-linearity.md](maximum-level-and-linearity.md)) | maximum input level with overload/rollover; largest gain non-linearity |

The null test (W7.10) needs two devices and is not part of the summary of one plugin; it is used by the Compare page and the matcher (W9).

`createMeasurementReport` writes the rows as a Markdown section "## Measurements (AES17)". Exact zeros (a perfect plugin) show as "silent" instead of a
number at the float floor.

## Where it shows
- **Developer page** (host): the report made by "Generate report" (the fingerprint, in a child process) now ends with the measurement section, for the
  plugin's default setting. The fingerprint settings file has a switch `"measurements": true` to leave it out.
- **Command line**: `PluginLabHost --measure <plugin file> <report file> [<plugin identifier>]` writes only the measurement section (default setting).
- **Code and tests**: `makePluginDevice` with any setting, for any unit.

## Band of validity and limits
- The default setting is what the report measures; an EQ at its defaults is often flat. The units with a chosen setting need the setting in normalised
  values (by code today; by the band model and mapping of W8 later).
- Every render makes a fresh instance: a plugin that loads slowly makes the summary slow (the reference EQ: 7.3 s without the linearity unit; the test
  gain plugin with all units: 11 s).
- Time-varying plugins (modulation, auto-gain) are measured as they are; the units assume time-invariance (the fingerprint says whether a plugin is).
- Latency: the units measure it (W7.3) and their windows start after settling; a plugin that reports latency is not compensated by the device (the
  delay is part of what is measured).

## Results for known test signals
From `PluginMeasurementTests` (2026-10-09, 0.36.0), 48 kHz, the reference plugins (VST3, built with the tests) through the plugin device:

| Plugin and setting | Unit | Expected (pluginlab_reference) | Measured through the host |
|---|---|---|---|
| Reference EQ: RBJ peak 1 kHz +6 dB Q 2 | swept response at the third octaves, both channels | $\lvert H(e^{j\omega})\rvert$ | within 0.00005 dB |
| the same | selective gain at 997 Hz; gain matching | 20 lg $\lvert H(997)\rvert$; 0 dB | within 0.001 dB; 0.000 dB |
| Reference Utility: -6 dB, inverted, Thiran 37.5 samples | phase delay at 100 Hz; polarity; gain | 37.5000 samples; inverting; -6 dB | 37.5000; inverting; -6.0000 dB |
| Reference Nonlinear: $x + 0.1x^2 + 0.05x^3$ | harmonics 2, 3 and THD at -1 dBFS | closed form, THD -27.065 dB | within 0.01 dB, THD -27.065 dB |
| Reference EQ (as above), the whole summary | range of the response re 997 Hz; maximum input level | range of $\lvert H\rvert$ at the third octaves; none (linear) | equal within 0.01 dB; none up to +24 dBFS |

The summary of the Reference EQ (RBJ peak +6 dB, from the test log): gain +6.00 dB, response -6.00 ... +0.00 dB re 997 Hz, impulse peak 0 samples,
phase delay -2.707 samples at 100 Hz (a minimum-phase peak leads), deviation from linear phase +18.5/-20.1 degrees (as in W7.4), THD+N -152.7 dB and THD
-192.0 dB at -1 dBFS (float rounding), idle noise silent, dynamic range 204 dB CCIR-RMS (float), crosstalk silent, no maximum input level.

The command line on the test gain plugin (CTest `PluginLabHostMeasure`): 0 dB gain, flat, no delay, THD+N -153.7 dB, idle noise silent, no maximum
input level, gain linearity within 0.000 dB down to -140 dBFS; 11 s.

Tolerances in the test: as in the units' own tests (response 0.01 dB, gain 0.001 dB, phase delay 0.001 samples, harmonics 0.01 dB).

**What the results teach**
- A plugin measured through a careful host gives the same numbers as the algorithm in the library: what differs in a real plugin is the plugin, not
  the measurement.
- A float plugin without a deliberate non-linearity has no maximum input level and a "dynamic range" of 200 dB: the AES17 numbers of converters say
  little about such a plugin; its deviations show in the frequency response, the phase and the null test.

## Implementation
```cpp
std::vector<float> setting = pluginlab::engine::getSetting(*instance);          // or getDefaultSetting(formats, description, 48000.0)
pluginlab::measure::Device device = pluginlab::engine::makePluginDevice(formats, description, setting);
pluginlab::measure::FrequencyResponse response = pluginlab::measure::measureSweptResponse(device, {});
pluginlab::engine::MeasurementSummary summary = pluginlab::engine::measurePlugin(formats, description, setting);   // all units
juce::String markdown = pluginlab::engine::createMeasurementReport(summary);
```
Command line: `PluginLabHost --measure <plugin file> <report file> [<plugin identifier>]`.
